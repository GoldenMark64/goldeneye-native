#!/usr/bin/env python3
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOSS = ROOT / "vendor/ge-decomp/src/boss.c"
WATCH_C = ROOT / "getv/port/src/ge_stall_watchdog.c"
WATCH_H = ROOT / "getv/port/src/ge_stall_watchdog.h"
DRAW_H = ROOT / "getv/port/src/ge_draw_diag.h"
GL = ROOT / "getv/port/fast3d/gfx_opengl.c"
PATCH = ROOT / "getv/patches/0052-draw-sync-attribution-diagnostic.patch"


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


class DrawSyncAttributionDiagnostic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.boss = BOSS.read_text()
        cls.watch_c = WATCH_C.read_text()
        cls.watch_h = WATCH_H.read_text()
        cls.draw_h = DRAW_H.read_text()
        cls.gl = GL.read_text()
        cls.patch = PATCH.read_text()

    def test_numbered_patch_arms_task(self):
        self.assertIn("gePortDrawSyncTaskArm();", self.patch)
        self.assertIn("gePortDrawSyncTaskArm();", self.boss)

    def test_patch_registered_all_platforms(self):
        for rel in ("tools/setup.sh", "tools/setup-mac.sh", "tools/setup-windows.sh"):
            self.assertIn("0052-draw-sync-attribution-diagnostic", (ROOT / rel).read_text(), rel)

    def test_public_api_declared(self):
        self.assertIn("void gePortDrawSyncTaskArm(void);", self.watch_h)
        self.assertIn("int gePortDrawSyncEnabled(void);", self.watch_h)

    def test_sync_is_opt_in(self):
        fn = body(self.watch_c, "gePortDrawSyncTaskArm")
        self.assertIn('getenv("GETV_DRAWDIAG_SYNC")', fn)
        self.assertIn("*e == '1'", fn)

    def test_watchdog_has_draw_sync_phase(self):
        self.assertIn('case 11: return "draw_sync";', self.watch_c)

    def test_draw_filter_resets_and_marks_selected(self):
        fn = body(self.draw_h, "geDrawDiagBeforeSubmit")
        reset = fn.index("ge_drawdiag_last_selected = 0;")
        mark = fn.index("ge_drawdiag_last_selected = 1;")
        filter_pos = fn.index("if ((verts_filter != 0")
        self.assertLess(reset, filter_pos)
        self.assertLess(filter_pos, mark)
        self.assertIn("return ge_drawdiag_last_selected;", body(self.draw_h, "geDrawDiagLastSelected"))

    def test_sync_happens_after_selected_draw_only(self):
        fn = body(self.gl, "gfx_opengl_draw_triangles")
        selected = fn.index("drawdiag_selected = geDrawDiagLastSelected();")
        draw = fn.index("glDrawArrays(GL_TRIANGLES")
        cond = fn.index("if (drawdiag_selected && gePortDrawSyncEnabled())")
        phase = fn.index("gePortStallGfxPhase(11);")
        finish = fn.index("glFinish();", phase)
        restore = fn.index("gePortStallGfxPhase(3);", finish)
        self.assertLess(selected, draw)
        self.assertLess(draw, cond)
        self.assertLess(cond, phase)
        self.assertLess(phase, finish)
        self.assertLess(finish, restore)

    def test_normal_path_has_single_attribution_finish(self):
        fn = body(self.gl, "gfx_opengl_draw_triangles")
        self.assertEqual(fn.count("glFinish();"), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
