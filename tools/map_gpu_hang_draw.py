#!/usr/bin/env python3
"""Map an Intel i915 GPU-hang batch pointer back to one GoldenEye flight-recorder draw.

Input 1 is a GETV_GPUFLIGHT v2 capture produced by ge_gpu_flight.h.
Input 2 is intel_error_decode output for the same process/hang.

The Intel decoder exposes one VERTEX_BUFFERS buffer-0 binding per submitted GL draw.  Its
reported pitch and raw final dword (currently labelled ``mbz`` by intel_error_decode) have
repeatedly matched GoldenEye's CPU-side stride and uploaded VBO byte count exactly.  Sequence
alignment lets us tolerate the one carry-over hardware binding that commonly precedes frame
draw 0 and then map BBADDR/IPEHR back to the corresponding CPU flight record.
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import importlib.util
from pathlib import Path
import re
import sys


def load_decoder(path: Path):
    spec = importlib.util.spec_from_file_location("decode_gpu_flight", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load decoder: {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def parse_hex(token: str) -> int:
    return int(token.replace("_", ""), 16)


def parse_i915(lines: list[str]) -> tuple[int | None, int | None, int | None, list[dict[str, int]]]:
    batch_start = None
    batch_end = None
    bbaddr = None

    for line in lines:
        m = re.search(r"batch:\s*\[0x([0-9a-fA-F_]+),\s*0x([0-9a-fA-F_]+)\]", line)
        if m and batch_start is None:
            batch_start = parse_hex(m.group(1)) & 0xFFFFFFFF
            batch_end = parse_hex(m.group(2)) & 0xFFFFFFFF
        m = re.search(r"BBADDR:\s*0x([0-9a-fA-F_]+)", line)
        if m and bbaddr is None:
            bbaddr = parse_hex(m.group(1)) & 0xFFFFFFFF

    hardware: list[dict[str, int]] = []

    for i, line in enumerate(lines):
        ma = re.match(r"\s*0x([0-9a-fA-F]+):", line)
        mp = re.search(r"buffer 0:\s+sequential,\s+pitch\s+(\d+)b", line)
        if not ma or not mp:
            continue

        cmd = int(ma.group(1), 16)
        if batch_start is not None and batch_end is not None and not (batch_start <= cmd < batch_end):
            continue

        pitch = int(mp.group(1))
        extent = None
        primitive = None

        for j in range(i + 1, min(i + 80, len(lines))):
            s = lines[j]
            if j > i + 1 and "buffer 0: sequential" in s:
                break
            if extent is None and "mbz" in s:
                mm = re.match(r"\s*0x[0-9a-fA-F]+:\s+0x([0-9a-fA-F]+):\s+mbz", s)
                if mm:
                    extent = int(mm.group(1), 16)
            if primitive is None and "3DPRIMITIVE" in s:
                mm = re.match(r"\s*0x([0-9a-fA-F]+):", s)
                if mm:
                    primitive = int(mm.group(1), 16)

        if extent is not None:
            hardware.append({
                "cmd": cmd,
                "pitch": pitch,
                "extent": extent,
                "primitive": primitive if primitive is not None else cmd,
            })

    return batch_start, batch_end, bbaddr, hardware


def cpu_index_for_hw(sm: difflib.SequenceMatcher, hw_index: int) -> int | None:
    for block in sm.get_matching_blocks():
        if block.size == 0:
            continue
        if block.b <= hw_index < block.b + block.size:
            return block.a + (hw_index - block.b)
    return None


def payload_suffix(gpu, data: bytes, h: dict[str, object], d: dict[str, object]) -> str:
    slot = int(d["payload_slot"])
    if slot < 0:
        return ""
    pserial, pbytes, ptrunc, payload = gpu.parse_payload(data, h, slot)
    if pserial != int(d["serial"]):
        return " payload=OVERWRITTEN"
    return (
        f" payload_bytes={pbytes}{'T' if ptrunc else ''}"
        f" payload_sha256={hashlib.sha256(payload).hexdigest()}"
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("flight", type=Path)
    ap.add_argument("i915_decoded", type=Path)
    ap.add_argument("--decoder", type=Path, default=Path(__file__).with_name("decode_gpu_flight.py"))
    ap.add_argument("--gap-ms", type=float, default=50.0)
    ap.add_argument("--neighbors", type=int, default=5)
    args = ap.parse_args()

    gpu = load_decoder(args.decoder)
    data = args.flight.read_bytes()
    h = gpu.read_header(data)
    draws = gpu.retained_draws(data, h)
    if len(draws) < 2:
        raise RuntimeError("flight capture has fewer than two retained draws")

    gaps = []
    threshold = int(args.gap_ms * 1_000_000.0)
    for i in range(1, len(draws)):
        delta = int(draws[i]["monotonic_ns"]) - int(draws[i - 1]["monotonic_ns"])
        if delta >= threshold:
            gaps.append((delta, i))
    if not gaps:
        raise RuntimeError(f"no submission gap >= {args.gap_ms:g} ms found")

    delta, post_index = max(gaps)
    pre = draws[post_index - 1]
    post = draws[post_index]
    frame = int(pre["frame"])
    cpu = [d for d in draws if int(d["frame"]) == frame]

    lines = args.i915_decoded.read_text(errors="replace").splitlines()
    batch_start, batch_end, bbaddr, hardware = parse_i915(lines)
    if not hardware:
        raise RuntimeError("no active-batch buffer-0 bindings decoded")

    cpu_seq = [(int(d["stride_floats"]) * 4, int(d["vbo_bytes"])) for d in cpu]
    hw_seq = [(x["pitch"], x["extent"]) for x in hardware]
    sm = difflib.SequenceMatcher(None, cpu_seq, hw_seq, autojunk=False)

    active_hw = None
    if bbaddr is not None:
        candidates = [i for i, x in enumerate(hardware) if x["primitive"] <= bbaddr]
        if candidates:
            active_hw = candidates[-1]

    print("=== GOLDENEYE GPU HANG DRAW MAP ===")
    print(f"flight={args.flight}")
    print(f"i915={args.i915_decoded}")
    print(f"largest_gap_ms={delta / 1_000_000.0:.3f}")
    print("PRE_GAP  " + gpu.format_draw(pre))
    print("POST_GAP " + gpu.format_draw(post))
    print()
    print(f"pre_gap_frame={frame} cpu_draws={len(cpu)}")
    if batch_start is not None and batch_end is not None:
        print(f"active_batch=0x{batch_start:08x}..0x{batch_end:08x}")
    print(f"hardware_bindings={len(hardware)}")
    print(f"sequence_ratio={sm.ratio():.9f}")
    for b in sm.get_matching_blocks():
        if b.size:
            print(f"match cpu[{b.a}:{b.a+b.size}] hw[{b.b}:{b.b+b.size}] len={b.size}")
    print()

    if bbaddr is None or active_hw is None:
        print("BBADDR could not be mapped to a hardware primitive")
        return 0

    hx = hardware[active_hw]
    cpu_index = cpu_index_for_hw(sm, active_hw)
    print(f"BBADDR=0x{bbaddr:08x}")
    print(
        f"active_hw={active_hw} bind=0x{hx['cmd']:08x} primitive=0x{hx['primitive']:08x} "
        f"pitch={hx['pitch']} bytes={hx['extent']}"
    )

    if cpu_index is None:
        print("active hardware binding is outside the CPU/HW sequence match")
        return 0

    target = cpu[cpu_index]
    print(f"mapped_cpu_index={cpu_index} draw={target['draw']} serial={target['serial']}")
    print("TARGET " + gpu.format_draw(target) + payload_suffix(gpu, data, h, target))
    print()
    print("=== TARGET NEIGHBORHOOD ===")
    lo = max(0, cpu_index - args.neighbors)
    hi = min(len(cpu), cpu_index + args.neighbors + 1)
    for i in range(lo, hi):
        marker = ">>" if i == cpu_index else "  "
        d = cpu[i]
        print(f"{marker} cpu_index={i} " + gpu.format_draw(d) + payload_suffix(gpu, data, h, d))

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"map_gpu_hang_draw: {exc}", file=sys.stderr)
        raise SystemExit(2)
