#!/usr/bin/env python3
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOSS = ROOT / "vendor/ge-decomp/src/boss.c"
WATCH_C = ROOT / "getv/port/src/ge_stall_watchdog.c"
WATCH_H = ROOT / "getv/port/src/ge_stall_watchdog.h"
SDL = ROOT / "getv/port/fast3d/gfx_sdl2.c"
PATCH = ROOT / "getv/patches/0051-gpu-sync-present-diagnostic.patch"


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


class GpuSyncPresentDiagnostic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.boss = BOSS.read_text()
        cls.watch_c = WATCH_C.read_text()
        cls.watch_h = WATCH_H.read_text()
        cls.sdl = SDL.read_text()
        cls.patch = PATCH.read_text()

    def test_numbered_patch_arms_probe(self):
        self.assertIn("gePortStallGfxPresentProbeArm();", self.patch)
        self.assertIn("gePortStallGfxPresentProbeArm();", self.boss)

    def test_patch_registered_all_platforms(self):
        for rel in ("tools/setup.sh", "tools/setup-mac.sh", "tools/setup-windows.sh"):
            self.assertIn("0051-gpu-sync-present-diagnostic", (ROOT / rel).read_text(), rel)

    def test_probe_api_declared(self):
        self.assertIn("void gePortStallGfxPresentProbeArm(void);", self.watch_h)
        self.assertIn("int gePortStallGfxSyncProbeEnabled(void);", self.watch_h)

    def test_probe_is_opt_in(self):
        fn = body(self.watch_c, "gePortStallGfxPresentProbeArm")
        self.assertIn('getenv("GETV_GLSYNC_DIAG")', fn)
        self.assertIn("*e == '1'", fn)

    def test_phase_names_present(self):
        for name in ("overlay", "gpu_finish", "present", "present_done"):
            self.assertIn(f'"{name}"', self.watch_c)

    def test_glfinish_only_in_opt_in_branch(self):
        fn = body(self.sdl, "gfx_sdl_swap_buffers_begin")
        cond = fn.index("if (gePortStallGfxSyncProbeEnabled())")
        finish = fn.index("glFinish();")
        present = fn.index("SDL_GL_SwapWindow(wnd);")
        self.assertLess(cond, finish)
        self.assertLess(finish, present)

    def test_present_phase_after_gpu_finish(self):
        fn = body(self.sdl, "gfx_sdl_swap_buffers_begin")
        gl = fn.split("#else", 1)[1]
        overlay = gl.index("gePortStallGfxPhase(7);")
        gpu = gl.index("gePortStallGfxPhase(8);")
        present = gl.index("gePortStallGfxPhase(9);")
        done = gl.index("gePortStallGfxPhase(10);")
        self.assertLess(overlay, gpu)
        self.assertLess(gpu, present)
        self.assertLess(present, done)

    def test_probe_does_not_change_normal_path(self):
        fn = body(self.sdl, "gfx_sdl_swap_buffers_begin")
        self.assertEqual(fn.count("SDL_GL_SwapWindow(wnd);"), 1)
        self.assertEqual(fn.count("glFinish();"), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
