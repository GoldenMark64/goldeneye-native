#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
BONDVIEW2 = DECOMP / "src/game/bondview2.c"
BONDTYPES = DECOMP / "src/bondtypes.h"
BONDVIEW_H = DECOMP / "src/game/bondview.h"


def compile_bondview2(output: Path) -> subprocess.CompletedProcess[str]:
    compiler = shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        raise unittest.SkipTest("a C compiler is required")

    warning_flags = ["-Werror=return-type"]
    if "clang" in Path(compiler).name:
        warning_flags = ["-Wno-everything", "-Werror=return-type", "-ferror-limit=0"]

    cmd = [
        compiler,
        "-fms-extensions",
        "-include", "src/ge_port_decls.h",
        "-I", ".", "-I", "include", "-I", "include/PR",
        "-I", "src", "-I", "src/game", "-I", "src/inflate",
        "-DVERSION_US", "-DLANG_US", "-DREFRESH_NTSC",
        "-DLEFTOVERDEBUG", "-DLEFTOVERSPECTRUM", "-DBUGFIX_R0",
        "-DTARGET_N64", "-DGE_PORT_NATIVE", "-DNON_MATCHING=1",
        "-DAVOID_UB=1", "-D_LANGUAGE_C=1",
        *warning_flags,
        "-fno-strict-aliasing", "-O1",
        "-c", "src/game/bondview2.c", "-o", str(output),
    ]
    return subprocess.run(cmd, cwd=DECOMP, capture_output=True, text=True)


class LadderLocusNativeTests(unittest.TestCase):
    def test_native_movebond_uses_full_locus_record(self):
        source = BONDVIEW2.read_text()
        move = source[source.index("void MoveBond("):source.index("void bondviewFrozenMoveBond", source.index("void MoveBond("))]
        marker = move.index("struct StandTileLocusCallbackRecord curLocus;")
        native_start = move.rfind("#ifdef GE_PORT_NATIVE", 0, marker)
        native_else = move.index("#else", marker)
        native_end = move.index("#endif", native_else)

        native = move[native_start:native_else]
        legacy = move[native_else:native_end]

        self.assertIn("struct StandTileLocusCallbackRecord curLocus;", native)
        self.assertNotIn("struct move_bond_temp_struct curLocus;", native)
        self.assertIn("struct move_bond_temp_struct curLocus;", legacy)

    def test_native_record_is_required_by_widened_layout(self):
        bondtypes = BONDTYPES.read_text()
        record_start = bondtypes.index("struct StandTileLocusCallbackRecord")
        record_end = bondtypes.index("};", record_start)
        record = bondtypes[record_start:record_end]

        bondview_h = BONDVIEW_H.read_text()
        temp_start = bondview_h.index("struct move_bond_temp_struct")
        temp_end = bondview_h.index("};", temp_start)
        temp = bondview_h[temp_start:temp_end]

        self.assertIn("s32 *rooms;", record)
        self.assertIn("s32 count;", record)
        self.assertIn("s32 bufMax;", record)
        self.assertIn("s32 nearEdgeCount;", record)

        # The retail placeholder is deliberately only two 32-bit words.
        self.assertEqual(temp.count("s32 "), 2)

    def test_movebond_locus_queries_share_the_fixed_record(self):
        source = BONDVIEW2.read_text()
        move = source[source.index("void MoveBond("):source.index("void bondviewFrozenMoveBond", source.index("void MoveBond("))]
        self.assertGreaterEqual(move.count("&curLocus"), 8)
        self.assertEqual(move.count("struct StandTileLocusCallbackRecord curLocus;"), 1)

        for needle in (
            "stanTileDistanceRelated(",
            "stanGetLocusCount(&curLocus)",
            "stanGetMoveBondCollisionTiles(&sp174, &sp170, &bondCollision)",
        ):
            self.assertIn(needle, move)

    def test_native_translation_unit_compiles(self):
        scratch = ROOT / "scratch"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as td:
            obj = Path(td) / "bondview2.o"
            result = compile_bondview2(obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(LadderLocusNativeTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Ladder locus native tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
