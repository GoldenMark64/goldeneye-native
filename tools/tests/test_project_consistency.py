#!/usr/bin/env python3
"""ROM-free consistency checks for public documentation and repository inventory."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class ProjectConsistencyTests(unittest.TestCase):
    def test_public_stage_count_matches_launcher_table(self) -> None:
        launcher = (ROOT / "getv/port/src/ge_launcher.cpp").read_text(encoding="utf-8")
        start = launcher.index("const Stage kStages[] = {")
        end = launcher.index("};", start)
        block = launcher[start:end]
        stage_count = len(re.findall(r"^\s*\{\s*\d+\s*,", block, re.MULTILINE))
        self.assertGreater(stage_count, 0)

        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        site = (ROOT / "site/index.html").read_text(encoding="utf-8")
        self.assertIn(f"**{stage_count} loadable stages**", readme)
        self.assertIn(f'data-count="{stage_count}">0</div><div class="l">Loadable stages', site)

    def test_ci_trust_inventory_covers_every_workflow(self) -> None:
        workflows = {
            path.name
            for path in (ROOT / ".github/workflows").glob("*.yml")
            if path.is_file()
        }
        trust = (ROOT / "docs/CI_TRUST.md").read_text(encoding="utf-8")
        documented = set(re.findall(r"^\| `([^`]+\.yml)` \|", trust, re.MULTILINE))
        self.assertEqual(documented, workflows)

    def test_primary_config_filename_matches_configuration_guide(self) -> None:
        source = (ROOT / "getv/port/src/ge_config.c").read_text(encoding="utf-8")
        match = re.search(r'#define GE_CFG_BASENAME "([^"]+)"', source)
        self.assertIsNotNone(match)
        primary = match.group(1)
        guide = (ROOT / "docs/CONFIGURATION.md").read_text(encoding="utf-8")
        self.assertIn(f"3. `{primary}`", guide)
        self.assertIn(f"4. `{primary}`", guide)


if __name__ == "__main__":
    unittest.main()
