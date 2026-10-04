#!/usr/bin/env python3
"""ROM-free regression for native NPC movement/weapon animation selection."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
SOURCE = DECOMP / "src/game/chraction.c"


def extract_function(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"function not found: {signature}")

    opening = source.find("{", start + len(signature))
    if opening < 0:
        raise AssertionError(f"opening brace not found: {signature}")

    depth = 0
    state = "code"
    i = opening

    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""

        if state == "code":
            if ch == "/" and nxt == "*":
                state = "block"
                i += 2
                continue
            if ch == "/" and nxt == "/":
                state = "line"
                i += 2
                continue
            if ch == '"':
                state = "string"
            elif ch == "'":
                state = "char"
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return source[start:i + 1]
        elif state == "block":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 2
                continue
        elif state == "line":
            if ch == "\n":
                state = "code"
        elif state in ("string", "char"):
            if ch == "\\":
                i += 2
                continue
            if (state == "string" and ch == '"') or (state == "char" and ch == "'"):
                state = "code"

        i += 1

    raise AssertionError(f"closing brace not found: {signature}")


def native_compile_command(compiler: str, source: str, output: str) -> list[str]:
    warning_flags = ["-Werror=return-type"]
    if "clang" in Path(compiler).name:
        warning_flags = ["-Wno-everything", "-Werror=return-type", "-ferror-limit=0"]

    return [
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
        source, "-o", output,
    ]


class ChrAnimationSelectionNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text()

    def test_native_layout_proves_legacy_raw_offsets_are_not_semantic_fields(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        self.assertIsNotNone(compiler, "a C compiler is required")

        probe = r'''
#include <stdio.h>
#include "bondtypes.h"

static int checks;
static int failures;

static void check(int condition, const char *label)
{
    checks++;
    printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    failures += !condition;
}

int main(void)
{
    size_t raw_speed = __builtin_offsetof(ChrRecord, act_ubytes) + 45u;
    size_t named_speed = __builtin_offsetof(ChrRecord, act_gopos)
                       + __builtin_offsetof(struct act_gopos, unk59);
    size_t raw_weapon = __builtin_offsetof(ChrRecord, act_bytes) + 84u;
    size_t named_weapon = __builtin_offsetof(WeaponObjRecord, weaponnum);

    if (sizeof(void *) != 8) {
        fputs("FAIL regression requires a 64-bit native ABI\n", stderr);
        return 2;
    }

    printf("raw_speed=%zu named_speed=%zu raw_weapon=%zu named_weapon=%zu\n",
           raw_speed, named_speed, raw_weapon, named_weapon);
    check(raw_speed != named_speed,
          "legacy movement byte alias misses act_gopos.unk59 on native");
    check(raw_weapon != named_weapon,
          "legacy weapon byte alias misses WeaponObjRecord.weaponnum on native");
    printf("%d checks, %d failures\n", checks, failures);

    return failures != 0;
}
'''

        with tempfile.TemporaryDirectory(
            prefix=".ge-chr-animation-layout-",
            dir=DECOMP,
        ) as td:
            td_path = Path(td)
            probe_c = td_path / "probe.c"
            exe = td_path / "probe"
            probe_c.write_text(probe)

            cmd = native_compile_command(compiler, str(probe_c), str(exe))
            result = subprocess.run(cmd, cwd=DECOMP, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            result = subprocess.run([str(exe)], capture_output=True, text=True)
            output = result.stdout + result.stderr
            self.assertEqual(result.returncode, 0, output)
            self.assertIn("2 checks, 0 failures", output)

    def test_native_weapon_classifier_uses_weapon_record_field(self):
        fn = extract_function(
            self.source,
            "u32 weaponIsOneHanded(PropRecord *arg0)",
        )
        self.assertIn("#ifdef GE_PORT_NATIVE", fn)
        self.assertIn("WeaponObjRecord *weapon = arg0->weapon;", fn)
        self.assertIn("weapon->weaponnum", fn)
        self.assertIn("#else", fn)
        self.assertIn("v->act_bytes.padding[84]", fn)

        native_pos = fn.index("#ifdef GE_PORT_NATIVE")
        weapon_pos = fn.index("weapon->weaponnum")
        else_pos = fn.index("#else")
        legacy_pos = fn.index("v->act_bytes.padding[84]")
        self.assertLess(native_pos, weapon_pos)
        self.assertLess(weapon_pos, else_pos)
        self.assertLess(else_pos, legacy_pos)

    def test_native_movement_animation_uses_named_speed_field(self):
        fn = extract_function(
            self.source,
            "void play_hit_soundeffect_and_proper_volume( ChrRecord *self)",
        )
        self.assertIn("#ifdef GE_PORT_NATIVE", fn)
        self.assertIn("self->act_gopos.unk59", fn)
        self.assertIn("#else", fn)
        self.assertIn("self->act_ubytes.padding[45]", fn)

        native_pos = fn.index("#ifdef GE_PORT_NATIVE")
        speed_pos = fn.index("self->act_gopos.unk59")
        else_pos = fn.index("#else")
        legacy_pos = fn.index("self->act_ubytes.padding[45]")
        self.assertLess(native_pos, speed_pos)
        self.assertLess(speed_pos, else_pos)
        self.assertLess(else_pos, legacy_pos)

    def test_full_chraction_translation_unit_compiles_native(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        self.assertIsNotNone(compiler, "a C compiler is required")

        with tempfile.TemporaryDirectory(prefix="ge-chr-animation-tu-") as td:
            obj = Path(td) / "chraction.o"
            cmd = native_compile_command(
                compiler,
                "src/game/chraction.c",
                str(obj),
            )
            cmd.insert(-2, "-c")
            result = subprocess.run(cmd, cwd=DECOMP, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        ChrAnimationSelectionNativeTests
    )
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("NPC animation regression tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
