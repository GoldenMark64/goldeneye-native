#!/usr/bin/env python3
"""ROM-free checks of the patched decomp's actual cuff-selection function."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = ROOT / "vendor/ge-decomp/src/game/bondview2.c"

HARNESS = r"""
#include <stdio.h>
#include <string.h>

typedef int s32;
typedef unsigned char u8;

typedef struct ModelNode {
    int unused;
} ModelNode;

typedef struct Model {
    s32 rwdata[40];
    s32 bad_rwdata;
    int seen[40];
} Model;

typedef struct ModelFileHeader {
    ModelNode **Switches;
    s32 numSwitches;
} ModelFileHeader;

typedef struct Player {
    s32 bondtype;
} Player;

enum {
    CUFF_BOILER = 1,
    CUFF_BROSNAN,
    CUFF_DALTON,
    CUFF_MOORE,
    CUFF_FOLDER,
    CUFF_CONNERY,
    CUFF_BLUE,
    CUFF_JUNGLE,
    CUFF_SNOW
};

static ModelNode nodes[40];
static Player player;
Player *g_CurrentPlayer = &player;

static int fileGetBondForCurrentFolder(void)
{
    return 0;
}

static s32 *modelGetNodeRwData(Model *model, ModelNode *node)
{
    int i;
    for (i = 0; i < 40; i++) {
        if (node == &nodes[i]) {
            model->seen[i]++;
            return &model->rwdata[i];
        }
    }
    model->bad_rwdata++;
    return &model->bad_rwdata;
}

#include "cuff.inc"

static int fails;

static void check(int condition, const char *what)
{
    if (condition) {
        printf("  PASS  %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        fails++;
    }
}

static int exactly_seen(Model *model, int first, int last)
{
    int i;
    if (model->bad_rwdata != 0) {
        return 0;
    }
    for (i = 0; i < 40; i++) {
        if (model->seen[i] != (i >= first && i <= last)) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    Model model;
    ModelNode *switches[40];
    ModelFileHeader header = { switches, 36 };
    int i;

    for (i = 0; i < 40; i++) {
        switches[i] = &nodes[i];
    }
    player.bondtype = CUFF_BOILER;

    memset(&model, 0, sizeof model);
    bondviewSelectCuff(&model, &header, 29);
    check(exactly_seen(&model, 29, 34),
          "Throwing Knife selects native entries 29 through 34");

    memset(&model, 0, sizeof model);
    header.numSwitches = 34;
    bondviewSelectCuff(&model, &header, 29);
    check(exactly_seen(&model, 1, 0),
          "Throwing Knife window extending past the array is rejected");

    memset(&model, 0, sizeof model);
    header.numSwitches = 5;
    bondviewSelectCuff(&model, &header, 0);
    check(exactly_seen(&model, 1, 0),
          "array shorter than six entries is rejected");

    memset(&model, 0, sizeof model);
    header.numSwitches = 10;
    bondviewSelectCuff(&model, &header, 4);
    check(exactly_seen(&model, 4, 9),
          "opening watch selects native entries 4 through 9");

    printf("\n%d checks, %d failures\n", 4, fails);
    return fails != 0;
}
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()

    compiler = shutil.which(args.cc)
    if compiler is None:
        raise SystemExit(f"compiler not found: {args.cc}")
    if not args.source.is_file():
        raise SystemExit(f"patched decomp source not found: {args.source}")

    source = args.source.read_text()
    start = source.index("void bondviewSelectCuff(")
    end = source.index("\n\n/**", start)

    with tempfile.TemporaryDirectory(prefix="ge-cuff-switch-") as temporary:
        directory = Path(temporary)
        (directory / "cuff.inc").write_text(source[start:end] + "\n")
        (directory / "test.c").write_text(HARNESS)
        executable = directory / "test_cuff_switch"
        compile_result = subprocess.run(
            [compiler, "-std=c11", "-O1", "-g", "-fno-strict-aliasing",
             "-Werror=return-type", "-DGE_PORT_NATIVE", directory / "test.c",
             "-o", executable],
            capture_output=True, text=True,
        )
        if compile_result.returncode:
            print(compile_result.stdout + compile_result.stderr, end="")
            return compile_result.returncode
        result = subprocess.run([executable], capture_output=True, text=True)
        print(result.stdout + result.stderr, end="")
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
