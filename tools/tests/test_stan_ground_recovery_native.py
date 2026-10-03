#!/usr/bin/env python3
"""ROM-free behavioral regression for native STAN ground recovery."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
STAN = DECOMP / "src/game/stan.c"
STAN_H = DECOMP / "src/game/stan.h"
BONDVIEW2 = DECOMP / "src/game/bondview2.c"


def compiler() -> str:
    cc = shutil.which("clang") or shutil.which("gcc")
    if not cc:
        raise unittest.SkipTest("a C compiler is required")
    return cc


def compile_translation_unit(source: str, output: Path) -> subprocess.CompletedProcess[str]:
    cc = compiler()
    warning_flags = ["-Werror=return-type"]
    if "clang" in Path(cc).name:
        warning_flags = ["-Wno-everything", "-Werror=return-type", "-ferror-limit=0"]

    cmd = [
        cc,
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


class StanGroundRecoveryNativeTests(unittest.TestCase):
    def test_ground_math_behavior(self):
        source = textwrap.dedent(
            r'''
            #include <math.h>
            #include <stdio.h>
            #include "game/stan_ground_native.h"

            static int closef(float a, float b) {
                return fabsf(a - b) < 0.0001f;
            }

            int main(void) {
                const float square[] = {
                    0.0f, 0.0f,
                    10.0f, 0.0f,
                    10.0f, 10.0f,
                    0.0f, 10.0f,
                };
                const float square_reverse[] = {
                    0.0f, 10.0f,
                    10.0f, 10.0f,
                    10.0f, 0.0f,
                    0.0f, 0.0f,
                };
                const float line[] = {
                    0.0f, 0.0f,
                    5.0f, 0.0f,
                    10.0f, 0.0f,
                };
                float cx = 0.0f;
                float cz = 0.0f;
                float d2;
                GeStanGroundChoiceNative choice;
                const int rooms[] = {2, 7};
                int i;

                if (!geStanNativeRoomAllowed(7, rooms, 2)) return 1;
                if (geStanNativeRoomAllowed(5, rooms, 2)) return 2;
                if (!geStanNativeRoomAllowed(99, NULL, 0)) return 3;

                if (!geStanNativePointInConvexPolygonXZ(square, 4, 5.0f, 5.0f)) return 10;
                if (!geStanNativePointInConvexPolygonXZ(square_reverse, 4, 5.0f, 5.0f)) return 13;
                if (geStanNativePointInConvexPolygonXZ(square, 4, 10.5f, 5.0f)) return 11;
                if (geStanNativePointInConvexPolygonXZ(line, 3, 5.0f, 0.0f)) return 12;

                d2 = geStanNativeClosestPointPolygonXZ(square, 4, 10.5f, 5.0f, &cx, &cz);
                if (!closef(d2, 0.25f) || !closef(cx, 10.0f) || !closef(cz, 5.0f)) return 20;

                d2 = geStanNativeClosestPointPolygonXZ(square, 4, 10.3f, 10.4f, &cx, &cz);
                if (!closef(d2, 0.25f) || !closef(cx, 10.0f) || !closef(cz, 10.0f)) return 21;

                geStanNativeGroundChoiceInit(&choice);
                geStanNativeGroundChoiceConsider(&choice, 1, 10.0f, 0.01f, 0, 20.0f, 1.0f);
                geStanNativeGroundChoiceConsider(&choice, 2, 2.0f, 0.0f, 1, 20.0f, 1.0f);
                if (!choice.valid || !choice.center || choice.index != 2 || !closef(choice.y, 2.0f)) return 30;
                geStanNativeGroundChoiceConsider(&choice, 3, 7.0f, 0.0f, 1, 20.0f, 1.0f);
                if (choice.index != 3 || !closef(choice.y, 7.0f)) return 31;
                geStanNativeGroundChoiceConsider(&choice, 4, 25.0f, 0.0f, 1, 20.0f, 1.0f);
                if (choice.index != 3) return 32;

                geStanNativeGroundChoiceInit(&choice);
                geStanNativeGroundChoiceConsider(&choice, 5, 4.0f, 0.64f, 0, 20.0f, 1.0f);
                geStanNativeGroundChoiceConsider(&choice, 6, 9.0f, 0.25f, 0, 20.0f, 1.0f);
                if (!choice.valid || choice.center || choice.index != 6) return 40;
                geStanNativeGroundChoiceConsider(&choice, 8, 11.0f, 0.25f, 0, 20.0f, 1.0f);
                if (choice.index != 6) return 42;
                geStanNativeGroundChoiceConsider(&choice, 7, 12.0f, 1.21f, 0, 20.0f, 1.0f);
                if (choice.index != 6) return 41;

                for (i = 0; i < 100; i++) {
                    GeStanGroundChoiceNative repeated;
                    geStanNativeGroundChoiceInit(&repeated);
                    geStanNativeGroundChoiceConsider(&repeated, 5, 4.0f, 0.64f, 0, 20.0f, 1.0f);
                    geStanNativeGroundChoiceConsider(&repeated, 6, 9.0f, 0.25f, 0, 20.0f, 1.0f);
                    geStanNativeGroundChoiceConsider(&repeated, 2, 2.0f, 0.0f, 1, 20.0f, 1.0f);
                    geStanNativeGroundChoiceConsider(&repeated, 3, 7.0f, 0.0f, 1, 20.0f, 1.0f);
                    if (repeated.index != 3 || !repeated.center || !closef(repeated.y, 7.0f)) return 50;
                }

                puts("PASS");
                return 0;
            }
            '''
        )

        scratch = ROOT / "scratch"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as td:
            tdpath = Path(td)
            cfile = tdpath / "stan-ground-test.c"
            exe = tdpath / "stan-ground-test"
            cfile.write_text(source)
            result = subprocess.run(
                [
                    compiler(),
                    "-std=c11",
                    "-I", str(DECOMP / "src"),
                    str(cfile),
                    "-lm",
                    "-o", str(exe),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("PASS", run.stdout)

    def test_stan_resolver_uses_authored_walk_room_locus_and_cylinder_search(self):
        source = STAN.read_text()
        start = source.index("StandTile *stanResolveGroundAtCylNative(")
        end = source.index("#endif", start)
        fn = source[start:end]

        self.assertIn("sub_GAME_7F0B0C24(", fn)
        self.assertIn("sub_GAME_7F0B21B0(", fn)
        self.assertIn("stanFindGroundAtCylNative(", fn)
        self.assertIn("pathRooms", fn)
        self.assertIn("locusRooms", fn)

    def test_room_filtered_search_has_global_fallback(self):
        source = STAN.read_text()
        start = source.index("StandTile *stanResolveGroundAtCylNative(")
        end = source.index("#endif", start)
        fn = source[start:end]

        compact = " ".join(fn.split())
        filtered = compact.index(
            "stanFindGroundAtCylNative( &probe, radius, roomFilter, roomCount, yRtn)")
        exhaustive = compact.index(
            "stanFindGroundAtCylNative( &probe, radius, NULL, 0, yRtn)", filtered)
        self.assertLess(filtered, exhaustive)

    def test_bondview_native_recovery_replaces_random_hops(self):
        source = BONDVIEW2.read_text()
        start = source.index("if (stanTestPointWithinTileBoundsMaybe(")
        end = source.index("bondviewUpdatePlayerRoom", start)
        block = source[start:end]

        native = block.index("#ifdef GE_PORT_NATIVE")
        legacy = block.index("#else", native)
        native_text = block[native:legacy]
        legacy_text = block[legacy:]

        self.assertIn("stanResolveGroundAtCylNative(", native_text)
        self.assertNotIn("randomGetNext()", native_text)
        self.assertIn("randomGetNext()", legacy_text)

    def test_new_native_api_is_declared(self):
        header = STAN_H.read_text()
        self.assertIn("stanResolveGroundAtCylNative", header)

    def test_stan_translation_unit_compiles_native(self):
        scratch = ROOT / "scratch"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as td:
            obj = Path(td) / "stan.o"
            result = compile_translation_unit("src/game/stan.c", obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())

    def test_bondview2_translation_unit_compiles_native(self):
        scratch = ROOT / "scratch"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as td:
            obj = Path(td) / "bondview2.o"
            result = compile_translation_unit("src/game/bondview2.c", obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(StanGroundRecoveryNativeTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("STAN ground recovery native tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
