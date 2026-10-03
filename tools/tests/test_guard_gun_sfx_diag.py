"""ROM-free compile check for the native guard gunshot diagnostic."""
from __future__ import annotations

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
SOURCE = DECOMP / "src/game/chraction.c"


class GuardGunSfxDiagnosticTests(unittest.TestCase):
    @staticmethod
    def _compiler():
        requested = os.environ.get("CC")
        if requested:
            return shutil.which(requested)
        return shutil.which("clang") or shutil.which("gcc")

    @staticmethod
    def _native_compile_flags(compiler):
        warning_flags = ["-Werror=return-type"]
        if "clang" in Path(compiler).name:
            warning_flags = ["-Wno-everything", "-Werror=return-type", "-ferror-limit=0"]
        return [
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
        ]

    def test_native_chraction_translation_unit_compiles(self):
        compiler = self._compiler()
        if not compiler:
            self.skipTest("a C compiler is required")
        with tempfile.TemporaryDirectory() as td:
            obj = Path(td) / "chraction.o"
            cmd = [compiler, *self._native_compile_flags(compiler),
                   "-c", "src/game/chraction.c", "-o", str(obj)]
            result = subprocess.run(cmd, cwd=DECOMP, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())

    def test_native_layout_proves_legacy_alias_differs_from_weaponnum(self):
        compiler = self._compiler()
        if not compiler:
            self.skipTest("a C compiler is required")
        probe = r'''
#include <ultra64.h>
#include "bondtypes.h"
_Static_assert(sizeof(void *) > 4, "native layout probe requires widened pointers");
_Static_assert(
    __builtin_offsetof(ChrRecord, act_attack)
        + __builtin_offsetof(struct act_attack, attack_item)
        != __builtin_offsetof(WeaponObjRecord, weaponnum),
    "legacy ChrRecord alias unexpectedly still matches WeaponObjRecord.weaponnum");
int main(void) { return 0; }
'''
        with tempfile.TemporaryDirectory() as td:
            td = Path(td)
            src = td / "guard_gun_sfx_layout.c"
            obj = td / "guard_gun_sfx_layout.o"
            src.write_text(probe)
            cmd = [compiler, *self._native_compile_flags(compiler),
                   "-c", str(src), "-o", str(obj)]
            result = subprocess.run(cmd, cwd=DECOMP, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())

    def test_diagnostic_is_native_and_debug_gated(self):
        source = SOURCE.read_text()
        marker = '[getv] guard-gun-sfx:'
        self.assertIn(marker, source)
        marker_pos = source.index(marker)
        native_pos = source.rfind("#ifdef GE_PORT_NATIVE", 0, marker_pos)
        debug_pos = source.rfind("gePortAudioDebugLevel() > 1", 0, marker_pos)
        self.assertGreater(native_pos, -1)
        self.assertGreater(debug_pos, native_pos)

    def test_native_guard_gun_sfx_reads_weaponnum_not_chr_action_alias(self):
        source = SOURCE.read_text()
        start = source.index("void sub_GAME_7F02BFE4(ChrRecord *self, s32 arg1, s32 arg2)")
        end = source.index("/**", start + 1)
        fn = source[start:end]

        native = fn.index("#ifdef GE_PORT_NATIVE")
        legacy = fn.index("#else", native)
        end_if = fn.index("#endif", legacy)

        native_branch = fn[native:legacy]
        legacy_branch = fn[legacy:end_if]

        self.assertIn("weapon_item = (s32)prop->weapon->weaponnum;", native_branch)
        self.assertNotIn("act_attack.attack_item", native_branch)
        self.assertIn("temp_v1 = prop->chr;", legacy_branch)
        self.assertIn("weapon_item = (s32)temp_v1->act_attack.attack_item;", legacy_branch)

        self.assertIn("bondwalkItemGetSoundTriggerRate(weapon_item)", fn)
        self.assertIn("bondwalkItemGetSound(weapon_item)", fn)
        self.assertIn("(int)weapon_item, (int)sp30", fn)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(GuardGunSfxDiagnosticTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Guard gun SFX diagnostic tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
