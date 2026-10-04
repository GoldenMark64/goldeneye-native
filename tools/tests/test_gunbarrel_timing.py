#!/usr/bin/env python3
"""ROM-free regression for corrected native gunbarrel timing.

The native pre-shot sequence may advance at a fractional authored cadence while rendering every
host frame.  After Bond fires, the title state returns to one step per render. In particular,
case 3 must decode a fresh blood frame every two renders so its dyn-buffer lifetime stays aligned
with GoldenEye's two alternating graphics arenas.

Bond's model animation keeps the original platform split per authored sequence step: NTSC advances
twice and PAL once. Native skipped pre-shot authored steps therefore render Bond with zero model
ticks on that host frame, preserving Bond/barrel synchronization.

Native builds may override only the gunbarrel walk-animation rate with
GETV_GUNBARREL_BOND_SPEED for final visual calibration.  The default remains the retail 0.91,
and non-native builds keep the literal retail call unchanged.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TITLE = ROOT / "vendor/ge-decomp/src/game/title.c"

HARNESS_PREFIX = r"""
#include <stdint.h>
#include <stdio.h>

typedef int32_t s32;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint8_t u8;
typedef float f32;
typedef struct Gfx { int dummy; } Gfx;

u32 D_8002A7D0;
u8 gunbarrel_mode;
f32 g_TitleX;
f32 titleTransitionX;
s16 word_CODE_bss_80069584;
s32 intro_eye_counter;
u32 intro_state_blood_animation;
s32 gunbarrelTimer;

#define BOND_EYE_FIRE_SHOT 230

#ifdef GE_PORT_NATIVE
static f32 geGunbarrelSequenceAccumulator;
static f32 geGunbarrelSequenceSpeed = 1.0f;
static s32 geGunbarrelSequenceAdvance = 1;
#endif

static Gfx dlBasicGeometry;
static int blood_calls;

#define gSPDisplayList(pkt, dl) ((void)(pkt), (void)(dl))

static Gfx *manipulateGunbarrelAndLogoMatrices(Gfx *gdl) { return gdl; }
static Gfx *clear_framebuffer_black(Gfx *gdl) { return gdl; }
static Gfx *insert_sniper_sight_eye_intro(Gfx *gdl) { return gdl; }
static Gfx *insert_sight_backdrop_eye_intro(Gfx *gdl) { return gdl; }
static Gfx *insert_bond_eye_intro(Gfx *gdl) { return gdl; }
static Gfx *gunbarrelBloodOverlayDL(Gfx *gdl) { return gdl; }
static Gfx *sub_GAME_7F01CA18(Gfx *gdl) { return gdl; }
static Gfx *sub_GAME_7F007E70(Gfx *gdl, u32 alpha) { (void)alpha; return gdl; }
static s16 sins(unsigned short x) { (void)x; return 0; }
static s32 die_blood_image_routine(s32 mode)
{
    if (mode == 1) blood_calls++;
    return 0;
}
"""

HARNESS_SUFFIX = r"""
static int failures;

static void check(int condition, const char *message)
{
    if (condition) {
        printf("PASS %s\n", message);
    } else {
        failures++;
        printf("FAIL %s\n", message);
    }
}

int main(void)
{
    Gfx dl[64];
    int i;

    /* Mode 2 / case 0: state progression is once per rendered intro frame. */
    gunbarrel_mode = 2;
    g_TitleX = -30.0f;
    titleTransitionX = -100.0f;
    word_CODE_bss_80069584 = 0x42;
    intro_eye_counter = 0;

    for (i = 0; i < 10; i++) {
        (void)renderGunbarrelEyeIntroSequence(dl);
    }

    check(g_TitleX > 29.99f && g_TitleX < 30.01f,
          "10 renders advance 10 title movement steps");

#ifdef GE_PORT_NATIVE
    /* 9/13 is the first calibration derived from measured native/N64 wall time. */
    gunbarrel_mode = 2;
    gunbarrelTimer = 0;
    g_TitleX = -30.0f;
    titleTransitionX = -100.0f;
    word_CODE_bss_80069584 = 0x42;
    geGunbarrelSequenceAccumulator = 0.0f;
    geGunbarrelSequenceSpeed = 0.692308f;

    for (i = 0; i < 13; i++) {
        (void)renderGunbarrelEyeIntroSequence(dl);
    }

    check(g_TitleX > 23.99f && g_TitleX < 24.01f,
          "13 host renders at 9/13 cadence advance 9 authored title steps");
    geGunbarrelSequenceSpeed = 1.0f;
#endif

    /*
     * Mode 5 / case 3: with a two-buffer dyn arena, decode every second render.
     * A four-render interval reuses the owning arena once without refreshing the image first
     * and is the shimmer regression this test is meant to prevent.
     */
    gunbarrel_mode = 5;
    intro_eye_counter = 2;
    intro_state_blood_animation = 0;
    blood_calls = 0;

    for (i = 0; i < 4; i++) {
        (void)renderGunbarrelEyeIntroSequence(dl);
    }

    check(blood_calls == 2,
          "4 renders decode 2 blood frames (one every 2 renders)");

    printf("%d failures\n", failures);
    return failures != 0;
}
"""


def extract_title_state(source: str) -> str:
    constants_start = source.index("#ifndef REFRESH_PAL\n    #define XINC")
    function_end = source.index("\n\ns32 isGunBarrelInMode9", constants_start)
    return source[constants_start:function_end].rstrip() + "\n"


def extract_native_walk_speed_helper(source: str) -> str:
    start = source.index("static f32 geGunbarrelBondWalkSpeed")
    end = source.index("\n}\n", start) + 3
    return source[start:end].rstrip() + "\n"


def animation_source_contract(source: str) -> list[str]:
    errors: list[str] = []

    if "geGunbarrelAuthoredStep" in source:
        errors.append("obsolete blanket geGunbarrelAuthoredStep gate remains in title.c")

    pattern = re.compile(
        r"#if\s+defined\s+REFRESH_PAL\s*"
        r"return\s+sub_GAME_7F007F30\(gdl,\s*1,\s*&matrix\);\s*"
        r"#elif\s+defined\s+GE_PORT_NATIVE\s*"
        r"return\s+sub_GAME_7F007F30\(gdl,\s*geGunbarrelSequenceAdvance\s*\?\s*2\s*:\s*0,\s*&matrix\);\s*"
        r"#else\s*"
        r"return\s+sub_GAME_7F007F30\(gdl,\s*2,\s*&matrix\);\s*"
        r"#endif",
        re.S,
    )
    if not pattern.search(source):
        errors.append(
            "Bond animation cadence no longer preserves PAL=1, retail NTSC=2, native authored-step=2"
        )

    if 'GETV_GUNBARREL_SEQUENCE_SPEED' not in source:
        errors.append("native pre-shot gunbarrel sequence-speed calibration override is missing")

    if 'GETV_GUNBARREL_BOND_SPEED' not in source:
        errors.append("native gunbarrel Bond walk-speed calibration override is missing")

    helper_pattern = re.compile(
        r"static\s+f32\s+geGunbarrelBondWalkSpeed\s*\(void\).*?"
        r"getenv\(\"GETV_GUNBARREL_BOND_SPEED\"\).*?"
        r"return\s+0\.91f;.*?"
        r"atof\(value\).*?"
        r"speed\s*<\s*0\.25f.*?speed\s*>\s*1\.50f.*?"
        r"return\s+speed;\s*}",
        re.S,
    )
    if not helper_pattern.search(source):
        errors.append("native Bond speed override parser/default/range contract changed")

    call_pattern = re.compile(
        r"#ifdef\s+GE_PORT_NATIVE\s*"
        r"modelSetAnimation\(chrModelInstance,.*?gunbarrelBondWalkSpeed,\s*0\.0f\);\s*"
        r"#else\s*"
        r"modelSetAnimation\(chrModelInstance,.*?0\.91f,\s*0\.0f\);\s*"
        r"#endif",
        re.S,
    )
    if not call_pattern.search(source):
        errors.append(
            "walk-speed override is not native-only with retail 0.91 preserved outside native"
        )

    if "modelSetAnimTranslationScale(chrModelInstance, 0.91f / gunbarrelBondWalkSpeed);" not in source:
        errors.append(
            "native Bond walk-speed calibration no longer compensates root translation"
        )

    reset_pattern = re.compile(
        r"if\s*\(gunbarrelTimer\s*==\s*BOND_EYE_ANIM_START\).*?"
        r"#ifdef\s+GE_PORT_NATIVE.*?"
        r"modelSetAnimTranslationScale\(chrModelInstance,\s*1\.0f\);.*?"
        r"#endif.*?"
        r"modelSetAnimation\(chrModelInstance,.*?GE_ANIMOFF_ANIM_DATA_bond_eye_fire",
        re.S,
    )
    if not reset_pattern.search(source):
        errors.append(
            "native gunbarrel walk translation compensation is not reset before turn/fire"
        )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--title-source", type=Path, default=TITLE)
    args = parser.parse_args()

    source = args.title_source.read_text()
    errors = animation_source_contract(source)
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1

    compiler = shutil.which(os.environ.get("CC", "gcc"))
    if compiler is None:
        raise SystemExit("C compiler not found")

    with tempfile.TemporaryDirectory(prefix="ge-gunbarrel-timing-", dir=ROOT / "scratch") as td:
        td = Path(td)
        cfile = td / "test.c"
        cfile.write_text(HARNESS_PREFIX + "\n" + extract_title_state(source) + "\n" + HARNESS_SUFFIX)

        for name, defines in (
            ("native", ["-DGE_PORT_NATIVE"]),
            ("retail-layout", []),
        ):
            exe = td / f"test_gunbarrel_timing_{name}"
            build = subprocess.run(
                [compiler, "-std=gnu17", "-O1", "-Wall", "-Wextra", "-Werror",
                 *defines, str(cfile), "-o", str(exe)],
                capture_output=True, text=True,
            )
            if build.returncode:
                print(build.stdout + build.stderr, end="")
                return build.returncode

            print(f"--- {name} title-state behavior ---")
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            print(run.stdout + run.stderr, end="")
            if run.returncode:
                return run.returncode

        helper_c = td / "test_walk_speed.c"
        helper_c.write_text(
            "#include <math.h>\n"
            "#include <stdlib.h>\n"
            "#include <stdio.h>\n"
            "typedef float f32;\n"
            "void osSyncPrintf(const char *fmt, ...) { (void)fmt; }\n"
            + extract_native_walk_speed_helper(source)
            + r'''
static int nearf(float a, float b) { return fabsf(a - b) < 0.00001f; }
int main(void)
{
    int failures = 0;
    unsetenv("GETV_GUNBARREL_BOND_SPEED");
    if (!nearf(geGunbarrelBondWalkSpeed(), 0.91f)) failures++;
    setenv("GETV_GUNBARREL_BOND_SPEED", "0.90", 1);
    if (!nearf(geGunbarrelBondWalkSpeed(), 0.90f)) failures++;
    setenv("GETV_GUNBARREL_BOND_SPEED", "0.88", 1);
    if (!nearf(geGunbarrelBondWalkSpeed(), 0.88f)) failures++;
    setenv("GETV_GUNBARREL_BOND_SPEED", "garbage", 1);
    if (!nearf(geGunbarrelBondWalkSpeed(), 0.91f)) failures++;
    setenv("GETV_GUNBARREL_BOND_SPEED", "nan", 1);
    if (!nearf(geGunbarrelBondWalkSpeed(), 0.91f)) failures++;
    setenv("GETV_GUNBARREL_BOND_SPEED", "0.24", 1);
    if (!nearf(geGunbarrelBondWalkSpeed(), 0.91f)) failures++;
    setenv("GETV_GUNBARREL_BOND_SPEED", "1.51", 1);
    if (!nearf(geGunbarrelBondWalkSpeed(), 0.91f)) failures++;
    printf("%d walk-speed parser failures\n", failures);
    return failures != 0;
}
'''
        )
        helper_exe = td / "test_walk_speed"
        helper_build = subprocess.run(
            [compiler, "-std=gnu17", "-O1", "-Wall", "-Wextra", "-Werror",
             str(helper_c), "-lm", "-o", str(helper_exe)],
            capture_output=True, text=True,
        )
        if helper_build.returncode:
            print(helper_build.stdout + helper_build.stderr, end="")
            return helper_build.returncode
        helper_run = subprocess.run([str(helper_exe)], capture_output=True, text=True)
        print(helper_run.stdout + helper_run.stderr, end="")
        if helper_run.returncode:
            return helper_run.returncode

    print("PASS native NTSC Bond model cadence is restored to 2 ticks per render")
    print("PASS native Bond walk-speed override parses valid values and rejects invalid values")
    print("PASS PAL Bond model cadence remains 1 tick per render")
    print("PASS retail NTSC Bond model cadence remains the original 2 ticks per render")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
