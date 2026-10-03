#!/usr/bin/env python3
"""ROM-free regression for reverse animation frame interpolation."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
SOURCE = DECOMP / "src/game/model.c"


def compiler() -> str:
    cc = shutil.which("clang") or shutil.which("gcc")
    if not cc:
        raise unittest.SkipTest("a C compiler is required")
    return cc


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


def native_compile_command(source: str, output: Path) -> list[str]:
    cc = compiler()
    warning_flags = ["-Werror=return-type"]
    if "clang" in Path(cc).name:
        warning_flags = ["-Wno-everything", "-Werror=return-type", "-ferror-limit=0"]

    return [
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


class ReverseAnimationInterpolationNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text()

    def test_production_functions_keep_fraction_in_unit_interval(self):
        f1 = extract_function(
            self.source,
            "void modelSetAnimFrame(Model* model, f32 frame)",
        )
        f2 = extract_function(
            self.source,
            "void modelSetAnimFrame2(Model* model, f32 frame1, f32 frame2)",
        )

        harness = r'''
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bondtypes.h"

s32 floorFloatToInt(f32 x) { return (s32)floorf(x); }
s32 ceilFloatToInt(f32 x) { return (s32)ceilf(x); }
s32 modelConstrainOrWrapAnimFrame(s32 frame, ModelAnimation *anim, f32 endframe)
{
    s32 end = (s32)endframe;
    (void)anim;
    if (end <= 0) return frame;
    while (frame < 0) frame += end;
    while (frame >= end) frame -= end;
    return frame;
}

__FUNC1__
__FUNC2__

static int closef(float a, float b) { return fabsf(a - b) < 0.0001f; }

static int check_primary(float frame, float speed, int a, int b, float frac)
{
    ModelAnimation anim;
    Model model;
    memset(&anim, 0, sizeof(anim));
    memset(&model, 0, sizeof(model));
    anim.unk04 = 20;
    model.anim = &anim;
    model.endframe = 20.0f;
    model.speed = speed;
    modelSetAnimFrame(&model, frame);
    if (model.framea != a || model.frameb != b) return 0;
    if (!closef(model.unk2c, frac)) return 0;
    if (!(model.unk2c >= 0.0f && model.unk2c <= 1.0f)) return 0;
    return closef(model.animframe1, frame);
}

static int check_secondary(float frame, float speed, int a, int b, float frac)
{
    ModelAnimation anim1;
    ModelAnimation anim2;
    Model model;
    memset(&anim1, 0, sizeof(anim1));
    memset(&anim2, 0, sizeof(anim2));
    memset(&model, 0, sizeof(model));
    anim1.unk04 = 20;
    anim2.unk04 = 20;
    model.anim = &anim1;
    model.anim2 = &anim2;
    model.endframe = 20.0f;
    model.unk6c = 20.0f;
    model.speed = 1.0f;
    model.speed2 = speed;
    modelSetAnimFrame2(&model, 3.5f, frame);
    if (model.frame2a != a || model.frame2b != b) return 0;
    if (!closef(model.unk5c, frac)) return 0;
    if (!(model.unk5c >= 0.0f && model.unk5c <= 1.0f)) return 0;
    return closef(model.animframe2, frame);
}

int main(void)
{
    if (!check_primary(10.25f,  1.0f, 10, 11, 0.25f)) return 10;
    if (!check_primary(10.25f, -1.0f, 11, 10, 0.75f)) return 11;
    if (!check_primary(10.00f, -1.0f, 10,  9, 0.00f)) return 12;
    if (!check_primary( 0.25f, -1.0f,  1,  0, 0.75f)) return 13;
    if (!check_primary(19.75f, -1.0f,  0, 19, 0.25f)) return 14;

    if (!check_secondary(10.25f,  1.0f, 10, 11, 0.25f)) return 20;
    if (!check_secondary(10.25f, -1.0f, 11, 10, 0.75f)) return 21;
    if (!check_secondary(10.00f, -1.0f, 10,  9, 0.00f)) return 22;
    if (!check_secondary( 0.25f, -1.0f,  1,  0, 0.75f)) return 23;
    if (!check_secondary(19.75f, -1.0f,  0, 19, 0.25f)) return 24;

    puts("PASS");
    return 0;
}
'''
        harness = harness.replace("__FUNC1__", f1).replace("__FUNC2__", f2)

        with tempfile.TemporaryDirectory(prefix="ge-reverse-anim-", dir=DECOMP) as td:
            td_path = Path(td)
            cfile = td_path / "probe.c"
            exe = td_path / "probe"
            cfile.write_text(harness)
            result = subprocess.run(
                [
                    compiler(),
                    "-fms-extensions",
                    "-I", ".", "-I", "include", "-I", "include/PR",
                    "-I", "src", "-I", "src/game",
                    "-DGE_PORT_NATIVE", "-D_LANGUAGE_C=1",
                    str(cfile), "-lm", "-o", str(exe),
                ],
                cwd=DECOMP,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("PASS", run.stdout)

    def test_native_reverse_bracketing_uses_ceiling(self):
        for signature in (
            "void modelSetAnimFrame(Model* model, f32 frame)",
            "void modelSetAnimFrame2(Model* model, f32 frame1, f32 frame2)",
        ):
            fn = extract_function(self.source, signature)
            native = fn.index("#ifdef GE_PORT_NATIVE")
            ceilpos = fn.index("ceilFloatToInt", native)
            elsepos = fn.index("#else", ceilpos)
            floorpos = fn.index("floorFloatToInt", elsepos)
            self.assertLess(native, ceilpos)
            self.assertLess(ceilpos, elsepos)
            self.assertLess(elsepos, floorpos)

    def test_model_translation_unit_compiles_native(self):
        with tempfile.TemporaryDirectory(prefix="ge-reverse-anim-tu-") as td:
            obj = Path(td) / "model.o"
            result = subprocess.run(
                native_compile_command("src/game/model.c", obj),
                cwd=DECOMP,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(obj.exists())


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        ReverseAnimationInterpolationNativeTests
    )
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("Reverse animation tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
