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
OBJECTIVE = DECOMP / "src/game/objective_status.c"
GUN = DECOMP / "src/game/gun.c"
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
        "-I", str(ROOT / "tools/tests/fixtures"),
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


class DepotTimedMineDiagnosticTests(unittest.TestCase):
    def test_native_translation_units_compile(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as td:
            tmp = Path(td)
            for rel in (
                "src/game/objective_status.c",
                "src/game/gun.c",
                "src/game/propobj.c",
            ):
                result = compile_tu(rel, tmp / (Path(rel).stem + ".o"))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_depot_probe_is_narrow_and_bounded(self):
        source = OBJECTIVE.read_text()

        self.assertIn('getenv("GETV_OBJ_DEBUG")', source)
        self.assertIn("[getv][depot-obj]", source)
        self.assertIn("bossGetStageNum() != LEVELID_DEPOT", source)
        self.assertIn("objective_num != 1", source)
        self.assertIn("tag_id < 1 || tag_id > 3", source)
        self.assertIn("last->damage_lines < 8", source)

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
            self.assertIn(field, source)

        self.assertIn("tag = sub_GAME_7F057080(tag_id);", source)
        self.assertIn("tagged = tag ? tag->TaggedObject : NULL;", source)
        self.assertIn("destroyed_level = tagged ? objGetDestroyedLevel(tagged) : -1;", source)
        self.assertIn("healthy = tagged ? objIsHealthy(tagged) : -1;", source)
        self.assertIn(
            "ge_depot_obj_debug_destroy(objectiveNum, objective->ObjRefID, obj);",
            source,
        )

    def test_depot_destroy_object_decision_is_unchanged(self):
        source = OBJECTIVE.read_text()
        start = source.index("case PROPDEF_OBJECTIVE_DESTROY_OBJECT:")
        end = source.index("case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:", start)
        block = source[start:end]

        self.assertIn("ObjectRecord *obj = objFindByTagId(objective->ObjRefID);", block)
        self.assertIn("if (obj && obj->prop && objIsHealthy(obj))", block)
        self.assertIn("currentstatus = OBJECTIVESTATUS_INCOMPLETE;", block)

        probe = source[source.index("static void ge_depot_obj_debug_destroy"):
                       source.index("#endif", source.index("static void ge_depot_obj_debug_destroy"))]
        for forbidden in (
            "tagged->state =",
            "tagged->damage =",
            "tagged->maxdamage =",
            "tagged->runtime_bitflags =",
            "objectiveStatuses[",
        ):
            self.assertNotIn(forbidden, probe)

    def test_timed_mine_throw_probe_covers_pre_and_post_projectile_init(self):
        source = GUN.read_text()

        self.assertIn('getenv("GETV_TIMEDMINE_DEBUG")', source)
        self.assertIn("[getv][timedmine]", source)
        self.assertIn('ge_timedmine_debug_throw("timer-set", wor);', source)
        self.assertIn('ge_timedmine_debug_throw("projectile-init", wor);', source)

        start = source.index("void generate_player_thrown_object")
        end = source.index("void gunSpawnGLGrenade", start)
        block = source[start:end]

        self.assertIn("case ITEM_TIMEDMINE:", block)
        self.assertIn("wor->timer = THROWN_ITEM_TIMER_SOLO;", block)
        self.assertIn("wor->timer = THROWN_ITEM_TIMER_MULTI;", block)
        self.assertIn("gunInitProjectileFromPlayer(wor,", block)

    def test_timed_mine_tick_probe_covers_all_decision_boundaries(self):
        source = PROPOBJ.read_text()

        self.assertIn('getenv("GETV_TIMEDMINE_DEBUG")', source)
        self.assertIn("weapon->weaponnum == ITEM_TIMEDMINE || obj->obj == PROP_CHRTIMEDMINE", source)
        for marker in (
            'ge_timedmine_debug_tick("tick-entry"',
            'ge_timedmine_debug_tick("countdown"',
            'ge_timedmine_debug_tick("explode-before"',
            'ge_timedmine_debug_tick("explode-after"',
        ):
            self.assertIn(marker, source)

        self.assertIn("state->last_bucket != bucket", source)
        self.assertIn("state->last_shuffle != shuffle", source)
        self.assertIn("state->last_objtype != obj->type", source)
        self.assertIn("state->last_proptype != proptype", source)
        self.assertIn("state->last_weaponnum != weapon->weaponnum", source)
        self.assertIn("weapon->timer <= 10", source)

        for field in (
            "tick=%d",
            "objtype=%d",
            "proptype=%d",
            "model=%d",
            "weapon=%d",
            "timer=%d",
            "clock=%d",
            "shuffle=%d",
            "runtime=0x%x",
            "projectile=%p",
            "hasprojectile=%d",
        ):
            self.assertIn(field, source)

    def test_timed_mine_gameplay_contract_is_unchanged(self):
        source = PROPOBJ.read_text()
        start = source.index("void chrobjWeaponTick")
        end = source.index("void objDropRecursively", start)
        block = source[start:end]

        self.assertIn("if (get_player_position_in_shuffled(get_cur_playernum()) != 0)", block)
        self.assertIn("if (obj->type == PROP_TYPE_SMOKE)", block)
        self.assertIn(
            "((weapon->weaponnum == ITEM_TIMEDMINE) || "
            "(weapon->weaponnum == ITEM_BOMBCASE)) && (weapon->timer >= 0)",
            block,
        )
        self.assertIn("weapon->timer -= g_ClockTimer;", block)
        self.assertIn("if (weapon->timer < 0)", block)
        self.assertIn("weapon->timer = -1;", block)
        self.assertIn("obj->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;", block)

        helper_start = source.index("static void ge_timedmine_debug_tick")
        helper_end = source.index("/* Restore the pre-fix values", helper_start)
        helper = source[helper_start:helper_end]
        for forbidden in (
            "weapon->timer -=",
            "weapon->timer =",
            "obj->state =",
            "obj->runtime_bitflags |=",
            "propExplode(",
        ):
            self.assertNotIn(forbidden, helper)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(DepotTimedMineDiagnosticTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Depot/timed-mine diagnostic tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
