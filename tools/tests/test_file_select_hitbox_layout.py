#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
FRONT = ROOT / "vendor/ge-decomp/src/game/front.c"


class FileSelectHitboxLayoutTests(unittest.TestCase):
    def test_file_select_uses_real_coord2d_extent_pairs(self):
        source = FRONT.read_text()
        start = source.index("s32 interface_menu05_fileselect(void)")
        end = source.index("void frontUpdateControlStickPosition", start) if "void frontUpdateControlStickPosition" in source[start:] else source.index("Gfx *constructor_menu05_fileselect", start)
        body = source[start:end]

        self.assertIn("struct coord2d folder_x_extents;", body)
        self.assertIn("struct coord2d folder_y_extents;", body)
        self.assertIn("struct coord2d folder_projected_a;", body)
        self.assertIn("struct coord2d folder_projected_b;", body)
        self.assertIn(
            "modelGetXYExtents(walletinst[foldernum], "
            "&folder_x_extents.f[1], &folder_x_extents.f[0], "
            "&folder_y_extents.f[1], &folder_y_extents.f[0]);",
            " ".join(body.split()),
        )
        self.assertIn(
            "projectRectCornersTo2D(&folderpositions_camspace[foldernum], "
            "&folder_x_extents, &folder_y_extents, "
            "&folder_projected_a, &folder_projected_b);",
            " ".join(body.split()),
        )
        self.assertIn("folderbbox.right = folder_projected_a.f[0];", body)
        self.assertIn("folderbbox.down = folder_projected_a.f[1];", body)
        self.assertIn("folderbbox.left = folder_projected_b.f[0];", body)
        self.assertIn("folderbbox.up = folder_projected_b.f[1];", body)

    def test_native_path_does_not_alias_scalar_float_as_coord2d(self):
        source = FRONT.read_text()
        start = source.index("s32 interface_menu05_fileselect(void)")
        end = source.index("Gfx *constructor_menu05_fileselect", start)
        body = source[start:end]
        native_start = body.index("#ifdef GE_PORT_NATIVE", body.index("for (foldernum"))
        native_end = body.index("#else", native_start)
        native_decls = body[native_start:native_end]
        self.assertIn("struct coord2d folder_x_extents;", native_decls)
        self.assertIn("struct coord2d folder_y_extents;", native_decls)
        self.assertIn("struct coord2d folder_projected_a;", native_decls)
        self.assertIn("struct coord2d folder_projected_b;", native_decls)

        call_start = body.index("#ifdef GE_PORT_NATIVE", native_end)
        call_end = body.index("#else", call_start)
        native_calls = body[call_start:call_end]
        self.assertNotIn("&xmin", native_calls)
        self.assertNotIn("&ymin", native_calls)
        self.assertNotIn("&folderbbox.right", native_calls)
        self.assertNotIn("&folderbbox.left", native_calls)
        self.assertIn("&folder_x_extents", native_calls)
        self.assertIn("&folder_y_extents", native_calls)
        self.assertIn("&folder_projected_a", native_calls)
        self.assertIn("&folder_projected_b", native_calls)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(FileSelectHitboxLayoutTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("File-select hitbox tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
