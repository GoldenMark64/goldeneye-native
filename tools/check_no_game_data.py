#!/usr/bin/env python3
"""Reject ROMs, saves and other non-public game artifacts before publication."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCAN_BYTES = 8 * 1024 * 1024

ROM_MAGICS = {
    b"\x80\x37\x12\x40": "big-endian N64 ROM header",
    b"\x37\x80\x40\x12": "byte-swapped N64 ROM header",
    b"\x40\x12\x37\x80": "little-endian N64 ROM header",
}
ARCHIVE_MAGICS = {
    b"PK\x03\x04": "ZIP archive",
    b"7z\xbc\xaf\x27\x1c": "7-Zip archive",
    b"Rar!\x1a\x07": "RAR archive",
    b"\x1f\x8b": "gzip archive",
}
FORBIDDEN_SUFFIXES = {
    ".z64", ".n64", ".v64", ".rom", ".sav", ".save", ".srm", ".eep",
    ".bin", ".iso", ".img", ".elf",
}
FORBIDDEN_NAMES = {
    "base.zip", "baserom", "baserom.z64", "eeprom.bin", "save.dat",
}
ALLOWED_BINARY_SUFFIXES = {
    ".png", ".jpg", ".jpeg", ".gif", ".ico", ".icns", ".ttf", ".otf",
    ".woff", ".woff2", ".pdf", ".ogg", ".wav",
}
BASE64_PAYLOAD = re.compile(rb"(?:[A-Za-z0-9+/]{4096,}={0,2})")
HEX_LITERAL = re.compile(r"\b0[xX][0-9A-Fa-f]{2,16}(?:[uUlL]*)\b")
BRACED_TEXT = re.compile(r"\{([^{}]*)\}", re.DOTALL)
# Unified-diff bookkeeping that carries no payload. "diff --git" ends a file section;
# the rest is skipped without interrupting a run of data lines inside one file section.
DIFF_FILE_BOUNDARY = re.compile(r"^diff --git ")
DIFF_METADATA = re.compile(
    r"^(?:@@ |index |--- |\+\+\+ |old mode |new mode |new file mode |deleted file mode |"
    r"similarity index |rename (?:from|to) |copy (?:from|to) |Binary files |GIT binary patch|"
    r"From [0-9a-f]{7,}|Subject: |\\ No newline)"
)
HEX_ARRAY_MIN_LITERALS = 128
HEX_ARRAY_MIN_DENSITY = 0.65
# A data line is one that is essentially nothing but hexadecimal values and separators.
# Four literals per line keeps ordinary code (masks, flags, a packed RGBA constant) out.
HEX_RUN_MIN_LITERALS_PER_LINE = 4
# Generated source interleaves per-row comments and bare brace lines between data rows.
# Those carry no values, so they neither extend nor interrupt a run.
HEX_RUN_TRANSPARENT = re.compile(r"^\s*(?:/\*|//|\*|[{}\[\](),;]+\s*)*$|^\s*(?:/\*|//|\*)")
# Reviewed port-owned files that legitimately hold a dense hexadecimal table. Each entry
# is exact-path only; a renamed or copied dense array is still rejected.
#   ge_icon.h    the port launcher icon, intentionally stored as a C header.
#   ge_mixer.c   sm64ex's software RSP audio microcode; resample_table is the stock
#                libultra resampler coefficient table, not GoldenEye asset data.
ALLOWED_DENSE_HEX_ARRAY_PATHS = {
    ROOT / "getv" / "port" / "src" / "ge_icon.h",
    ROOT / "getv" / "port" / "ge_mixer.c",
}


def _display(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(ROOT))
    except (OSError, ValueError):
        return path.name


def _is_dense(literals: list[re.Match[str]], body: str) -> bool:
    """Report whether hexadecimal literals dominate the non-whitespace characters."""
    non_whitespace = sum(not char.isspace() for char in body)
    if non_whitespace == 0:
        return False
    literal_characters = sum(len(item.group(0)) for item in literals)
    return literal_characters / non_whitespace >= HEX_ARRAY_MIN_DENSITY


def _has_dense_braced_array(text: str) -> bool:
    """Identify a complete brace-delimited initializer of hexadecimal values."""
    for match in BRACED_TEXT.finditer(text):
        body = match.group(1)
        literals = list(HEX_LITERAL.finditer(body))
        if len(literals) < HEX_ARRAY_MIN_LITERALS:
            continue
        if _is_dense(literals, body):
            return True
    return False


def _has_dense_hex_run(text: str) -> bool:
    """Identify a run of hexadecimal data lines even without the enclosing braces.

    Generated asset source reaches a patch as a hunk that starts partway through an
    initializer, or as a nested table whose innermost braces are individually small.
    Neither shape is brace-delimited in the patch text, so the run is measured line by
    line. The leading unified-diff marker is dropped so that added, removed and context
    lines are all measured on their payload alone, and no value is ever retained.
    """
    run = 0
    for raw in text.splitlines():
        if DIFF_FILE_BOUNDARY.match(raw):
            run = 0
            continue
        if DIFF_METADATA.match(raw):
            continue
        line = raw[1:] if raw[:1] in {"+", "-", " "} else raw
        literals = list(HEX_LITERAL.finditer(line))
        if len(literals) >= HEX_RUN_MIN_LITERALS_PER_LINE and _is_dense(literals, line):
            run += len(literals)
            if run >= HEX_ARRAY_MIN_LITERALS:
                return True
        elif line.strip() and not HEX_RUN_TRANSPARENT.match(line):
            run = 0
    return False


def _has_dense_hex_array(data: bytes) -> bool:
    """Identify large literal arrays without retaining or reporting their values."""
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return _has_dense_braced_array(text) or _has_dense_hex_run(text)


def inspect_path(path: Path, *, allow_native_bmp: bool = False) -> list[str]:
    """Return publication-safety failures without following symlinks."""
    path = path if path.is_absolute() else ROOT / path
    failures: list[str] = []
    display = _display(path)

    if path.is_symlink():
        target = os.readlink(path)
        lower_target = target.lower()
        if any(lower_target.endswith(suffix) for suffix in FORBIDDEN_SUFFIXES):
            failures.append(f"{display}: symlink targets a forbidden game-data file")
        return failures
    if not path.exists():
        return [f"{display}: file does not exist"]
    if not path.is_file():
        return [f"{display}: not a regular file"]

    with path.open("rb") as fh:
        data = fh.read(SCAN_BYTES)
    return inspect_content(path, data, allow_native_bmp=allow_native_bmp)


def inspect_content(path: Path, data: bytes, *, allow_native_bmp: bool = False) -> list[str]:
    """Scan the supplied version of a file, including an index blob at commit time."""
    failures: list[str] = []
    display = _display(path)
    lower_name = path.name.lower()
    lower_parts = {part.lower() for part in path.parts}
    suffix = path.suffix.lower()
    if lower_name in FORBIDDEN_NAMES:
        failures.append(f"{display}: forbidden game-data filename")
    if suffix in FORBIDDEN_SUFFIXES:
        failures.append(f"{display}: forbidden game-data or compiled-output extension")
    if "roms" in lower_parts:
        failures.append(f"{display}: files from a ROM directory may never be published")
    if suffix == ".bmp" and not allow_native_bmp:
        failures.append(f"{display}: rendered BMP captures must not be committed")

    for magic, description in ROM_MAGICS.items():
        if magic in data:
            failures.append(f"{display}: contains a {description}, even if renamed")
    for magic, description in ARCHIVE_MAGICS.items():
        at_archive_header = data.startswith(magic)
        prefixed_archive = len(magic) >= 4 and magic in data[:4096]
        if at_archive_header or prefixed_archive:
            failures.append(f"{display}: {description} files are not accepted as public artifacts")

    binary_allowed = suffix in ALLOWED_BINARY_SUFFIXES or (allow_native_bmp and suffix == ".bmp")
    if b"\x00" in data and not binary_allowed:
        failures.append(f"{display}: unexpected binary content")
    if BASE64_PAYLOAD.search(data):
        failures.append(f"{display}: contains a suspicious encoded binary payload")
    try:
        allowed_dense_array = path.resolve() in {
            allowed.resolve() for allowed in ALLOWED_DENSE_HEX_ARRAY_PATHS
        }
    except OSError:
        allowed_dense_array = False
    if not allowed_dense_array and _has_dense_hex_array(data):
        failures.append(f"{display}: contains a suspicious high-density hexadecimal array")
    return failures


def inspect_staged(path: Path) -> list[str]:
    """Inspect the index version even when the working copy differs or was removed."""
    relative = path.relative_to(ROOT).as_posix()
    entry = subprocess.run(
        ["git", "ls-files", "--stage", "-z", "--", f":(literal){relative}"],
        cwd=ROOT, capture_output=True, check=True,
    ).stdout
    mode, oid, _ = entry.split(b" ", 2)
    data = subprocess.run(["git", "cat-file", "blob", oid.decode("ascii")],
                          cwd=ROOT, capture_output=True, check=True).stdout[:SCAN_BYTES]
    if mode == b"120000":
        if any(os.fsdecode(data).lower().endswith(suffix) for suffix in FORBIDDEN_SUFFIXES):
            return [f"{_display(path)}: symlink targets a forbidden game-data file"]
        return []
    return inspect_content(path, data)


def _git_paths(args: list[str]) -> set[Path]:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, check=True, capture_output=True
    )
    return {
        ROOT / os.fsdecode(value)
        for value in result.stdout.split(b"\0")
        if value
    }


def changed_paths(base: str) -> set[Path]:
    paths = _git_paths(["diff", "--name-only", "--diff-filter=ACMR", "-z", f"{base}...HEAD"])
    paths |= _git_paths(["diff", "--name-only", "--diff-filter=ACMR", "-z"])
    paths |= _git_paths(["diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"])
    paths |= _git_paths(["ls-files", "--others", "--exclude-standard", "-z"])
    return paths


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--changed", metavar="BASE", help="check files changed from BASE plus local changes")
    mode.add_argument("--staged", action="store_true", help="check files staged for commit")
    mode.add_argument("--tracked", action="store_true", help="check every tracked file")
    parser.add_argument("paths", nargs="*", type=Path, help="specific files to check")
    args = parser.parse_args(argv)
    if not (args.changed or args.staged or args.tracked or args.paths):
        parser.error("choose --changed, --staged, --tracked, or provide paths")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    paths = {path if path.is_absolute() else ROOT / path for path in args.paths}
    try:
        if args.changed:
            paths |= changed_paths(args.changed)
        if args.staged:
            paths |= _git_paths(["diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"])
        if args.tracked:
            paths |= _git_paths(["ls-files", "-z"])
    except subprocess.CalledProcessError as exc:
        print(exc.stderr.decode(errors="replace"), file=sys.stderr)
        return 2

    failures: list[str] = []
    checked = 0
    for path in sorted(paths, key=str):
        if args.staged:
            checked += 1
            try:
                failures.extend(inspect_staged(path))
            except (ValueError, subprocess.CalledProcessError):
                failures.append(f"{_display(path)}: cannot inspect staged version")
            continue
        if not path.exists() and not path.is_symlink():
            continue
        checked += 1
        failures.extend(inspect_path(path))

    if failures:
        print("PUBLIC ARTIFACT CHECK FAILED", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        print("Never include or upload a ROM. Remove every prohibited artifact before continuing.", file=sys.stderr)
        return 1

    print(f"public artifact check: {checked} file(s) passed; no prohibited game data detected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
