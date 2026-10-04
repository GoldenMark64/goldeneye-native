"""ROM-free compile and contract checks for GETV_TRAIN_DOOR_DEBUG."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
CHRACTION = DECOMP / "src/game/chraction.c"
CHRAI = DECOMP / "src/game/chrai.c"
PROPOBJ = DECOMP / "src/game/propobj.c"
AICMDS = DECOMP / "src/aicommands.def"


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


class TrainDoorDiagnosticTests(unittest.TestCase):
    def test_native_translation_units_compile(self):
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            for source in ("src/game/chraction.c", "src/game/chrai.c", "src/game/propobj.c"):
                obj = td_path / (Path(source).stem + ".o")
                result = compile_tu(source, obj)
                self.assertEqual(
                    result.returncode, 0,
                    f"{source}\n{result.stdout}{result.stderr}",
                )
                self.assertTrue(obj.exists())

    def test_probe_is_opt_in_and_read_only_at_guard_door_boundary(self):
        source = CHRACTION.read_text()
        self.assertIn('getenv("GETV_TRAIN_DOOR_DEBUG")', source)
        self.assertIn("GE_TRAIN_DOOR_TAG_FIRST 0x1e", source)
        self.assertIn("GE_TRAIN_DOOR_TAG_LAST  0x28", source)
        for marker in (
            "[getv][train-door] family",
            "[getv][train-door] travel",
            "[getv][train-door] contact",
            "[getv][train-door] activate-before",
            "[getv][train-door] activate-after",
        ):
            self.assertIn(marker, source)

        contact = source[source.index("[getv][train-door] contact") - 700:
                         source.index("[getv][train-door] contact") + 2200]
        self.assertIn("ge_train_door_debug_tag_for_obj(obj)", contact)
        self.assertIn("doorActivate(phi_s3->door, 1)", contact)

        propobj = PROPOBJ.read_text()
        self.assertIn('getenv("GETV_TRAIN_DOOR_DEBUG")', propobj)
        self.assertIn("[getv][train-door] player-locked", propobj)
        player_locked = propobj[propobj.index("[getv][train-door] player-locked") - 1200:
                                propobj.index("[getv][train-door] player-locked") + 1800]
        self.assertIn("door->keyflags", player_locked)
        self.assertIn("doorIsPadlockFree(door)", player_locked)
        self.assertIn("door->linkedDoor", player_locked)

    def test_ai_probe_watches_full_family_in_active_and_builder_paths(self):
        chrai = CHRAI.read_text()
        aicmds = AICMDS.read_text()
        self.assertIn('getenv("GETV_TRAIN_DOOR_DEBUG")', chrai)
        self.assertIn("[getv][train-door] ai-watch tag=0x%02x", chrai)
        self.assertIn("[getv][train-door] unlock tag=0x%02x", chrai)

        ifdoor_start = chrai.rindex("case AI_IFDoorStateEqual:")
        active_ifdoor = chrai[ifdoor_start:
                              chrai.index("case AI_IFDoorHasBeenOpenedBefore:", ifdoor_start)]
        self.assertIn("ge_train_door_debug_watch(AiListp, door, ai->OBJECT_TAG, pass)", active_ifdoor)

        unlock_start = chrai.rindex("case AI_DoorUnsetLock:")
        active_unlock = chrai[unlock_start:
                              chrai.index("case AI_IFDoorLockEqual:", unlock_start)]
        self.assertIn("door->keyflags &= ~bits", active_unlock)
        self.assertIn("ge_train_door_debug_unlock(AiListp, door, ai->OBJECT_TAG, bits, before)", active_unlock)

        ifdoor = aicmds[aicmds.index("IF DOOR STATE EQUAL"):
                       aicmds.index("DOOR LOCK", aicmds.index("IF DOOR STATE EQUAL"))]
        self.assertIn("GE_TRAIN_DOOR_TAG_FIRST", ifdoor)
        self.assertIn("ge_train_door_debug_watch(AiListp, door, ai->val[0], pass)", ifdoor)
        self.assertIn("door->keyflags &= ~bits", ifdoor)
        self.assertIn("ge_train_door_debug_unlock(AiListp, door, ai->val[0], bits, before)", ifdoor)

    def test_brake_objective_trigger_guards_are_instrumented(self):
        chrai = CHRAI.read_text()
        setup = (DECOMP / "assets/obseg/setup/u/UsetuptraZ.c").read_text()
        stubs = (ROOT / "getv/port/src/ge_link_stubs.c").read_text()

        for marker in (
            "[getv][train-door] trigger-objective",
            "[getv][train-door] trigger-jump",
            "[getv][train-door] trigger-run",
        ):
            self.assertIn(marker, chrai)
        self.assertIn("GE_TRAIN_TRIGGER_GUARD_A 66", chrai)
        self.assertIn("GE_TRAIN_TRIGGER_GUARD_B 68", chrai)
        self.assertIn("ge_train_trigger_debug_jump(AiListp, ChrEntityp, AI_LIST_ID)", chrai)
        self.assertIn("ge_train_trigger_debug_run(AiListp, ChrEntityp, started)", chrai)
        self.assertIn("ge_train_trigger_debug_objective(AiListp, ChrEntityp, objective_status, objective_complete)", chrai)

        # Objective 0 is six tagged brake units; chr 66/68 wait in ai_16 for it,
        # then jump (byte-reversed source literal 0x0904) into local ai_8.
        for tag in range(8, 14):
            self.assertIn(f"_mkword({tag}, 0x0001)", setup)
        self.assertIn("_mkword(66, 83), _mkword(4, 1041)", setup)
        self.assertIn("_mkword(68, 82), _mkword(4, 1041)", setup)
        self.assertIn("_mkword(36, 0x0001)", setup)
        self.assertIn("_mkword(162, 82)", setup)
        self.assertIn("if_objective_num_complete(0x00, 0x08)", setup)
        self.assertIn("jump_to_ai_list(0xfd, 0x0904)", setup)
        self.assertIn("guard_try_running_to_bond_position(0x11)", setup)
        self.assertIn("long objectiveGetStatus_WEAK(void)", stubs)
        objective_start = chrai.rindex("case AI_IFObjectiveNumComplete:")
        objective_case = chrai[objective_start:
                               chrai.index("case AI_TRYUnknown6e:", objective_start)]
        self.assertIn("#ifdef GE_PORT_NATIVE", objective_case)
        self.assertIn("objective_status = get_status_of_objective(ai->OBJ_NUM);", objective_case)
        self.assertIn("objective_status = objectiveGetStatus_WEAK(ai->OBJ_NUM * 1, ai->OBJ_NUM);", objective_case)

    def test_train_setup_contract_still_contains_progression_watch(self):
        setup = (DECOMP / "assets/obseg/setup/u/UsetuptraZ.c").read_text()
        for tag in range(0x1e, 0x29):
            self.assertIn(f"if_door_state_equal(0x{tag:02x}, 0x01, 0x35)", setup)
            self.assertIn(f"door_unset_lock(0x{tag:02x}, 0x02)", setup)
        self.assertIn("{ &UsetuptraZ_ai_38, 0x0000100a }", setup)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TrainDoorDiagnosticTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Train door diagnostic tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
