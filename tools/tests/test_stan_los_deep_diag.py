#!/usr/bin/env python3
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STAN = (ROOT / "vendor/ge-decomp/src/game/stan.c").read_text()
CHRPROP = (ROOT / "vendor/ge-decomp/src/game/chrprop.c").read_text()
PROPOBJ = (ROOT / "vendor/ge-decomp/src/game/propobj.c").read_text()
WATCH_C = (ROOT / "getv/port/src/ge_stall_watchdog.c").read_text()
WATCH_H = (ROOT / "getv/port/src/ge_stall_watchdog.h").read_text()
PATCH = (ROOT / "getv/patches/0048-stan-los-deep-diagnostic.patch").read_text()

checks = [
    ("roomGetProps begin/end instrumentation",
     "gePortStallStanRoomPropsBegin(sp124)" in STAN
     and "gePortStallStanRoomPropsEnd()" in STAN),
    ("prop-list iterator instrumentation",
     "gePortStallStanPropIter((s32)(spB8 - ptr_list_object_lookup_indices)" in STAN),
    ("separate stan Y phases",
     "gePortStallStanPhase(13, 0)" in STAN
     and "gePortStallStanPhase(14, 0)" in STAN),
    ("room chunk progress instrumentation",
     "gePortStallStanRoomPropsProgress(room, chunkindex, ge_stan_room_steps, writes)" in CHRPROP),
    ("room chunk runaway diagnostic",
     "room chunk traversal exceeded" in CHRPROP),
    ("room chunk index diagnostic",
     "impossible room chunk=" in CHRPROP),
    ("room prop index diagnostic",
     "impossible room prop index=" in CHRPROP),
    ("prop list capacity diagnostic",
     "prop lookup list has no terminator space" in CHRPROP
     and "prop lookup terminator write is out of bounds" in CHRPROP),
    ("watchdog deep STAN phases",
     'return "roomprops_chunk"' in WATCH_C
     and 'return "prop_iter"' in WATCH_C
     and 'return "stany_start"' in WATCH_C
     and 'return "stany_dest"' in WATCH_C),
    ("watchdog publishes auxiliary fields",
     "stanaux=%d,%d,%d" in WATCH_C
     and "gePortStallStanRoomPropsProgress" in WATCH_H),
    ("native collision_data size assertion",
     "_Static_assert(sizeof(collision_data) == 0x4c" in PROPOBJ),
    ("collision allocation fit assertion",
     "_Static_assert(sizeof(collision_data) <= 0x50U" in PROPOBJ),
    ("0048 carries deep diagnostics and ABI proof",
     "room chunk traversal exceeded" in PATCH
     and "sizeof(collision_data) == 0x4c" in PATCH
     and "gePortStallStanPropIter" in PATCH),
]

failed = False
for name, ok in checks:
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
    failed |= not ok

alloc_re = re.compile(
    r"ptr_allocated_collisiondata_block\s*=\s*"
    r"mempAllocBytesInBank\((0x[0-9a-fA-F]+)U?,\s*MEMPOOL_STAGE\)"
)
allocs = [int(v, 16) for v in alloc_re.findall(PROPOBJ)]
alloc_ok = allocs == [0x50, 0x50] and all(v >= 0x4C for v in allocs)
print(f"{'PASS' if alloc_ok else 'FAIL'}: collision allocation sites {allocs!r}")
failed |= not alloc_ok

probe = r'''
#include "bondtypes.h"
_Static_assert(sizeof(collision_data) == 0x4c, "collision_data size");
_Static_assert(__builtin_offsetof(collision_data, edges) == 0x00, "edges");
_Static_assert(__builtin_offsetof(collision_data, polygon) == 0x04, "polygon");
_Static_assert(__builtin_offsetof(collision_data, top) == 0x44, "top");
_Static_assert(__builtin_offsetof(collision_data, bottom) == 0x48, "bottom");
_Static_assert(sizeof(coord2d) == 0x08, "coord2d");
_Static_assert(sizeof(struct rect4f) == 0x20, "rect4f");
'''

cc = os.environ.get("CC", "cc")
with tempfile.TemporaryDirectory(prefix="getv-stan-layout-") as td:
    src = Path(td) / "probe.c"
    src.write_text(probe)
    cmd = [
        cc, "-std=gnu11", "-Wno-builtin-declaration-mismatch", "-fsyntax-only",
        "-I", str(ROOT / "vendor/ge-decomp"),
        "-I", str(ROOT / "vendor/ge-decomp/include"),
        "-I", str(ROOT / "vendor/ge-decomp/include/PR"),
        "-I", str(ROOT / "vendor/ge-decomp/src"),
        "-I", str(ROOT / "vendor/ge-decomp/src/game"),
        "-DVERSION_US", "-DLANG_US", "-DREFRESH_NTSC",
        "-DLEFTOVERDEBUG", "-DLEFTOVERSPECTRUM", "-DBUGFIX_R0",
        "-DTARGET_N64", "-DGE_PORT_NATIVE", "-DNON_MATCHING=1",
        "-DAVOID_UB=1", "-D_LANGUAGE_C=1",
        str(src),
    ]
    proc = subprocess.run(cmd, text=True, capture_output=True)
    abi_ok = proc.returncode == 0
    print(f"{'PASS' if abi_ok else 'FAIL'}: compiler-proven native collision_data ABI")
    if not abi_ok:
        print(proc.stderr)
    failed |= not abi_ok

if failed:
    raise SystemExit(1)

print(f"PASS: STAN LOS deep diagnostic regression ({len(checks) + 2}/{len(checks) + 2})")
