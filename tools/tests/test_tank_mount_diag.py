#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
BONDVIEW2 = DECOMP / "src/game/bondview2.c"
PROPOBJ = DECOMP / "src/game/propobj.c"


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


class TankMountDiagnosticTests(unittest.TestCase):
    def test_native_translation_unit_compiles(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as td:
            obj = Path(td) / "bondview2.o"
            result = compile_tu("src/game/bondview2.c", obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())

    def test_native_propobj_translation_unit_compiles(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as td:
            obj = Path(td) / "propobj.o"
            result = compile_tu("src/game/propobj.c", obj)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())

    def test_native_object_collision_state_alias_fix(self):
        source = PROPOBJ.read_text()
        start = source.index("void sub_GAME_7F04F218")
        end = source.index("void sub_GAME_7F04F244", start)
        helper = source[start:end]

        native_start = helper.index("#ifdef GE_PORT_NATIVE")
        legacy_start = helper.index("#else", native_start)
        branch_end = helper.index("#endif", legacy_start)
        native = helper[native_start:legacy_start]
        legacy = helper[legacy_start:branch_end]

        self.assertIn("ObjectRecord *obj;", native)
        self.assertIn("obj = prop->obj;", native)
        self.assertIn("obj->state = (u8) obj->state & ~PROPSTATE_20;", native)
        self.assertIn("obj->state = (u8) obj->state | PROPSTATE_20;", native)
        self.assertNotIn("accuracyrating", native)

        self.assertIn("ChrRecord* chr;", legacy)
        self.assertIn("chr = prop->chr;", legacy)
        self.assertIn("chr->accuracyrating = (u8) chr->accuracyrating & ~0x20;", legacy)
        self.assertIn("chr->accuracyrating = (u8) chr->accuracyrating | 0x20;", legacy)

        self.assertIn(
            '_Static_assert(__builtin_offsetof(ObjectRecord, state)      == 1',
            source,
        )
        self.assertIn(
            '_Static_assert(__builtin_offsetof(ObjectRecord, extrascale) == 2',
            source,
        )

        def native_state_after(state: int, arg1: int) -> int:
            return (state & ~0x20) if arg1 else (state | 0x20)

        self.assertEqual(native_state_after(0x00, 0), 0x20)
        self.assertEqual(native_state_after(0xFF, 1), 0xDF)

    def test_probe_is_opt_in_and_transition_bounded(self):
        source = BONDVIEW2.read_text()
        self.assertIn('getenv("GETV_TANKDEBUG")', source)
        for marker in (
            "[getv][tank] candidate",
            "[getv][tank] discover",
            "[getv][tank] nearest",
            "[getv][tank] outer",
            "[getv][tank] outerpoly",
            "[getv][tank] innergeom",
            "[getv][tank] innerdist",
            "[getv][tank] state",
            "[getv][tank] topmove-pre",
            "[getv][tank] topmove-post",
            "[getv][tank] lost",
            "[getv][tank] btap",
            "[getv][tank] entered",
        ):
            self.assertIn(marker, source)

        state = source[source.index("[getv][tank] state") - 2600:
                       source.index("[getv][tank] state") + 2600]
        self.assertIn("tankdbg_last_outer", state)
        self.assertIn("tankdbg_last_inner", state)
        self.assertIn("tankdbg_last_canenter", state)
        self.assertIn("tankdbg_last_ybucket", state)

        self.assertIn("tankdbg_last_candidate", source)
        self.assertIn("tankdbg_candidate_initialized", source)
        self.assertIn("ge_tankdebug_nearest_tank()", source)
        self.assertIn("tankdbg_last_outer_result", source)
        self.assertIn("stanGetSignedPointLineDistance", source)
        self.assertIn("radius=%.6f contact=%.6f minedge=%.6f", source)
        self.assertIn("tank_point_hit", source)
        self.assertIn("tank_radius_hit", source)
        self.assertIn("tankdbg_last_innergeom_world", source)
        self.assertIn("(s32)(uintptr_t)tank_objrecord->collision", source)
        self.assertIn("tank_objrecord->rect.points[3]", source)
        self.assertIn("tankdbg_top_probe", source)
        self.assertIn("stanSavedColl_posData", source)
        nearest = source[source.index("static void ge_tankdebug_nearest_tank"):
                         source.index("const char *ge_cinemode_name")]
        for detail in (
            "chrpropGetActiveTail()",
            "PROPDEF_TANK",
            "chraiGetCollisionBounds(best",
            "ptr_allocated_collisiondata_block",
            "best->rooms[0]",
            "player_room",
        ):
            self.assertIn(detail, nearest)

    def test_original_tank_decision_contract_is_preserved(self):
        source = BONDVIEW2.read_text()

        discovery = source[source.index("s32 bondviewTryMoveToStan"):
                           source.index("s32 bondviewTrySimpleMovePlayerCollision")]
        self.assertIn("g_PlayerTankProp == NULL", discovery)
        self.assertIn("stanSavedColl_posData->type == PROP_TYPE_OBJ", discovery)
        self.assertIn("tank->type == PROPDEF_TANK", discovery)
        self.assertIn("g_WorldTankProp = stanSavedColl_posData", discovery)

        collision = source[source.index("void bondviewCalcUpdatePlayerCollision"):
                           source.index("void bondviewDeregisterPlayerRoom")]
        self.assertIn("g_BondCanEnterTank = 0", collision)
        self.assertIn("chraiGetCollisionBoundsWithoutY(g_WorldTankProp, &polygon, &edges)", collision)
        self.assertIn("g_PlayerTankProp = g_WorldTankProp", collision)
        self.assertIn("g_BondCanEnterTank = 1", collision)
        self.assertIn("g_PlayerTankYOffset += (20.0f * g_GlobalTimerDelta)", collision)

        self.assertIn("#define GE_TANK_CONTACT_EPSILON 0.01f", source)
        self.assertIn("tank_contact_radius += GE_TANK_CONTACT_EPSILON", collision)
        self.assertIn("tank_contact_radius,\n            polygon, edges", collision)

        entry = source[source.index("if (moveData.btap)"):]
        self.assertIn("g_PlayerTankProp->type == PROP_TYPE_OBJ", entry)
        self.assertIn("g_PlayerTankProp->obj->type == PROPDEF_TANK", entry)
        self.assertIn("&& g_BondCanEnterTank", entry)
        self.assertIn("g_PlayerIsInTank = 1", entry)

    def test_probe_reads_state_only(self):
        source = BONDVIEW2.read_text()
        blocks: list[str] = []
        cursor = 0
        needle = "if (ge_tankdebug()) {"
        while True:
            start = source.find(needle, cursor)
            if start < 0:
                break
            brace = source.index("{", start)
            depth = 0
            end = brace
            while end < len(source):
                if source[end] == "{":
                    depth += 1
                elif source[end] == "}":
                    depth -= 1
                    if depth == 0:
                        end += 1
                        break
                end += 1
            blocks.append(source[start:end])
            cursor = end

        self.assertGreaterEqual(len(blocks), 5)
        joined = "\n".join(blocks)
        for assignment in (
            "g_BondCanEnterTank =",
            "g_PlayerIsInTank =",
            "g_PlayerTankYOffset +=",
            "g_PlayerTankYOffset =",
            "g_WorldTankProp =",
            "g_PlayerTankProp =",
        ):
            self.assertNotIn(assignment, joined)

    def test_native_tank_contact_tolerance_is_small_and_local(self):
        source = BONDVIEW2.read_text()
        match = re.search(r"#define GE_TANK_CONTACT_EPSILON ([0-9.]+)f", source)
        self.assertIsNotNone(match)
        epsilon = float(match.group(1))

        self.assertGreater(epsilon, 0.0065)
        self.assertLessEqual(epsilon, 0.01)

        collision = source[source.index("void bondviewCalcUpdatePlayerCollision"):
                           source.index("void bondviewDeregisterPlayerRoom")]
        self.assertEqual(collision.count("GE_TANK_CONTACT_EPSILON"), 1)

        propobj = (DECOMP / "src/game/propobj.c").read_text()
        helper = propobj[propobj.index("s32 chrobjTestPointPolygonCollision"):
                         propobj.index("s32 sub_GAME_7F0448A8")]
        self.assertIn("temp_f0 < collision_radius", helper)
        self.assertNotIn("GE_TANK_CONTACT_EPSILON", helper)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TankMountDiagnosticTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Tank mount diagnostic tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
