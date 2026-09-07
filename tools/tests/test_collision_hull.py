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
struct rect4f { struct { float x, y; } points[8]; };
struct collision_data { int edges; };
#include "hull.inc"

int main(int argc, char **argv)
{
    Mtxf m = {0};
    struct rect4f poly = {0};
    struct collision_data collision = {0};
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
    sub_GAME_7F03ECC0(-x,x,-y,y,-z,z,&m,&poly,&collision);
    if (collision.edges < 4 || collision.edges > 8) return 2;
    for (int i = 0; i < collision.edges; ++i) {
        int j = (i + 1) % collision.edges;
        area += (double)poly.points[i].x * poly.points[j].y
              - (double)poly.points[j].x * poly.points[i].y;
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
        compiler = shutil.which(os.environ.get("CC", "clang"))
        if not compiler:
            raise unittest.SkipTest("a C compiler with UBSan is required")
        source = SOURCE.read_text()
        start = source.index("void sub_GAME_7F03ECC0(")
        end = source.index("\nvoid sub_GAME_7F03F540(", start)
        cls.directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.directory.cleanup)
        directory = Path(cls.directory.name)
        (directory / "hull.inc").write_text(source[start:end])
        (directory / "test.c").write_text(HARNESS)
        cls.binary = directory / "test"
        result = subprocess.run(
            [compiler, "-std=c11", "-O1", "-g", "-DGE_PORT_NATIVE",
             "-fsanitize=undefined", "-fno-sanitize-recover=all",
             str(directory / "test.c"), "-lm", "-o", str(cls.binary)],
            capture_output=True, text=True,
        )
        if result.returncode:
            raise AssertionError(result.stderr)

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


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CollisionHullTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Collision regression tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
