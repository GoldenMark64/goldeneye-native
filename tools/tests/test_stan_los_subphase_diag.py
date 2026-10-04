#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STAN = (ROOT / "vendor/ge-decomp/src/game/stan.c").read_text()
WATCH_C = (ROOT / "getv/port/src/ge_stall_watchdog.c").read_text()
WATCH_H = (ROOT / "getv/port/src/ge_stall_watchdog.h").read_text()
P46 = (ROOT / "getv/patches/0046-stan-room-buffer-native-terminator.patch").read_text()
P47 = (ROOT / "getv/patches/0047-stan-los-subphase-diagnostic.patch").read_text()

checks = [
    ("native room buffer has terminator slot", "s32 spD0[0x15]" in STAN),
    ("0046 carries only room-buffer safety", "s32 spD0[0x15]" in P46 and "gePortStallStanPhase" not in P46),
    ("watchdog exposes STAN phase API", "gePortStallStanPhase(int phase, int detail)" in WATCH_H),
    ("watchdog prints STAN phase", "stanphase=%s standetail=%d" in WATCH_C),
    ("tile-walk phase marker", "gePortStallStanPhase(2, 0)" in STAN),
    ("room-prop phase marker", "gePortStallStanRoomPropsBegin(sp124)" in STAN),
    ("collision bounds phase marker", "gePortStallStanPhase(6, (s32)*spB8)" in STAN),
    ("edge-loop phase marker", "gePortStallStanPhase(8, numvertices0)" in STAN),
    ("impossible edge-count diagnostic", "impossible collision edge count" in STAN),
    ("0047 carries diagnostics", "gePortStallStanPhase" in P47 and "impossible collision edge count" in P47),
]

failed = False
for name, ok in checks:
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
    failed |= not ok

if failed:
    raise SystemExit(1)

print(f"PASS: STAN LOS subphase regression ({len(checks)}/{len(checks)})")
