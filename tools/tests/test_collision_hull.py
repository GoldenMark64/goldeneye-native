"""ROM-free checks of the patched decomp's actual box-projection function.

Requires the source checkout prepared by tools/setup.sh, but no generated assets.
Only the function under test is compiled; no decomp source is stored in this test.
Run: python3 tools/tests/test_collision_hull.py
Direct execution is strict: skipped tests or zero executed tests fail the run.
Unittest discovery retains optional skips for unprepared developer checkouts.
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "vendor/ge-decomp/src/game/chrprop.c"

HARNESS = r"""
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
typedef float f32;
typedef double f64;
typedef int s32;
typedef struct { float m[4][4]; } Mtxf;
typedef struct coord2d { union { struct { float x, y; }; float f[2]; }; } coord2d;
struct rect4f { coord2d points[4]; };
struct collision_data { int edges; coord2d polygon[8]; float top, bottom; };
#ifdef LEGACY_RECT4_ACCESS
#define GE_COLLISION_POLY_POINT(poly, index) ((poly)->points[index])
#else
#define GE_COLLISION_POLY_POINT(poly, index) (((coord2d *)(void *)(poly))[index])
#endif
#include "hull.inc"

int main(int argc, char **argv)
{
    Mtxf m = {0};
    struct collision_data collision = {0};
    struct rect4f *poly = (struct rect4f *)(void *)collision.polygon;
    float x = 1, y = 1, z = 1;
    double expected, area = 0;
    int which = atoi(argv[1]);
    m.m[0][0] = m.m[1][1] = m.m[2][2] = 1;
    if (which == 0) {
        /* All extrema coincide: seven candidates, zero projected area. */
        x = y = z = 0;
        expected = 0;
    } else if (which == 1) {
        /* Orthogonal rows of length 3, and a zero-depth box. Extrema
         * are 4/2/2/4; rem is 0,1,3,5,6,7. Corner 6 is required.
         * The projected parallelogram has area 4 * abs(-1*1-2*(-2)). */
        m = (Mtxf){{{-1,-2,-2,0},{2,-2,1,0},{2,1,-2,0},{0,0,0,0}}};
        z = 0;
        expected = 12;
    } else if (which == 2) {
        /* Four distinct extrema: a translated ordinary box. */
        x = 2; y = 3; z = 4;
        m.m[3][0] = 10; m.m[3][2] = -7;
        expected = 32;
    } else if (which == 3) {
        /* A fully three-dimensional box under the same rotation.
         * Projection area is the sum of its three parallelograms. */
        m = (Mtxf){{{-1,-2,-2,0},{2,-2,1,0},{2,1,-2,0},{0,0,0,0}}};
        expected = 4 * (3 + 6 + 6);
    } else {
        /* A line must remain a line, even with repeated extrema. */
        y = z = 0;
        expected = 0;
    }
    sub_GAME_7F03ECC0(-x,x,-y,y,-z,z,&m,poly,&collision);
    if (collision.edges < 4 || collision.edges > 8) return 2;
    for (int i = 0; i < collision.edges; ++i) {
        int j = (i + 1) % collision.edges;
        area += (double)collision.polygon[i].x * collision.polygon[j].y
              - (double)collision.polygon[j].x * collision.polygon[i].y;
    }
    area = fabs(area) / 2;
    if (fabs(area - expected) > 0.0001) {
        fprintf(stderr, "case %d: area %.9g, expected %.9g\n", which, area, expected);
        return 3;
    }
    printf("case %d: edges=%d area=%.9g\n", which, collision.edges, area);
    return 0;
}
"""


class CollisionHullTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not SOURCE.exists():
            raise unittest.SkipTest("prepare the decomp source with tools/setup.sh (no ROM needed)")
        compiler = shutil.which(os.environ.get("CC", "cc"))
        if not compiler:
            raise unittest.SkipTest("a C compiler with UBSan is required")
        source = SOURCE.read_text()
        start = source.index("void sub_GAME_7F03ECC0(")
        end = source.index("\nvoid sub_GAME_7F03F540(", start)
        function = source[start:end]
        if "GE_COLLISION_POLY_POINT(poly, cnt)" not in function:
            raise AssertionError("production hull builder still indexes rect4f.points directly")
        cls.directory = tempfile.TemporaryDirectory(prefix=".collision-hull-", dir=ROOT)
        cls.addClassCleanup(cls.directory.cleanup)
        directory = Path(cls.directory.name)
        (directory / "hull.inc").write_text(function)
        (directory / "test.c").write_text(HARNESS)
        cls.binary = directory / "test"
        cls.legacy_binary = directory / "test-legacy"
        version = subprocess.run(
            [compiler, "--version"], capture_output=True, text=True
        ).stdout.lower()
        sanitizer = (
            "-fsanitize=undefined,bounds-strict"
            if "gcc" in version or "free software foundation" in version
            else "-fsanitize=undefined,array-bounds"
        )
        result = subprocess.run(
            [compiler, "-std=c11", "-O1", "-g", "-DGE_PORT_NATIVE",
             sanitizer, "-fno-sanitize-recover=all",
             str(directory / "test.c"), "-lm", "-o", str(cls.binary)],
            capture_output=True, text=True,
        )
        if result.returncode:
            raise AssertionError(result.stderr)
        legacy = subprocess.run(
            [compiler, "-std=c11", "-O1", "-g", "-DGE_PORT_NATIVE",
             "-DLEGACY_RECT4_ACCESS", sanitizer, "-fno-sanitize-recover=all",
             str(directory / "test.c"), "-lm", "-o", str(cls.legacy_binary)],
            capture_output=True, text=True,
        )
        if legacy.returncode:
            raise AssertionError(legacy.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), str(case)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_collapsed_point_does_not_overflow(self):
        self.run_case(0)

    def test_flat_box_keeps_the_late_candidate(self):
        self.run_case(1)

    def test_four_extrema_and_translation(self):
        self.run_case(2)

    def test_rotated_volume(self):
        self.run_case(3)

    def test_collapsed_line(self):
        self.run_case(4)

    def test_legacy_rect4_extent_is_ubsan_failure(self):
        result = subprocess.run(
            [str(self.legacy_binary), "3"], capture_output=True, text=True
        )
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("out of bounds", result.stderr.lower())

    def test_stan_los_uses_extended_polygon_accessor(self):
        source = (ROOT / "vendor/ge-decomp/src/game/stan.c").read_text()
        start = source.index("s32 stanTestLineUnobstructed(")
        end = source.index("\nPropRecord *sub_GAME_7F0B1410(", start)
        function = source[start:end]
        self.assertIn("GE_COLLISION_POLY_POINT(polygon, i)", function)
        self.assertNotIn("polygon->points[i]", function)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CollisionHullTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Collision regression tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
