#!/usr/bin/env python3
"""Wrap an existing macOS build in a Finder app that opens its settings launcher.

The app stays beside the executable. No game binary or extracted data is copied.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import plistlib
import shutil


def make_app(binary: Path) -> Path:
    binary = binary.resolve()
    names = {
        "goldeneye": ("GoldenEye", "org.goldeneyenative.launcher.gl"),
        "goldeneye-metal": ("GoldenEye Metal", "org.goldeneyenative.launcher.metal"),
    }
    if binary.name not in names:
        raise ValueError("expected a goldeneye or goldeneye-metal executable")
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise ValueError(f"build the executable first: {binary}")

    name, identifier = names[binary.name]
    app = binary.parent / f"{name}.app"
    macos = app / "Contents/MacOS"
    resources = app / "Contents/Resources"
    macos.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)

    # Resolve from the bundle at launch time so moving the whole build folder works.
    # exec preserves the game's own path/config lookup and its launcher-to-game restart.
    launcher = macos / "LaunchGoldenEye"
    launcher.write_text(
        '#!/bin/bash\n'
        'set -euo pipefail\n'
        'GAME_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"\n'
        'cd "$GAME_DIR"\n'
        f'exec "$GAME_DIR/{binary.name}" --launcher "$@"\n',
        encoding="utf-8",
    )
    launcher.chmod(0o755)
    info = {
        "CFBundleName": name,
        "CFBundleDisplayName": name,
        "CFBundleIdentifier": identifier,
        "CFBundleExecutable": launcher.name,
        "CFBundlePackageType": "APPL",
        "CFBundleIconFile": "GoldenEye.icns",
        "CFBundleShortVersionString": "0.1",
        "CFBundleVersion": "1",
        "LSMinimumSystemVersion": "13.0",
        "NSHighResolutionCapable": True,
    }
    with (app / "Contents/Info.plist").open("wb") as output:
        plistlib.dump(info, output)
    icon = Path(__file__).resolve().parents[1] / "assets/icon/goldeneye-plus.icns"
    shutil.copyfile(icon, resources / "GoldenEye.icns")
    return app


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="existing goldeneye or goldeneye-metal build")
    args = parser.parse_args()
    try:
        app = make_app(args.binary)
    except (OSError, ValueError) as error:
        parser.exit(1, f"mac launcher: {error}\n")
    print(f"mac launcher: {app}")


if __name__ == "__main__":
    main()
