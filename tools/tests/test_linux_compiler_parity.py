#!/usr/bin/env python3
"""ROM-free regression for Linux compiler selection."""

from __future__ import annotations

import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "getv" / "build_linux.sh"
INSTALL = ROOT / "tools" / "install.sh"


def _write_exe(path: Path, body: str) -> None:
    path.write_text(body, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


class LinuxCompilerParityTests(unittest.TestCase):
    def run_env(self, arch: str, override: str | None = None) -> str:
        with tempfile.TemporaryDirectory() as td:
            bindir = Path(td)
            _write_exe(
                bindir / "uname",
                "#!/bin/sh\n"
                "if [ \"$1\" = \"-m\" ]; then echo " + arch
                + "; else exec /usr/bin/uname \"$@\"; fi\n",
            )
            _write_exe(bindir / "gcc", "#!/bin/sh\necho 'gcc (fake) 13.3.0'\n")
            _write_exe(bindir / "g++", "#!/bin/sh\necho 'g++ (fake) 13.3.0'\n")
            _write_exe(bindir / "clang", "#!/bin/sh\necho 'clang version 18.1.0'\n")
            _write_exe(bindir / "clang++", "#!/bin/sh\necho 'clang version 18.1.0'\n")

            env = os.environ.copy()
            env.pop("CC", None)
            env.pop("CXX", None)
            env["PATH"] = str(bindir) + os.pathsep + env["PATH"]
            if override is not None:
                env["CC"] = override

            proc = subprocess.run(
                ["bash", str(BUILD), "env"],
                cwd=ROOT,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(proc.returncode, 0, proc.stdout)
            return proc.stdout

    def test_x86_64_defaults_to_gcc_for_frozen_0064_parity(self) -> None:
        self.assertIn("CC=gcc (gcc)", self.run_env("x86_64"))

    def test_non_x86_64_keeps_clang_first_policy(self) -> None:
        self.assertIn("CC=clang (clang)", self.run_env("aarch64"))

    def test_explicit_cc_override_still_wins(self) -> None:
        self.assertIn("CC=clang (clang)", self.run_env("x86_64", override="clang"))

    def test_build_rejects_missing_explicit_cxx_driver(self) -> None:
        env = os.environ.copy()
        env["CC"] = "gcc"
        env["CXX"] = "goldeneye-missing-cxx-driver"
        proc = subprocess.run(
            ["bash", str(BUILD), "env"],
            cwd=ROOT,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertNotEqual(proc.returncode, 0, proc.stdout)
        self.assertIn("C++ compiler 'goldeneye-missing-cxx-driver' not found", proc.stdout)

    def test_installer_rejects_missing_matching_cxx_before_fetching(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            bindir = Path(td)
            _write_exe(
                bindir / "uname",
                "#!/bin/sh\n"
                "case \"$1\" in\n"
                "  -s) echo Linux ;;\n"
                "  -m) echo x86_64 ;;\n"
                "  *) exec /usr/bin/uname \"$@\" ;;\n"
                "esac\n",
            )
            _write_exe(bindir / "gcc", "#!/bin/sh\necho 'gcc (fake) 13.3.0'\n")

            env = os.environ.copy()
            env["PATH"] = str(bindir) + os.pathsep + env["PATH"]
            env["CC"] = "gcc"
            env["CXX"] = "goldeneye-missing-cxx-driver"

            proc = subprocess.run(
                ["bash", str(INSTALL), "--no-build", "--yes"],
                cwd=ROOT,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertNotEqual(proc.returncode, 0, proc.stdout)
            self.assertIn("a-C++-compiler", proc.stdout)
            self.assertNotIn("third-party port-layer sources", proc.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
