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
import subprocess


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

    # Compile a ROM-free native bootstrap; no Python/SDL runtime is needed at launch.
    launcher = macos / "LaunchGoldenEye"
    source = Path(__file__).resolve().parents[1] / "getv/port/mac/ge_renderer_app.m"
    arch = os.environ.get("MACARCH", os.uname().machine)
    if arch not in ("arm64", "x86_64"):
        raise ValueError(f"unsupported app architecture: {arch}")
    temporary = macos / "LaunchGoldenEye.new"
    try:
        subprocess.run(["clang", "-fobjc-arc", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-parameter", "-target", f"{arch}-apple-macos13.0",
                        "-framework", "Cocoa", "-framework", "Metal", str(source),
                        "-o", str(temporary)], check=True)
        temporary.replace(launcher)
    finally:
        temporary.unlink(missing_ok=True)
    info = {
        "GERendererBuild": "metal" if binary.name == "goldeneye-metal" else "gl",
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
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"mac launcher: {error}\n")
    print(f"mac launcher: {app}")


if __name__ == "__main__":
    main()
