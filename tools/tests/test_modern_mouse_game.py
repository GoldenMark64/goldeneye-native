#!/usr/bin/env python3
"""ROM-free checks of the patched game's mouse gate and angle application.

Requires patched game source, not extracted assets. The production blocks compile
against synthetic player state; the full platform build checks real game types.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'vendor/ge-decomp/src/game/bondview2.c')
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    source = args.source.read_text()
    gate = source.split('/* GE_MODERN_MOUSE_BEGIN:', 1)[1].split('*/', 1)[1]
    gate = gate.split('/* GE_MODERN_MOUSE_END */', 1)[0]
    angles = source.split('    if (ge_mouse_modern) {\n        f32 zoom_scale', 1)[1]
    angles = '    if (ge_mouse_modern) {\n        f32 zoom_scale' + angles.split('\n#endif', 1)[0]
    harness = r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "ge_mouse_look.h"
typedef int s32;
typedef float f32;
enum { CAMERAMODE_NONE, CAMERAMODE_FP, CAMERAMODE_CUTSCENE, WATCH_ANIMATION_0x0=0 };
#define FOV_Y_F 60.0f
static struct { int bonddead, watch_animation_state, insightaimmode; float vv_theta, vv_verta; } player;
static __typeof__(player) *g_CurrentPlayer = &player;
static int g_CameraMode=CAMERAMODE_FP, is_timer_active=1, locked, g_bondviewForceDisarm;
static int g_stopPlayFlag, g_gameOverFlag, g_PlayerIsInTank, player_number;
static float g_GlobalTimerDelta=1.0f, fov=60.0f;
static GeMouseLook look;
static int lvlGetControlsLockedFlag(void) { return locked; }
static int get_cur_playernum(void) { return player_number; }
static float viGetFovY(void) { return fov; }
#define osSyncPrintf printf
int gePortInputTakeMouseLook(int p, int context, float *yaw, float *pitch) {
    *yaw=*pitch=0;
    return p==0 ? geMouseLookTake(&look,context,yaw,pitch) : 0;
}
static void tick(void) {
    float ge_mouse_yaw=0, ge_mouse_pitch=0;
    int ge_mouse_modern=0;
    GATE
    ANGLES
}
static int failures, checks;
static void check(int ok, const char *message) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", message); ++checks; failures += !ok;
}
int main(void) {
    tick();
    geMouseLookAdd(&look,1000,-100,100); tick();
    check(fabsf(player.vv_theta-100)<0.001f && fabsf(player.vv_verta-10)<0.001f,"game applies full mouse displacement");
    tick(); check(fabsf(player.vv_theta-100)<0.001f,"second game tick cannot replay a sample");
    geMouseLookAdd(&look,-1,0,100); tick();
    check(fabsf(player.vv_theta-99.9f)<0.001f,"game reverses immediately");
    g_GlobalTimerDelta=2; geMouseLookAdd(&look,100,0,100); tick();
    check(fabsf(player.vv_theta-109.9f)<0.001f,"tick duration does not multiply displacement");
    g_GlobalTimerDelta=0; geMouseLookAdd(&look,100,0,100); tick();
    check(fabsf(player.vv_theta-109.9f)<0.001f,"render-only frame leaves pending input for simulation");
    g_GlobalTimerDelta=1; tick();
    check(fabsf(player.vv_theta-119.9f)<0.001f,"next simulation consumes preserved input");
    fov=30; player.insightaimmode=1; geMouseLookAdd(&look,100,0,100); tick();
    check(fabsf(player.vv_theta-124.9f)<0.001f,"zoom scales mouse angles while aiming");
    fov=60; geMouseLookAdd(&look,0,-100000,100); tick();
    check(player.vv_verta==89.9f,"upward pitch clamps");
    geMouseLookAdd(&look,0,100000,100); tick();
    check(player.vv_verta==-89.9f,"downward pitch clamps");
    player.vv_theta=1; geMouseLookAdd(&look,-30,0,100); tick();
    check(fabsf(player.vv_theta-358)<0.001f,"yaw wraps at zero");
    int *blocked[]={&locked,&player.bonddead,&player.watch_animation_state,
                    &g_bondviewForceDisarm,&g_stopPlayFlag,&g_gameOverFlag};
    for (unsigned i=0;i<sizeof(blocked)/sizeof(blocked[0]);++i) {
        *blocked[i]=1; geMouseLookAdd(&look,100,0,100); tick();
        check(player.vv_theta==358,"game input lock discards mouse travel");
        *blocked[i]=0; tick();
        check(player.vv_theta==358,"unlock cannot replay blocked input");
    }
    is_timer_active=0; geMouseLookAdd(&look,100,0,100); tick();
    check(player.vv_theta==358,"paused timer discards motion");
    is_timer_active=1; tick();
    g_CameraMode=CAMERAMODE_CUTSCENE; geMouseLookAdd(&look,100,0,100); tick();
    check(player.vv_theta==358,"cutscene discards motion");
    g_CameraMode=CAMERAMODE_FP; tick();
    g_PlayerIsInTank=1; tick();
    check(look.context==2,"tank selects classic response");
    printf("%d checks, %d failures\n",checks,failures);
    return failures != 0 || checks==0;
}
'''.replace('    GATE', gate).replace('    ANGLES', angles)
    with tempfile.TemporaryDirectory(prefix='ge-mouse-game-') as temp:
        directory = Path(temp)
        c = directory / 'test.c'
        c.write_text(harness)
        exe = directory / ('test.exe' if os.name == 'nt' else 'test')
        subprocess.run([args.cc, '-std=gnu17', '-O1', '-Wall', '-Wextra', '-Werror',
                        '-I'+str(ROOT/'getv/port/src'), str(c), '-lm', '-o', str(exe)],
                       check=True, timeout=60)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
