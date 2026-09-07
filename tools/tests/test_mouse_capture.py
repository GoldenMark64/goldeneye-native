#!/usr/bin/env python3
"""Run ROM-free mouse handoff checks through the production SDL event and polling paths.

Requires SDL2 headers, a C compiler, and the reconstructed gfx_sdl2.c (fetch-thirdparty.sh).
Only SDL/device state and unrelated window callbacks are simulated. No SDL library, game
assets, renderer or window is used. --source-root runs the same checks against an old checkout.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = ("resume", "focus", "ownership", "failure", "disabled", "idle", "selftest-x",
             "selftest-y", "unfocused-start")


def section(path, start, end):
    source = path.read_text()
    first = source.index(start)
    return source[first:source.index(end, first)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--sdl-include", type=Path)
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if compiler is None:
        parser.error(f"C compiler unavailable: {args.cc}")
    candidates = ([args.sdl_include] if args.sdl_include else []) + [
        Path.home() / ".n64tvos/sdl2-mac/include/SDL2", Path("/usr/include/SDL2"),
        Path("/opt/homebrew/include/SDL2"), Path("/usr/local/include/SDL2")]
    include = next((p for p in candidates if (p / "SDL.h").is_file()), None)
    if include is None:
        parser.error("SDL2 headers unavailable; install them or pass --sdl-include")
    source = args.source_root.resolve()
    port = source / "getv/port/src"
    try:
        mouse = section(port / "port_input.c", "static int geKeyboardIdle(void);",
                        "static int geKeyboardEnabled(void)")
        events = section(source / "getv/port/fast3d/gfx_sdl2.c",
                         "static void gfx_sdl_handle_events(void)",
                         "static void gfx_sdl_set_keyboard_callbacks(")
    except (OSError, ValueError) as error:
        parser.error(f"required production source unavailable or extraction boundary changed: {error}")

    with tempfile.TemporaryDirectory(prefix="ge-mouse-capture-") as temporary:
        directory = Path(temporary)
        (directory / "mouse.inc").write_text(mouse)
        (directory / "events.inc").write_text(events)
        executable = directory / "test_mouse_capture"
        subprocess.run([compiler, "-std=gnu17", "-O1", "-g", "-Wall", "-Wextra",
                        "-Werror", "-Wno-unused-parameter", "-DGE_PLATFORM_DESKTOP",
                        "-I" + str(include), "-I" + str(port), "-I" + str(directory),
                        str(ROOT / "getv/port/tests/mouse_capture_harness.c"),
                        "-o", str(executable)], check=True, timeout=60)
        failures = 0
        total = 0
        for scenario in SCENARIOS:
            result = subprocess.run([str(executable), scenario], capture_output=True,
                                    text=True, timeout=10)
            print(result.stdout + result.stderr, end="")
            count = sum(line.startswith(("PASS ", "FAIL "))
                        for line in result.stdout.splitlines())
            total += count
            if result.returncode or count == 0:
                failures += 1
                print(f"FAIL scenario {scenario}: exit={result.returncode}, checks={count}")
        print(f"{len(SCENARIOS)} scenarios, {total} checks, {failures} failed scenarios")
        return int(failures != 0 or total == 0)


if __name__ == "__main__":
    raise SystemExit(main())
