#!/usr/bin/env python3
"""ROM-free regressions for native STAN floor-recovery safety."""
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
STAN = DECOMP / "src/game/stan.c"


def compile_translation_unit(source: str, output: Path) -> subprocess.CompletedProcess[str]:
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
        "-c", source, "-o", str(output),
    ]
    return subprocess.run(cmd, cwd=DECOMP, capture_output=True, text=True)


class StanRecoverySafetyNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bondview = BONDVIEW2.read_text()
        cls.stan = STAN.read_text()

    def test_native_zero_link_guard_precedes_random_modulo(self):
        start = self.bondview.index("void bondviewCalcUpdatePlayerCollision(")
        end = self.bondview.index("void bondviewUpdatePlayerRoom", start)
        fn = self.bondview[start:end]

        # Mission B supersedes the native random-neighbour recovery entirely.
        # In that case the stronger invariant is that the native branch cannot
        # reach a modulo at all. Keep the original guard check for trees where
        # Mission A is the newest recovery patch.
        resolver = fn.find("stanResolveGroundAtCylNative(")
        if resolver != -1:
            native = fn.rfind("#ifdef GE_PORT_NATIVE", 0, resolver)
            native_else = fn.index("#else", resolver)
            self.assertLess(native, resolver)
            self.assertNotIn("randomGetNext() %", fn[native:native_else])
            return

        modulo = fn.index("randomGetNext() % (u32)phi_a0_3")
        guard = fn.rfind("if (phi_a0_3 == 0)", 0, modulo)
        self.assertNotEqual(
            guard,
            -1,
            "native STAN recovery must reject a zero linked-edge count before modulo",
        )

        native = fn.rfind("#ifdef GE_PORT_NATIVE", 0, guard)
        native_end = fn.index("#endif", guard)
        self.assertLess(native, guard)
        self.assertLess(guard, native_end)
        self.assertIn("break;", fn[guard:native_end])

    def test_near_edge_is_reset_for_each_candidate_tile(self):
        start = self.stan.index("StandTile *stanFindTileBelowPos(")
        end = self.stan.index("void stanLoadFile", start)
        fn = self.stan[start:end]

        tile_loop = fn.index("while (((*((u32 *) tile)) != 0) && (tile->room == room))")
        edge_loop = fn.index("for (i = 0; i < 3; i++)", tile_loop)
        near_read = fn.index("if (nearEdge)", edge_loop)
        reset = fn.index("nearEdge = 0;", tile_loop, edge_loop)

        self.assertLess(tile_loop, reset)
        self.assertLess(reset, edge_loop)
        self.assertLess(edge_loop, near_read)

        native = fn.rfind("#ifdef GE_PORT_NATIVE", tile_loop, reset)
        native_end = fn.index("#endif", reset)
        self.assertNotEqual(native, -1)
        self.assertLess(reset, native_end)

    def test_bondview2_translation_unit_compiles_native(self):
        scratch = ROOT / "scratch"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as td:
            obj = Path(td) / "bondview2.o"
            result = compile_translation_unit("src/game/bondview2.c", obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())

    def test_stan_translation_unit_compiles_native(self):
        scratch = ROOT / "scratch"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as td:
            obj = Path(td) / "stan.o"
            result = compile_translation_unit("src/game/stan.c", obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        StanRecoverySafetyNativeTests
    )
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("STAN recovery safety tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
