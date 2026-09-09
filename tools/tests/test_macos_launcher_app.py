"""ROM-free packaging checks, including the normal build driver's app/bundle commands."""

from __future__ import annotations

import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
# Lets the same regression exercise an unchanged checkout without modifying it.
SOURCE_ROOT = Path(os.environ.get("GETV_LAUNCHER_TEST_ROOT", ROOT))


class MacLauncherTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="mac launcher ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "checkout with spaces"
        self.getv = self.root / "getv"
        self.getv.mkdir(parents=True)
        self.tools = self.root / "tools"
        self.tools.mkdir()
        shutil.copyfile(SOURCE_ROOT / "getv/build_mac.sh", self.getv / "build_mac.sh")
        generator = SOURCE_ROOT / "tools/make_macos_launcher_app.py"
        if generator.exists():
            shutil.copyfile(generator, self.tools / generator.name)
        icon = self.root / "assets/icon/goldeneye-plus.icns"
        icon.parent.mkdir(parents=True)
        icon.write_bytes(b"synthetic icon fixture")
        self.commands = self.root / "commands"
        self.commands.mkdir()
        self.write_command("xcrun", '#!/bin/sh\nprintf "/synthetic-sdk\\n"\n')
        self.write_command("sysctl", '#!/bin/sh\nprintf "1\\n"\n')
        self.write_command("lipo", '#!/bin/sh\nprintf "arm64\\n"\n')
        self.write_command("ar", '#!/bin/sh\n: > "$2"\n')
        self.write_command("clang", '''#!/bin/bash
while [ "$#" -gt 0 ]; do
    if [ "$1" = "-o" ]; then
        shift
        printf '#!/bin/sh\\nexit 0\\n' > "$1"
        chmod +x "$1"
        break
    fi
    shift
done
''')
        self.env = os.environ.copy()
        self.env.update(PATH=f"{self.commands}:{self.env['PATH']}", MACARCH="arm64")

    def write_command(self, name: str, content: str) -> None:
        command = self.commands / name
        command.write_text(content, encoding="utf-8")
        command.chmod(0o755)

    def build_paths(self, renderer: str) -> tuple[Path, Path]:
        suffix = "-metal" if renderer == "metal" else ""
        build = self.getv / f"build-mac{suffix}"
        build.mkdir(exist_ok=True)
        return build / f"goldeneye{suffix}", build / ("GoldenEye Metal.app" if suffix else "GoldenEye.app")

    def run_build(self, renderer: str, action: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["bash", str(self.getv / "build_mac.sh"), action],
            env={**self.env, "GETV_RENDERER": renderer}, cwd=self.temp.name,
            text=True, capture_output=True, timeout=20,
        )

    def bundle(self, renderer: str = "gl") -> tuple[Path, Path]:
        binary, app = self.build_paths(renderer)
        binary.write_text(
            f'#!{sys.executable}\nimport json, os, sys\n'
            'print(json.dumps({"argv": sys.argv, "cwd": os.getcwd(), '
            '"marker": os.environ.get("GETV_TEST_MARKER")}))\n',
            encoding="utf-8",
        )
        binary.chmod(0o755)
        result = self.run_build(renderer, "bundle")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(app.is_dir(), result.stdout + result.stderr)
        return binary, app

    def test_normal_app_build_also_creates_launcher(self) -> None:
        for renderer in ("gl", "metal"):
            with self.subTest(renderer=renderer):
                binary, app = self.build_paths(renderer)
                objects = binary.parent / "obj"
                objects.mkdir()
                for name in ("port_ge_tvos_main.o", "port_ge_mac_main.o", "port_synthetic.o"):
                    (objects / name).touch()
                result = self.run_build(renderer, "app")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue(binary.is_file())
                self.assertTrue((app / "Contents/Info.plist").is_file(), result.stdout + result.stderr)

    def test_bundle_metadata_and_contents(self) -> None:
        identifiers = set()
        for renderer in ("gl", "metal"):
            binary, app = self.bundle(renderer)
            info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
            identifiers.add(info["CFBundleIdentifier"])
            self.assertEqual(info["CFBundlePackageType"], "APPL")
            self.assertEqual(info["CFBundleName"], app.stem)
            self.assertTrue(info["NSHighResolutionCapable"])
            files = {p.relative_to(app).as_posix() for p in app.rglob("*") if p.is_file()}
            self.assertEqual(files, {"Contents/Info.plist", "Contents/MacOS/LaunchGoldenEye", "Contents/Resources/GoldenEye.icns"})
            self.assertEqual(info["GERendererBuild"], renderer)
        self.assertEqual(len(identifiers), 2)

    def test_rebundling_preserves_binary_and_settings(self) -> None:
        binary, app = self.bundle()
        original = binary.read_bytes()
        config = binary.parent / "goldeneye.cfg"
        config.write_text("resolution = 1280x960\n", encoding="utf-8")
        result = self.run_build("gl", "bundle")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(binary.read_bytes(), original)
        self.assertEqual(config.read_text(), "resolution = 1280x960\n")
        self.assertTrue(os.access(app / "Contents/MacOS/LaunchGoldenEye", os.X_OK))

    def test_missing_or_nonexecutable_binary_fails(self) -> None:
        binary, app = self.build_paths("gl")
        for exists in (False, True):
            with self.subTest(exists=exists):
                if exists:
                    binary.write_text("not executable\n", encoding="utf-8")
                result = self.run_build("gl", "bundle")
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertFalse(app.exists())

    def test_compile_failure_does_not_replace_existing_bootstrap(self) -> None:
        binary, app = self.bundle()
        entry = app / "Contents/MacOS/LaunchGoldenEye"
        original = entry.read_bytes()
        self.write_command("clang", "#!/bin/sh\nexit 1\n")
        result = self.run_build("gl", "bundle")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(entry.read_bytes(), original)

    def test_direct_binary_does_not_request_launcher(self) -> None:
        binary, _ = self.bundle()
        result = subprocess.run([str(binary)], text=True, capture_output=True, check=True)
        self.assertEqual(json.loads(result.stdout)["argv"], [str(binary)])


if __name__ == "__main__":
    unittest.main()
