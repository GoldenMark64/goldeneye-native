#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHRPROP = (ROOT / "vendor/ge-decomp/src/game/chrprop.c").read_text()
EXPLOSION = (ROOT / "vendor/ge-decomp/src/game/explosion.c").read_text()
PROPOBJ = (ROOT / "vendor/ge-decomp/src/game/propobj.c").read_text()
PATCH = (ROOT / "getv/patches/0045-proximity-box-stall-diagnostic.patch").read_text()

checks = [
    ("roomGetProps generation counter", "ge_roomprops_generation++" in CHRPROP),
    ("proximity source filter", "ge_proxbox_source_weapon(temp_s2) == ITEM_PROXIMITYMINE" in EXPLOSION),
    ("room-list mutation marker", "ROOMLIST-MUTATED" in EXPLOSION),
    ("bounded room-list scan", "PTR_LIST_OBJECT_LOOKUP_INDICES_LEN" in EXPLOSION),
    ("mine explode before marker", 'ge_proxbox_mine_debug_tick("mine-explode-before"' in PROPOBJ),
    ("mine explode after marker", 'ge_proxbox_mine_debug_tick("mine-explode-after"' in PROPOBJ),
    ("diagnostic env gate", "GETV_PROXBOX_DEBUG" in EXPLOSION and "GETV_PROXBOX_DEBUG" in PROPOBJ),
    ("durable patch carries mutation marker", "ROOMLIST-MUTATED" in PATCH),
]

failed = False
for name, ok in checks:
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
    failed |= not ok

if failed:
    raise SystemExit(1)

print(f"PASS: proximity/box diagnostic regression ({len(checks)}/{len(checks)})")
