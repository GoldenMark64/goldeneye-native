#!/usr/bin/env python3
"""Regression contract for Bunker 2 CCTV objective diagnostics."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
OBJECTIVE = DECOMP / "src/game/objective_status.c"
SETUP = DECOMP / "assets/obseg/setup/UsetupsevbZ.c"


def compile_tu(source: str, output: Path) -> subprocess.CompletedProcess[str]:
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


class Bunker2CctvObjectiveDiagnosticTests(unittest.TestCase):
    def test_authored_objective_tracks_six_cctv_tags(self):
        source = SETUP.read_text()
        start = source.index("/* Type = ObjectiveStart; index = 12 */")
        end = source.index("/* Type = ObjectiveEnd; index = 19 */", start)
        block = source[start:end]

        for tag in range(28, 34):
            self.assertIn(
                f"_mkword(0, _mkshort(0, 25)), {tag},",
                block,
            )

        for tag in range(28, 34):
            self.assertIn(f"_mkword({tag}, 0x0001)", source)

        self.assertEqual(source.count("/* Type = Cctv;"), 6)

    def test_probe_is_narrow_read_only_and_state_bounded(self):
        source = OBJECTIVE.read_text()
        self.assertIn('getenv("GETV_BUNKER2_CCTV_DEBUG")', source)
        self.assertIn("[getv][bunker2-cctv]", source)
        self.assertIn("bossGetStageNum() != LEVELID_BUNKER2", source)
        self.assertIn("objective_num != 2", source)
        self.assertIn("tag_id < 28", source)
        self.assertIn("tag_id > 33", source)
        self.assertIn("last->damage_lines < 8", source)
        self.assertIn(
            "ge_bunker2_cctv_debug_destroy(objectiveNum, objective->ObjRefID, obj);",
            source,
        )

        start = source.index("static void ge_bunker2_cctv_debug_destroy")
        end = source.index("#endif", start)
        probe = source[start:end]

        for field in (
            "tagrec=%p",
            "tagged=%p",
            "lookup=%p",
            "type=%d",
            "model=%d",
            "pad=%d",
            "prop=%p",
            "proptype=%d",
            "runtime=0x%x",
            "state=0x%x",
            "damage=%.3f",
            "max=%.3f",
            "destroyed=%d",
            "healthy=%d",
        ):
            self.assertIn(field, probe)

        for forbidden in (
            "tagged->state =",
            "tagged->damage =",
            "tagged->maxdamage =",
            "tagged->runtime_bitflags =",
            "objectiveStatuses[",
        ):
            self.assertNotIn(forbidden, probe)

    def test_destroy_object_decision_is_unchanged(self):
        source = OBJECTIVE.read_text()
        start = source.index("case PROPDEF_OBJECTIVE_DESTROY_OBJECT:")
        end = source.index("case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:", start)
        block = source[start:end]
        self.assertIn("ObjectRecord *obj = objFindByTagId(objective->ObjRefID);", block)
        self.assertIn("if (obj && obj->prop && objIsHealthy(obj))", block)
        self.assertIn("currentstatus = OBJECTIVESTATUS_INCOMPLETE;", block)

    def test_objective_translation_unit_compiles_native(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as td:
            obj = Path(td) / "objective_status.o"
            result = compile_tu("src/game/objective_status.c", obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        Bunker2CctvObjectiveDiagnosticTests
    )
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Bunker 2 CCTV objective diagnostic tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
