#!/usr/bin/env python3
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOSS = ROOT / "vendor/ge-decomp/src/boss.c"
WATCH_C = ROOT / "getv/port/src/ge_stall_watchdog.c"
WATCH_H = ROOT / "getv/port/src/ge_stall_watchdog.h"
GFX = ROOT / "getv/port/fast3d/gfx_pc.c"
PATCH = ROOT / "getv/patches/0050-gfx-run-progress-diagnostic.patch"


def body(src: str, name: str) -> str:
    m = re.search(rf"\b{name}\s*\([^)]*\)\s*\{{", src)
    if not m:
        raise AssertionError(f"missing function {name}")
    start = m.start()
    depth = 0
    seen = False
    for i in range(m.end() - 1, len(src)):
        if src[i] == "{":
            depth += 1
            seen = True
        elif src[i] == "}":
            depth -= 1
            if seen and depth == 0:
                return src[start:i + 1]
    raise AssertionError(f"unterminated function {name}")


class GfxRunProgressDiagnostic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.boss = BOSS.read_text()
        cls.watch_c = WATCH_C.read_text()
        cls.watch_h = WATCH_H.read_text()
        cls.gfx = GFX.read_text()
        cls.patch = PATCH.read_text()

    def test_numbered_patch_arms_each_submitted_task(self):
        self.assertIn("extern void gePortStallGfxTaskArm(void);", self.patch)
        self.assertIn("gePortStallGfxTaskArm();", self.patch)
        self.assertIn("gePortStallGfxSubmit();\n                            gePortStallGfxTaskArm();", self.boss)

    def test_patch_registered_all_platform_setups(self):
        for rel in ("tools/setup.sh", "tools/setup-mac.sh", "tools/setup-windows.sh"):
            self.assertIn("0050-gfx-run-progress-diagnostic", (ROOT / rel).read_text(), rel)

    def test_public_probe_api_is_declared(self):
        for decl in (
            "void gePortStallGfxTaskArm(void);",
            "void gePortStallGfxPhase(int phase);",
            "void gePortStallGfxCommand(int opcode, int depth, int progress);",
        ):
            self.assertIn(decl, self.watch_h)

    def test_watchdog_reports_graphics_location(self):
        for field in ("gfxphase=%s", "gfxcmd=%d", "gfxop=0x%02x", "gfxdepth=%d"):
            self.assertIn(field, self.watch_c)

    def test_graphics_subphases_are_named(self):
        for name in ("wapi_start", "rapi_start", "display_list", "flush", "rapi_end", "swap_begin"):
            self.assertIn(f'"{name}"', self.watch_c)

    def test_task_arm_clears_stale_graphics_telemetry(self):
        fn = body(self.watch_c, "gePortStallGfxTaskArm")
        for atom in ("ge_stall_gfx_phase", "ge_stall_gfx_opcode", "ge_stall_gfx_depth", "ge_stall_gfx_progress"):
            self.assertRegex(fn, rf"SDL_AtomicSet\(&{atom},\s*0\)")

    def test_command_probe_does_not_advance_watchdog_heartbeat(self):
        fn = body(self.watch_c, "gePortStallGfxCommand")
        self.assertNotIn("ge_stall_macro_mark", fn)
        self.assertNotIn("ge_stall_heartbeat", fn)

    def test_phase_probe_does_not_advance_watchdog_heartbeat(self):
        fn = body(self.watch_c, "gePortStallGfxPhase")
        self.assertNotIn("ge_stall_macro_mark", fn)
        self.assertNotIn("ge_stall_heartbeat", fn)

    def test_display_list_publishes_current_opcode_depth_and_progress(self):
        self.assertIn('#include "../src/ge_stall_watchdog.h"', self.gfx)
        needle = "uint32_t opcode = (cmd->words.w0 >> 24) & 0xFF;"
        pos = self.gfx.index(needle)
        hook = self.gfx.index("gePortStallGfxCommand((int)opcode, ge_dl_depth, ++ge_stall_gfx_cmd_progress);", pos)
        switch = self.gfx.index("switch (opcode)", hook)
        self.assertLess(pos, hook)
        self.assertLess(hook, switch)

    def test_gfx_run_subphase_order_is_monotonic(self):
        fn = body(self.gfx, "gfx_run")
        positions = [fn.index(f"gePortStallGfxPhase({i});") for i in range(1, 7)]
        self.assertEqual(positions, sorted(positions))

    def test_command_progress_resets_before_display_list_walk(self):
        fn = body(self.gfx, "gfx_run")
        reset = fn.index("ge_stall_gfx_cmd_progress = 0;")
        phase = fn.index("gePortStallGfxPhase(3);")
        walk = fn.index("gfx_run_dl(commands);")
        self.assertLess(reset, phase)
        self.assertLess(phase, walk)

    def test_probe_is_diagnostic_only(self):
        fn = body(self.watch_c, "gePortStallGfxCommand")
        self.assertIn("SDL_AtomicSet", fn)
        for forbidden in ("gfx_run_dl(", "gfx_flush(", "swap_buffers", "return 1", "abort("):
            self.assertNotIn(forbidden, fn)


if __name__ == "__main__":
    unittest.main(verbosity=2)
