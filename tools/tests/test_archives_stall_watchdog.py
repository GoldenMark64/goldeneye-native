#!/usr/bin/env python3
"""ROM-free regression for GETV_STALLTRACE Archives stall instrumentation."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
WATCH = ROOT / "getv/port/src/ge_stall_watchdog.c"
HEADER = ROOT / "getv/port/src/ge_stall_watchdog.h"
RENDER = ROOT / "getv/port/src/port_render.c"
BOSS = ROOT / "vendor/ge-decomp/src/boss.c"
BG = ROOT / "vendor/ge-decomp/src/game/bg.c"
STAN = ROOT / "vendor/ge-decomp/src/game/stan.c"


class ArchivesStallWatchdogTests(unittest.TestCase):
    def test_source_contract(self) -> None:
        watch = WATCH.read_text()
        render = RENDER.read_text()
        boss = BOSS.read_text()
        bg = BG.read_text()
        stan = STAN.read_text()

        self.assertIn('getenv("GETV_STALLTRACE")', watch)
        self.assertIn("SDL_atomic_t", watch)
        self.assertIn("SDL_CreateThread", watch)
        self.assertIn("250ms threshold", watch)
        self.assertIn("last_hotspot=%s", watch)
        self.assertNotIn("g_CurrentPlayer", watch)
        self.assertNotIn("g_BgRoomInfo", watch)

        self.assertLess(boss.index("gePortStallLvlManage();"), boss.index("lvlManageMpGame();"))
        self.assertLess(boss.index("gePortStallLvlRender();"), boss.index("gdl = lvlRender(gdl);"))
        self.assertLess(boss.index("gePortStallGfxSubmit();"), boss.index("rspGfxTaskStart(firstGdl"))
        self.assertLess(boss.index("gePortStallMemaDefrag();"), boss.index("memaSingleDefragPass();"))

        self.assertIn("gePortStallGfxStartFrame();", render)
        self.assertIn("gePortStallGfxRun();", render)
        self.assertIn("gePortStallGfxEndFrame();", render)
        self.assertIn("gePortStallPostFrame(rendered);", render)

        self.assertIn("gePortStallRoomLoad(roomID);", bg)
        self.assertIn("gePortStallStanCall();", stan)

    def test_watchdog_reports_hotspots_and_recovery(self) -> None:
        cc = shutil.which("cc")
        self.assertIsNotNone(cc, "C compiler required")

        flags = subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "sdl2"], text=True
        ).split()

        harness = r"""
#include <SDL.h>
#include <stdlib.h>
#include "ge_stall_watchdog.h"

int main(void)
{
    int i;
    setenv("GETV_STALLTRACE", "1", 1);
    if (SDL_Init(0) != 0) return 2;

    gePortStallTickStart(42);
    gePortStallLvlManage();
    for (i = 0; i < 100; i++) gePortStallStanCall();
    SDL_Delay(420);

    gePortStallLvlRender();
    SDL_Delay(120);
    gePortStallRoomLoad(17);
    SDL_Delay(420);

    gePortStallGfxSubmit();
    SDL_Delay(120);
    SDL_Quit();
    return 0;
}
"""
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as td:
            tdpath = Path(td)
            src = tdpath / "watchdog_harness.c"
            exe = tdpath / "watchdog_harness"
            src.write_text(harness)
            subprocess.check_call(
                [
                    cc,
                    "-D_GNU_SOURCE",
                    "-I",
                    str(HEADER.parent),
                    str(src),
                    str(WATCH),
                    *flags,
                    "-o",
                    str(exe),
                ]
            )
            run = subprocess.run(
                [str(exe)],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
                env=os.environ.copy(),
            )

        out = run.stdout + run.stderr
        self.assertIn("macro=lvl_manage", out)
        self.assertIn("last_hotspot=stan_los", out)
        self.assertIn("stancalls>=96", out)
        self.assertIn("macro=lvl_render", out)
        self.assertIn("last_hotspot=room_load", out)
        self.assertIn("hotdetail=17", out)
        self.assertGreaterEqual(out.count("[getv][stall] recovered"), 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
