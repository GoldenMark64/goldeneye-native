#!/usr/bin/env python3
"""ROM-free regression for the shared Windows MinGW toolchain default."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
EXPECTED = r"C:\mingw64"
FILES = (
    ROOT / "tools" / "fetch_deps_windows.ps1",
    ROOT / "tools" / "install.ps1",
    ROOT / "getv" / "build_windows.ps1",
)


def mingw_default(path: Path) -> str:
    text = path.read_text(encoding="utf-8")
    match = re.search(r"\[string\]\$Mingw\s*=\s*'([^']+)'", text)
    if match is None:
        raise AssertionError(f"could not find the -Mingw default in {path}")
    return match.group(1)


class WindowsToolchainDefaultTests(unittest.TestCase):
    def test_all_windows_entrypoints_use_the_installer_toolchain_root(self) -> None:
        for path in FILES:
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertEqual(mingw_default(path), EXPECTED)


if __name__ == "__main__":
    unittest.main()
