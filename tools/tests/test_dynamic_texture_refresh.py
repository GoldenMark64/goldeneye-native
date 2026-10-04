#!/usr/bin/env python3
"""ROM-free regression for mutable textures allocated from GoldenEye's transient dyn buffers."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_GFX = ROOT / "getv/port/fast3d/gfx_pc.c"
PLATFORM = ROOT / "getv/port/platform.h"
SUPPORT = ROOT / "getv/port/src/port_support.c"


def function_slice(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    pos = brace
    while pos < len(source):
        ch = source[pos]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
        pos += 1
    raise ValueError(f"unterminated function: {signature}")


def check(gfx_path: Path) -> list[str]:
    errors: list[str] = []
    platform = PLATFORM.read_text()
    support = SUPPORT.read_text()
    gfx = gfx_path.read_text()

    if "bool gePortTextureSourceIsTransient(const void *ptr);" not in platform:
        errors.append("platform.h does not expose the transient texture-source classifier")

    try:
        helper = function_slice(support, "bool gePortTextureSourceIsTransient(const void *ptr)")
    except ValueError as exc:
        errors.append(str(exc))
        helper = ""

    for needle in (
        "g_VtxBuffers[0] == NULL",
        "g_VtxBuffers[2] == NULL",
        "p >= begin && p < end",
    ):
        if needle not in helper:
            errors.append(f"transient classifier missing range guard: {needle}")

    try:
        lookup = function_slice(
            gfx,
            "static bool gfx_texture_cache_lookup(int tile, struct TextureHashmapNode **n, "
            "const uint8_t *orig_addr, uint32_t fmt, uint32_t siz)",
        )
    except ValueError as exc:
        errors.append(str(exc))
        return errors

    transient = "gePortTextureSourceIsTransient(orig_addr)"
    if transient not in lookup:
        errors.append("texture-cache hit does not classify transient source addresses")
        return errors

    call = lookup.index(transient)
    before = lookup[:call]
    after = lookup[call:call + 260]

    if "gfx_rapi->select_texture(tile, (*node)->texture_id);" not in before:
        errors.append("transient refresh does not reuse/select the existing GPU texture object")
    if "*n = *node;" not in before:
        errors.append("transient refresh does not preserve the existing cache node")
    if "return false;" not in after:
        errors.append("transient cache hit does not force the caller through import/upload")

    static_hit = lookup[call:]
    if "ge_prof.tex_hit++;" not in static_hit or "return true;" not in static_hit:
        errors.append("ordinary immutable texture-cache hits no longer retain the normal fast path")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gfx-source", type=Path, default=DEFAULT_GFX)
    args = parser.parse_args()

    if not args.gfx_source.is_file():
        print(f"gfx source not found: {args.gfx_source}", file=sys.stderr)
        return 2

    errors = check(args.gfx_source)
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1

    print("PASS dyn buffer range is exposed through tracked port code")
    print("PASS transient address cache hits reuse the GPU object but force pixel re-upload")
    print("PASS ordinary static texture cache hits retain the existing fast path")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
