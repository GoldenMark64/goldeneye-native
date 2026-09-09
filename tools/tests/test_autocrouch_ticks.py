#!/usr/bin/env python3
"""ROM-free regression for auto-crouch across render-only frames.

Compile the production collision reset and animation integrator against a
synthetic low-passage probe. Requires patched source, never extracted assets.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT / 'vendor/ge-decomp')
    args = parser.parse_args()
    bond = (args.source_root / 'src/game/bondview2.c').read_text()
    collision = function(bond, 'void bondviewCalcUpdatePlayerCollision(')
    reset = collision.split('    g_BondCanEnterTank = 0;', 1)[1].split('    if (g_WorldTankProp != NULL)', 1)[0]
    prop = (args.source_root / 'src/game/propobj.c').read_text()
    animation = function(prop, 'void chrobjApplySpeed(')
    harness = '''
#include <stdio.h>
typedef float f32;
typedef int s32;
enum { CROUCH_SQUAT = 0, CROUCH_STAND = 2 };
struct Player { int autocrouchpos; } player, *g_CurrentPlayer = &player;
int g_ClockTimer;
float g_GlobalTimerDelta;
''' + animation + '''
void collision_reset(void) {
''' + reset + '''
}
int main(void) {
    int failed = 0;
    for (int cadence = 0; cadence < 3; ++cadence) {
        float height = 0, speed = 0;
        player.autocrouchpos = CROUCH_STAND;
        for (int phase = 0; phase < 2; ++phase) {
            for (int frame = 0; frame < 240; ++frame) {
                int dt = cadence == 0 ? (frame % 2) : cadence;
                g_ClockTimer = dt;
                g_GlobalTimerDelta = (float)dt;
                float target = player.autocrouchpos == CROUCH_SQUAT ? -100 : 0;
                chrobjApplySpeed(&height, target, &speed, .5f, .5f, 5.f);
                if (height == target) speed = 0;
                collision_reset();
                /* Only a moving probe reaches the neighboring low tile. */
                if (dt && phase == 0) player.autocrouchpos = CROUCH_SQUAT;
            }
            int ok = height == (phase == 0 ? -100.f : 0.f);
            printf("%s cadence=%d %s height=%.1f\\n", ok ? "PASS" : "FAIL",
                   cadence, phase == 0 ? "enter vent" : "leave vent", height);
            failed += !ok;
        }
    }
    return failed != 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='autocrouch-test-') as directory:
        source, binary = Path(directory) / 'test.c', Path(directory) / 'test'
        source.write_text(harness)
        subprocess.run(['cc', '-DGE_PORT_NATIVE', '-std=c11', '-Wall', '-Wextra',
                        '-Werror', str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
