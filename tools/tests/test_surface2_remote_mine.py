#!/usr/bin/env python3
"""ROM-free regression for Surface 2's remote-mine objective lookup.

The mission asks whether ITEM_REMOTEMINE has been thrown and come to rest. On native little-endian
hosts the retail KeyRecord overlay reads WeaponObjRecord.timer instead of weaponnum, so an ordinary
grenade whose timer low byte is 0x1d can falsely satisfy the objective failure condition.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
SOURCE = DECOMP / "src/game/propobj.c"

HARNESS = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
typedef uint8_t u8;
typedef uint32_t u32;

enum {
    PROP_TYPE_WEAPON = 4,
    PROPDEF_KEY = 4,
    ITEM_GRENADE = 0x1a,
    ITEM_REMOTEMINE = 0x1d,
    RUNTIMEBITFLAG_HASPROJECTILE = 1 << 7
};

/* Preserve the native offsets that make the legacy overlay dangerous:
 * KeyRecord.keyID and WeaponObjRecord.timer both begin at byte 144, while weaponnum is byte 147.
 * runtime_bitflags only needs to share an offset between the two views for weaponFindThrown(). */
typedef struct KeyRecord {
    u32 runtime_bitflags;
    u8 prefix[140];
    s8 keyID;
} KeyRecord;

typedef struct WeaponObjRecord {
    u32 runtime_bitflags;
    u8 prefix[140];
#ifdef GE_PORT_NATIVE
    s16 timer;
    s8 LinkedWeaponType;
    s8 weaponnum;
#else
    s8 weaponnum;
    s8 LinkedWeaponType;
    s16 timer;
#endif
} WeaponObjRecord;

typedef struct PropRecord {
    u8 type;
    union {
        void *obj;
        WeaponObjRecord *weapon;
    };
    struct PropRecord *child;
    struct PropRecord *prev;
} PropRecord;

static PropRecord *active_tail;

static PropRecord *chrpropGetActiveTail(void)
{
    return active_tail;
}

#include "surface2_lookup.inc"

static int checks;
static int failures;

static void check(int condition, const char *message)
{
    checks++;
    if (condition) {
        printf("PASS %s\n", message);
    } else {
        failures++;
        printf("FAIL %s\n", message);
    }
}

int main(void)
{
    WeaponObjRecord grenade;
    WeaponObjRecord remote;
    PropRecord prop;

    memset(&grenade, 0, sizeof grenade);
    memset(&remote, 0, sizeof remote);
    memset(&prop, 0, sizeof prop);

    prop.type = PROP_TYPE_WEAPON;
    prop.weapon = &grenade;
    active_tail = &prop;

    grenade.weaponnum = ITEM_GRENADE;
    grenade.timer = ITEM_REMOTEMINE; /* low byte 0x1d: legacy KeyRecord overlay false positive */
    check(weaponFindThrown(ITEM_REMOTEMINE) == NULL,
          "grenade timer byte cannot masquerade as remote-mine item id");

    prop.weapon = &remote;
    remote.weaponnum = ITEM_REMOTEMINE;
    remote.timer = 100;
    check(weaponFindThrown(ITEM_REMOTEMINE) != NULL,
          "actual stationary remote mine is found by weapon id");

    remote.runtime_bitflags = RUNTIMEBITFLAG_HASPROJECTILE;
    check(weaponFindThrown(ITEM_REMOTEMINE) == NULL,
          "airborne remote mine is not yet stationary");

    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0 || checks != 3;
}
"""

LAYOUT = r"""
#include "bondtypes.h"

_Static_assert(sizeof(void *) > 4, "native layout probe requires widened pointers");
_Static_assert(__builtin_offsetof(KeyRecord, keyID) == 144,
               "unexpected native KeyRecord.keyID offset");
_Static_assert(__builtin_offsetof(WeaponObjRecord, timer) == 144,
               "unexpected native WeaponObjRecord.timer offset");
_Static_assert(__builtin_offsetof(WeaponObjRecord, LinkedWeaponType) == 146,
               "unexpected native WeaponObjRecord.LinkedWeaponType offset");
_Static_assert(__builtin_offsetof(WeaponObjRecord, weaponnum) == 147,
               "unexpected native WeaponObjRecord.weaponnum offset");
_Static_assert(__builtin_offsetof(KeyRecord, keyID) ==
               __builtin_offsetof(WeaponObjRecord, timer),
               "legacy key overlay must demonstrate the native timer alias");
_Static_assert(__builtin_offsetof(KeyRecord, keyID) !=
               __builtin_offsetof(WeaponObjRecord, weaponnum),
               "legacy key overlay unexpectedly still addresses weaponnum");

int main(void) { return 0; }
"""


def run(args: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True)


def extract_lookup(source: str) -> str:
    start = source.index("KeyRecord *check_if_entry_is_collectable(")
    end = source.index("void add_obj_to_temp_proxmine_table(", start)
    return source[start:end].rstrip() + "\n"


def native_header_flags(include_root: Path) -> list[str]:
    return [
        "-std=gnu17", "-fms-extensions", "-fno-strict-aliasing", "-O1",
        "-DVERSION_US", "-DLANG_US", "-DREFRESH_NTSC", "-DLEFTOVERDEBUG",
        "-DLEFTOVERSPECTRUM", "-DBUGFIX_R0", "-DTARGET_N64", "-DGE_PORT_NATIVE",
        "-DNON_MATCHING=1", "-DAVOID_UB=1", "-D_LANGUAGE_C=1",
        "-Werror=return-type", "-D_FORTIFY_SOURCE=0",
        "-I", str(include_root),
        "-I", str(DECOMP),
        "-I", str(DECOMP / "include"),
        "-I", str(DECOMP / "include/PR"),
        "-I", str(DECOMP / "src"),
        "-I", str(DECOMP / "src/game"),
        "-I", str(DECOMP / "src/inflate"),
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--source", type=Path, default=SOURCE)
    args = parser.parse_args()

    compiler = shutil.which(args.cc)
    if compiler is None:
        raise SystemExit(f"compiler not found: {args.cc}")
    if not args.source.is_file():
        raise SystemExit(f"patched decomp source not found: {args.source}")

    source = args.source.read_text()
    lookup = extract_lookup(source)

    scratch_root = ROOT / "scratch"
    scratch_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-surface2-mine-", dir=scratch_root) as temporary:
        td = Path(temporary)

        # The real headers only need the image identifier include to parse. An empty synthetic
        # definition list is sufficient for this layout-only compile and contains no game data.
        image_dir = td / "assets"
        image_dir.mkdir()
        (image_dir / "images.def").write_text("")

        layout_c = td / "layout.c"
        layout_o = td / "layout.o"
        layout_c.write_text(LAYOUT)
        layout_result = run(
            [compiler, *native_header_flags(td), "-c", str(layout_c), "-o", str(layout_o)],
            cwd=DECOMP,
        )
        if layout_result.returncode:
            print(layout_result.stdout + layout_result.stderr, end="")
            return layout_result.returncode
        print("PASS real native headers: keyID=timer offset, weaponnum differs")

        (td / "surface2_lookup.inc").write_text(lookup)
        harness_c = td / "test.c"
        harness_c.write_text(HARNESS)
        for name, defines in (
            ("native", ["-DGE_PORT_NATIVE"]),
            ("retail-layout", []),
        ):
            harness_exe = td / f"test_surface2_remote_mine_{name}"
            compile_result = run(
                [compiler, "-std=gnu17", "-O1", "-Wall", "-Wextra", "-Werror",
                 *defines, str(harness_c), "-o", str(harness_exe)],
            )
            if compile_result.returncode:
                print(compile_result.stdout + compile_result.stderr, end="")
                return compile_result.returncode

            print(f"--- {name} behavior ---")
            result = run([str(harness_exe)])
            print(result.stdout + result.stderr, end="")
            if result.returncode:
                return result.returncode
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
