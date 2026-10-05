#!/usr/bin/env python3
"""ROM-free regression for setupCctv using CCTVRecord.lookpad."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = ROOT / "vendor/ge-decomp/src/game/prop.c"
HARNESS = ROOT / "getv/port/tests/game/cctv_lookpad_harness.c"


def extract_function(source: str, signature: str) -> str:
    search_from = 0
    start = -1
    opening = -1

    while True:
        candidate = source.find(signature, search_from)

        if candidate < 0:
            break

        after = candidate + len(signature)

        while after < len(source) and source[after].isspace():
            after += 1

        if after < len(source) and source[after] == "{":
            start = candidate
            opening = after
            break

        search_from = candidate + len(signature)

    if start < 0:
        raise ValueError(f"production function definition not found: {signature}")

    depth = 0
    state = "code"
    index = opening

    while index < len(source):
        char = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""

        if state == "code":
            if char == "/" and following == "*":
                state = "block_comment"
                index += 2
                continue

            if char == "/" and following == "/":
                state = "line_comment"
                index += 2
                continue

            if char == '"':
                state = "string"
            elif char == "'":
                state = "character"
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1

                if depth == 0:
                    return source[start:index + 1]

        elif state == "block_comment":
            if char == "*" and following == "/":
                state = "code"
                index += 2
                continue

        elif state == "line_comment":
            if char == "\n":
                state = "code"

        elif state in ("string", "character"):
            if char == "\\":
                index += 2
                continue

            if ((state == "string" and char == '"')
                    or (state == "character" and char == "'")):
                state = "code"

        index += 1

    raise ValueError(f"closing brace not found: {signature}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument(
        "--expect",
        choices=("fixed", "vulnerable"),
        default="fixed",
        help="expected production behavior for positive or unchanged-main negative control",
    )
    args = parser.parse_args()

    compiler = shutil.which(str(args.cc))

    if compiler is None:
        raise SystemExit(f"compiler not found: {args.cc}")

    if not args.source.is_file():
        raise SystemExit(f"production source not found: {args.source}")

    if not HARNESS.is_file():
        raise SystemExit(f"ROM-free harness not found: {HARNESS}")

    source = args.source.read_text()
    function = extract_function(
        source,
        "void setupCctv(s32 arg0, CCTVRecord *arg1, s32 cmdindex)",
    )

    with tempfile.TemporaryDirectory(prefix="ge-cctv-lookpad-") as temporary:
        directory = Path(temporary)

        (directory / "cctv_production.inc").write_text(function + "\n")

        executable = directory / (
            "test_cctv_lookpad.exe" if os.name == "nt"
            else "test_cctv_lookpad"
        )

        command = [
            compiler,
            "-std=c11",
            "-O1",
            "-g",
            "-fno-strict-aliasing",
            "-Werror=return-type",
            "-DGE_PORT_NATIVE",
            f"-I{directory}",
            str(HARNESS),
            "-o",
            str(executable),
            "-lm",
        ]

        compiled = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=60,
        )

        if compiled.returncode:
            print(compiled.stdout + compiled.stderr, end="")
            return compiled.returncode

        result = subprocess.run(
            [str(executable)],
            capture_output=True,
            text=True,
            timeout=10,
        )

        output = result.stdout + result.stderr
        print(output, end="")

        if args.expect == "fixed":
            if result.returncode != 0:
                print("ERROR: fixed production source failed the CCTV regression")
                return 1

            if "6 checks, 0 failures" not in output:
                print("ERROR: fixed run did not execute the required 6 checks cleanly")
                return 1

            return 0

        if result.returncode == 0:
            print("ERROR: vulnerable negative control unexpectedly passed")
            return 1

        if "6 checks, 4 failures" not in output:
            print("ERROR: vulnerable control did not reproduce the expected 4 failures")
            return 1

        print("EXPECTED: unchanged production source reproduced all 4 CCTV failures")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
