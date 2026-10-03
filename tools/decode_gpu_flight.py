#!/usr/bin/env python3
"""Decode GETV_GPUFLIGHT=... capture metadata without dumping raw VBO payload bytes.

The raw .bin file contains a rolling window of exact pre-submit OpenGL draw metadata and,
for the configured vertex count (24 by default), exact interleaved VBO payloads. Treat the
raw file and extracted payloads as private local diagnostic evidence.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys
from typing import Iterable


MAGIC_V2 = b"GEGF0060"
MAGIC_V3 = b"GEGF0061"

HEADER = struct.Struct("<8s10I3Q96s128s96s")
DRAW_V2 = struct.Struct(
    "<"
    "27Q"
    "10I"
    "2i2I"
    "2i2i2i2i2i2i"
    "4i"
    "4i4i"
    "2iI"
    "i2I"
)
DRAW_V3 = struct.Struct(
    "<"
    "28Q"      # old 27Q + source owner pointer
    "11IiI32s" # old 10I + source kind/id/aux + bounded producer name
    "2i2I"     # texture ids, GL ids
    "2i2i2i2i2i2i"  # width, height, filter, has_height, wrap_s, wrap_t
    "4i"       # depth, zwrite, blend, decal
    "4i4i"     # viewport, scissor
    "2iI"      # gfx command progress, dl depth, opcode
    "i2I"      # payload slot, payload bytes, truncated
)
PAYLOAD_HEAD = struct.Struct("<QII")
SHADER_HEAD = struct.Struct("<QIIIQQ")
PAYLOAD_DATA_BYTES = 4096
SHADER_VS_BYTES = 1024
SHADER_FS_BYTES = 4096


def cstr(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("utf-8", "replace")


def hex64(v: int) -> str:
    return f"{v:016x}"


def read_header(data: bytes) -> dict[str, int | str]:
    if len(data) < HEADER.size:
        raise ValueError("capture is shorter than header")
    vals = HEADER.unpack_from(data, 0)
    (
        magic,
        version,
        header_bytes,
        draw_bytes,
        payload_record_bytes,
        shader_record_bytes,
        draw_slots,
        payload_slots,
        shader_slots,
        payload_max_bytes,
        payload_vertex_filter,
        total_draws,
        total_payloads,
        total_shaders,
        gl_vendor,
        gl_renderer,
        gl_version,
    ) = vals
    if (magic, version) not in ((MAGIC_V2, 2), (MAGIC_V3, 3)):
        raise ValueError(
            f"unsupported capture magic/version {magic!r}/{version}; "
            f"expected {MAGIC_V2!r}/2 or {MAGIC_V3!r}/3"
        )
    if header_bytes != HEADER.size:
        raise ValueError(f"header size mismatch: file={header_bytes} decoder={HEADER.size}")
    expected_draw = DRAW_V2.size if version == 2 else DRAW_V3.size
    if draw_bytes != expected_draw:
        raise ValueError(f"draw size mismatch: file={draw_bytes} decoder={expected_draw}")
    if payload_record_bytes != PAYLOAD_HEAD.size + PAYLOAD_DATA_BYTES:
        raise ValueError("payload record size mismatch")
    if shader_record_bytes != SHADER_HEAD.size + SHADER_VS_BYTES + SHADER_FS_BYTES:
        raise ValueError("shader record size mismatch")
    return {
        "header_bytes": header_bytes,
        "version": version,
        "draw_bytes": draw_bytes,
        "payload_record_bytes": payload_record_bytes,
        "shader_record_bytes": shader_record_bytes,
        "draw_slots": draw_slots,
        "payload_slots": payload_slots,
        "shader_slots": shader_slots,
        "payload_max_bytes": payload_max_bytes,
        "payload_vertex_filter": payload_vertex_filter,
        "total_draws": total_draws,
        "total_payloads": total_payloads,
        "total_shaders": total_shaders,
        "gl_vendor": cstr(gl_vendor),
        "gl_renderer": cstr(gl_renderer),
        "gl_version": cstr(gl_version),
    }


def draw_offset(h: dict[str, int | str], slot: int) -> int:
    return int(h["header_bytes"]) + slot * int(h["draw_bytes"])


def payload_base(h: dict[str, int | str]) -> int:
    return int(h["header_bytes"]) + int(h["draw_slots"]) * int(h["draw_bytes"])


def shader_base(h: dict[str, int | str]) -> int:
    return payload_base(h) + int(h["payload_slots"]) * int(h["payload_record_bytes"])


def parse_draw(data: bytes, h: dict[str, int | str], slot: int) -> dict[str, object]:
    version = int(h["version"])
    if version == 2:
        v = DRAW_V2.unpack_from(data, draw_offset(h, slot))
        q = list(v[:27])
        i = 27
        u = list(v[i:i+10]); i += 10
        source_owner = 0
        source_kind = 0
        source_id = -1
        source_aux = 0
        source_name = ""
    else:
        v = DRAW_V3.unpack_from(data, draw_offset(h, slot))
        q = list(v[:28])
        i = 28
        u = list(v[i:i+11]); i += 11
        source_owner = q[27]
        source_kind = u[10]
        source_id = v[i]; i += 1
        source_aux = v[i]; i += 1
        source_name = cstr(v[i]); i += 1

    texture_id = list(v[i:i+2]); i += 2
    texture_gl_id = list(v[i:i+2]); i += 2
    texture_width = list(v[i:i+2]); i += 2
    texture_height = list(v[i:i+2]); i += 2
    texture_filter = list(v[i:i+2]); i += 2
    texture_has_height = list(v[i:i+2]); i += 2
    texture_wrap_s = list(v[i:i+2]); i += 2
    texture_wrap_t = list(v[i:i+2]); i += 2
    depth_test, depth_write, blend, decal = v[i:i+4]; i += 4
    viewport = list(v[i:i+4]); i += 4
    scissor = list(v[i:i+4]); i += 4
    gfx_cmd_progress, gfx_dl_depth, gfx_opcode = v[i:i+3]; i += 3
    payload_slot, payload_bytes, payload_truncated = v[i:i+3]

    return {
        "serial": q[0],
        "monotonic_ns": q[1],
        "vbo_hash": q[2],
        "shader_id": q[3],
        "texture_hash": q[4:6],
        "height_hash": q[6:8],
        "gfx_w0": q[8],
        "gfx_w1": q[9],
        "gfx_cmd_ptr": q[10],
        "gfx_dl_stack": q[11:19],
        "gfx_dl_callsite": q[19:27],
        "gfx_source_owner": source_owner,
        "frame": u[0],
        "draw": u[1],
        "tris": u[2],
        "verts": u[3],
        "vbo_bytes": u[4],
        "stride_floats": u[5],
        "attribs": u[6],
        "program": u[7],
        "gfx_dl_stack_base_depth": u[8],
        "gfx_dl_stack_count": u[9],
        "gfx_source_kind": source_kind,
        "gfx_source_id": source_id,
        "gfx_source_aux": source_aux,
        "gfx_source_name": source_name,
        "texture_id": texture_id,
        "texture_gl_id": texture_gl_id,
        "texture_width": texture_width,
        "texture_height": texture_height,
        "texture_filter": texture_filter,
        "texture_has_height": texture_has_height,
        "texture_wrap_s": texture_wrap_s,
        "texture_wrap_t": texture_wrap_t,
        "depth_test": depth_test,
        "depth_write": depth_write,
        "blend": blend,
        "decal": decal,
        "viewport": viewport,
        "scissor": scissor,
        "gfx_cmd_progress": gfx_cmd_progress,
        "gfx_dl_depth": gfx_dl_depth,
        "gfx_opcode": gfx_opcode,
        "payload_slot": payload_slot,
        "payload_bytes": payload_bytes,
        "payload_truncated": payload_truncated,
    }


def retained_draws(data: bytes, h: dict[str, int | str]) -> list[dict[str, object]]:
    total = int(h["total_draws"])
    slots = int(h["draw_slots"])
    first = max(1, total - slots + 1)
    out: list[dict[str, object]] = []
    for serial in range(first, total + 1):
        slot = (serial - 1) % slots
        d = parse_draw(data, h, slot)
        if int(d["serial"]) == serial:
            out.append(d)
    return out


def parse_payload(data: bytes, h: dict[str, int | str], slot: int) -> tuple[int, int, int, bytes]:
    off = payload_base(h) + slot * int(h["payload_record_bytes"])
    serial, size, truncated = PAYLOAD_HEAD.unpack_from(data, off)
    start = off + PAYLOAD_HEAD.size
    return serial, size, truncated, data[start:start + min(size, PAYLOAD_DATA_BYTES)]


def shader_map(data: bytes, h: dict[str, int | str]) -> dict[int, dict[str, object]]:
    out: dict[int, dict[str, object]] = {}
    slots = int(h["shader_slots"])
    count = min(int(h["total_shaders"]), slots)
    base = shader_base(h)
    rec_size = int(h["shader_record_bytes"])
    for slot in range(count):
        off = base + slot * rec_size
        shader_id, program, vs_len, fs_len, vs_hash, fs_hash = SHADER_HEAD.unpack_from(data, off)
        if shader_id == 0 and program == 0:
            continue
        p = off + SHADER_HEAD.size
        vs = data[p:p + SHADER_VS_BYTES].split(b"\0", 1)[0].decode("utf-8", "replace")
        p += SHADER_VS_BYTES
        fs = data[p:p + SHADER_FS_BYTES].split(b"\0", 1)[0].decode("utf-8", "replace")
        out[shader_id] = {
            "program": program,
            "vs_len": vs_len,
            "fs_len": fs_len,
            "vs_hash": vs_hash,
            "fs_hash": fs_hash,
            "vs": vs,
            "fs": fs,
        }
    return out


def format_draw(d: dict[str, object]) -> str:
    tex_id = d["texture_id"]
    tex_hash = d["texture_hash"]
    tex_gl = d["texture_gl_id"]
    dims = list(zip(d["texture_width"], d["texture_height"]))
    dl_count = min(int(d["gfx_dl_stack_count"]), len(d["gfx_dl_stack"]))
    dl_stack = d["gfx_dl_stack"][:dl_count]
    dl_calls = d["gfx_dl_callsite"][:dl_count]
    dl_text = ">".join(
        f"{int(root):016x}@{int(call):016x}" for root, call in zip(dl_stack, dl_calls)
    )
    cmd_off = "?"
    if dl_stack:
        root = int(dl_stack[-1])
        cmd = int(d["gfx_cmd_ptr"])
        if root != 0 and cmd >= root:
            cmd_off = f"+0x{cmd - root:x}"
    source_name = str(d["gfx_source_name"]) or "?"
    return (
        f"serial={d['serial']} mono_ns={d['monotonic_ns']} frame={d['frame']} draw={d['draw']} "
        f"verts={d['verts']} tris={d['tris']} bytes={d['vbo_bytes']} stride_f={d['stride_floats']} "
        f"vbo={hex64(int(d['vbo_hash']))} shader={hex64(int(d['shader_id']))}/prog={d['program']} "
        f"tex0={tex_id[0]}/{tex_gl[0]}/{hex64(int(tex_hash[0]))}/{dims[0][0]}x{dims[0][1]} "
        f"tex1={tex_id[1]}/{tex_gl[1]}/{hex64(int(tex_hash[1]))}/{dims[1][0]}x{dims[1][1]} "
        f"depth={d['depth_test']} zwrite={d['depth_write']} blend={d['blend']} decal={d['decal']} "
        f"cmd={d['gfx_cmd_progress']} op=0x{int(d['gfx_opcode']):02x} depth={d['gfx_dl_depth']} "
        f"w0={int(d['gfx_w0']):016x} w1={int(d['gfx_w1']):016x} "
        f"cmdptr={int(d['gfx_cmd_ptr']):016x} cmdoff={cmd_off} "
        f"dl={d['gfx_dl_stack_base_depth']}+{dl_count}[{dl_text}] "
        f"src={source_name}/k{d['gfx_source_kind']}/id{d['gfx_source_id']}/"
        f"aux0x{int(d['gfx_source_aux']):x}/owner{int(d['gfx_source_owner']):016x} "
        f"payload={d['payload_slot']}:{d['payload_bytes']}{'T' if d['payload_truncated'] else ''}"
    )


def print_sequence(draws: Iterable[dict[str, object]], width: int = 24) -> None:
    vals = list(draws)
    for start in range(0, len(vals), width):
        chunk = vals[start:start + width]
        print("  " + " ".join(f"{int(d['verts']):>3}" for d in chunk))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("capture", type=Path)
    ap.add_argument("--last", type=int, default=256, help="last N retained submissions to print")
    ap.add_argument("--verts", type=int, default=None, help="candidate vertex count (default: capture payload filter)")
    ap.add_argument("--gaps-ms", type=float, default=0.0,
                    help="print consecutive submission gaps at least this many milliseconds")
    ap.add_argument("--show-shaders", action="store_true", help="print GLSL for candidate draws")
    ap.add_argument("--extract-payloads", type=Path, default=None,
                    help="PRIVATE: extract candidate VBO payloads to this directory")
    args = ap.parse_args()

    data = args.capture.read_bytes()
    h = read_header(data)
    expected = (
        int(h["header_bytes"])
        + int(h["draw_slots"]) * int(h["draw_bytes"])
        + int(h["payload_slots"]) * int(h["payload_record_bytes"])
        + int(h["shader_slots"]) * int(h["shader_record_bytes"])
    )
    if len(data) < expected:
        raise ValueError(f"capture truncated: have {len(data)} bytes, expected at least {expected}")

    draws = retained_draws(data, h)
    verts = args.verts if args.verts is not None else int(h["payload_vertex_filter"])
    shaders = shader_map(data, h)

    print("=== GPU FLIGHT 0061 ===")
    print(f"file: {args.capture}")
    print(f"GL vendor:   {h['gl_vendor']}")
    print(f"GL renderer: {h['gl_renderer']}")
    print(f"GL version:  {h['gl_version']}")
    print(f"total submissions: {h['total_draws']}")
    print(f"retained submissions: {len(draws)} / {h['draw_slots']}")
    print(f"target payload verts: {h['payload_vertex_filter']}")
    print(f"payload writes: {h['total_payloads']}")
    print(f"shader writes: {h['total_shaders']}")
    if draws:
        first_ns = int(draws[0]["monotonic_ns"])
        last_ns = int(draws[-1]["monotonic_ns"])
        span_s = max(0, last_ns - first_ns) / 1_000_000_000.0
        print(f"retained monotonic range: {first_ns} .. {last_ns} ({span_s:.3f}s)")
        print(f"retained frame range: {draws[0]['frame']} .. {draws[-1]['frame']}")
    print()

    if args.gaps_ms > 0:
        threshold_ns = int(args.gaps_ms * 1_000_000.0)
        print(f"=== SUBMISSION GAPS >= {args.gaps_ms:g} ms ===")
        found_gap = False
        for prev, cur in zip(draws, draws[1:]):
            delta_ns = int(cur["monotonic_ns"]) - int(prev["monotonic_ns"])
            if delta_ns < threshold_ns:
                continue
            found_gap = True
            print(f"gap_ms={delta_ns / 1_000_000.0:.3f}")
            print("PREV " + format_draw(prev))
            print("NEXT " + format_draw(cur))
        if not found_gap:
            print("none")
        print()

    tail = draws[-max(0, args.last):]
    print(f"=== LAST {len(tail)} VERTEX COUNTS ===")
    print_sequence(tail)
    print()

    print(f"=== LAST {len(tail)} DRAW RECORDS ===")
    for d in tail:
        print(format_draw(d))
    print()

    candidates = [d for d in draws if int(d["verts"]) == verts]
    print(f"=== RETAINED {verts}-VERTEX CANDIDATES: {len(candidates)} ===")
    for d in candidates[-128:]:
        line = format_draw(d)
        slot = int(d["payload_slot"])
        if slot >= 0:
            pserial, pbytes, ptrunc, payload = parse_payload(data, h, slot)
            if pserial == int(d["serial"]):
                line += f" payload_sha256={hashlib.sha256(payload).hexdigest()}"
            else:
                line += " payload=OVERWRITTEN"
        print(line)

    if args.show_shaders:
        ids = []
        for d in candidates[-128:]:
            sid = int(d["shader_id"])
            if sid not in ids:
                ids.append(sid)
        for sid in ids:
            s = shaders.get(sid)
            if not s:
                continue
            print()
            print(f"=== SHADER {sid:016x} program={s['program']} "
                  f"vs_hash={int(s['vs_hash']):016x} fs_hash={int(s['fs_hash']):016x} ===")
            print("--- vertex ---")
            print(s["vs"])
            print("--- fragment ---")
            print(s["fs"])

    if args.extract_payloads is not None:
        args.extract_payloads.mkdir(parents=True, exist_ok=True)
        seen: set[int] = set()
        for d in candidates:
            serial = int(d["serial"])
            slot = int(d["payload_slot"])
            if slot < 0 or serial in seen:
                continue
            pserial, _pbytes, _ptrunc, payload = parse_payload(data, h, slot)
            if pserial != serial:
                continue
            out = args.extract_payloads / f"serial-{serial}-verts-{verts}.vbo.bin"
            out.write_bytes(payload)
            seen.add(serial)
        print()
        print(f"PRIVATE payloads extracted: {len(seen)} -> {args.extract_payloads}")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as exc:
        print(f"decode_gpu_flight: {exc}", file=sys.stderr)
        raise SystemExit(2)
