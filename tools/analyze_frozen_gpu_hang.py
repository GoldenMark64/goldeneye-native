#!/usr/bin/env python3
"""Correlate an in-progress i915 hang with frozen GoldenEye GPU/function rings.

Unlike map_gpu_hang_draw.py, this tool does not require a post-stall submission
gap.  It is intended for unattended captures taken while the game is still
blocked in the GPU wait path and the function-flight ring has already frozen.
"""
from __future__ import annotations

import argparse
import difflib
import importlib.util
from pathlib import Path
import sys


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gpu", type=Path)
    ap.add_argument("function", type=Path)
    ap.add_argument("i915", type=Path)
    ap.add_argument("--frames", type=int, default=512,
                    help="recent GPU frames to consider (default: 512)")
    ap.add_argument("--neighbors", type=int, default=8)
    args = ap.parse_args()

    here = Path(__file__).resolve().parent
    gpu = load("decode_gpu_flight", here / "decode_gpu_flight.py")
    mapper = load("map_gpu_hang_draw", here / "map_gpu_hang_draw.py")
    fun = load("decode_function_flight", here / "decode_function_flight.py")

    gdata = args.gpu.read_bytes()
    gh = gpu.read_header(gdata)
    draws = gpu.retained_draws(gdata, gh)
    if not draws:
        raise RuntimeError("GPU flight ring contains no retained draws")

    lines = args.i915.read_text(errors="replace").splitlines()
    batch_start, batch_end, bbaddr, hardware = mapper.parse_i915(lines)
    if not hardware:
        raise RuntimeError("i915 decode contains no active-batch buffer-0 bindings")
    if bbaddr is None:
        raise RuntimeError("i915 decode contains no BBADDR")

    candidates = [i for i, x in enumerate(hardware) if x["primitive"] <= bbaddr]
    if not candidates:
        raise RuntimeError("BBADDR precedes all decoded hardware primitives")
    active_hw = candidates[-1]
    hx = hardware[active_hw]
    hw_seq = [(x["pitch"], x["extent"]) for x in hardware]

    # Preserve frame order while considering only the most recent N frames.
    frame_order = []
    by_frame = {}
    for d in draws:
        f = int(d["frame"])
        if f not in by_frame:
            frame_order.append(f)
            by_frame[f] = []
        by_frame[f].append(d)
    frame_order = frame_order[-max(1, args.frames):]

    ranked = []
    for frame in frame_order:
        cpu = by_frame[frame]
        cpu_seq = [(int(d["stride_floats"]) * 4, int(d["vbo_bytes"])) for d in cpu]
        sm = difflib.SequenceMatcher(None, cpu_seq, hw_seq, autojunk=False)
        cpu_index = mapper.cpu_index_for_hw(sm, active_hw)
        if cpu_index is None or not (0 <= cpu_index < len(cpu)):
            continue
        # Ratio is the primary discriminator; matching-block count is reported
        # for review rather than folded into an opaque score.
        matched = sum(b.size for b in sm.get_matching_blocks())
        ranked.append((sm.ratio(), matched, frame, cpu_index, cpu, sm))

    if not ranked:
        raise RuntimeError("no recent CPU frame maps the active hardware primitive")
    ranked.sort(key=lambda x: (x[0], x[1], x[2]), reverse=True)
    ratio, matched, frame, cpu_index, cpu, sm = ranked[0]
    target = cpu[cpu_index]

    print("=== GOLDENEYE FROZEN GPU HANG ANALYSIS ===")
    print(f"gpu={args.gpu}")
    print(f"function={args.function}")
    print(f"i915={args.i915}")
    if batch_start is not None and batch_end is not None:
        print(f"active_batch=0x{batch_start:08x}..0x{batch_end:08x}")
    print(f"BBADDR=0x{bbaddr:08x}")
    print(
        f"active_hw={active_hw} bind=0x{hx['cmd']:08x} "
        f"primitive=0x{hx['primitive']:08x} pitch={hx['pitch']} bytes={hx['extent']}"
    )
    print()

    print("=== TOP FRAME MATCHES ===")
    for r, m, f, ci, c, _ in ranked[:10]:
        print(f"frame={f} ratio={r:.9f} matched={m} cpu_draws={len(c)} mapped_cpu_index={ci}")
    print()

    print("=== HARDWARE-MAPPED TARGET ===")
    print(f"frame={frame} ratio={ratio:.9f} matched={matched}")
    print(f"mapped_cpu_index={cpu_index} draw={target['draw']} gpu_serial={target['serial']}")
    print("TARGET " + gpu.format_draw(target) + mapper.payload_suffix(gpu, gdata, gh, target))
    print()

    print("=== SAME DRAW POSITION IN RECENT MATCHING FRAMES ===")
    for r, m, f, ci, c, _ in ranked[:16]:
        if ci >= len(c):
            continue
        d = c[ci]
        print(
            f"frame={f} ratio={r:.9f} cpu_index={ci} "
            + gpu.format_draw(d)
            + mapper.payload_suffix(gpu, gdata, gh, d)
        )
    print()

    print("=== TARGET SHADER OCCURRENCES ===")
    target_shader = int(target["shader_id"])
    same_shader = [d for d in draws if int(d["shader_id"]) == target_shader]
    print(f"shader={target_shader:016x} occurrences={len(same_shader)}")
    for d in same_shader[-32:]:
        print(gpu.format_draw(d))
    print()

    shaders = gpu.shader_map(gdata, gh)
    shader = shaders.get(target_shader)
    if shader:
        print("=== TARGET SHADER SOURCE ===")
        print(
            f"shader={target_shader:016x} program={shader['program']} "
            f"vs_hash={int(shader['vs_hash']):016x} fs_hash={int(shader['fs_hash']):016x}"
        )
        print("--- vertex ---")
        print(shader["vs"])
        print("--- fragment ---")
        print(shader["fs"])
        print()

    print("=== TARGET GPU NEIGHBORHOOD ===")
    lo = max(0, cpu_index - max(0, args.neighbors))
    hi = min(len(cpu), cpu_index + max(0, args.neighbors) + 1)
    for i in range(lo, hi):
        mark = ">>" if i == cpu_index else "  "
        print(f"{mark} cpu_index={i} " + gpu.format_draw(cpu[i]))
    print()

    events, meta = fun.load_events(args.function)
    print("=== FUNCTION RING ===")
    print(
        f"events_total={meta['total']} retained={len(events)} slots={meta['slots']} "
        f"batches={meta['batches']}"
    )
    if not events:
        raise RuntimeError("function-flight ring contains no retained events")
    print(f"retained_event_serial={events[0]['serial']}..{events[-1]['serial']}")
    links = [e for e in events if e["phase"] == 5 and e["gpu_serial"]]
    if links:
        print(f"retained_gpu_serial={min(e['gpu_serial'] for e in links)}..{max(e['gpu_serial'] for e in links)}")
    print()

    gpu_serial = int(target["serial"])
    matches = [e for e in events if e["phase"] == 5 and e["gpu_serial"] == gpu_serial]
    if not matches:
        print(f"TARGET_FUNCTION_LINK=NOT_RETAINED gpu_serial={gpu_serial}")
    else:
        ev = matches[-1]
        stack, last_tri = fun.stack_at(events, ev["serial"])
        batch = next(
            (x for x in reversed(events)
             if x["serial"] <= ev["serial"] and x["phase"] == 4 and x["batch"] == ev["batch"]),
            None,
        )
        print("=== TARGET FUNCTION CORRELATION ===")
        print(f"TARGET_FUNCTION_LINK=RETAINED gpu_serial={gpu_serial} event={ev['serial']} batch={ev['batch']}")
        if batch:
            print(
                f"BATCH reason={batch['label'] or '?'} cmd={batch['cmd']} depth={batch['depth']} "
                f"op=0x{batch['opcode']:02x} cmdptr=0x{batch['ptr0']:x} "
                f"floats={batch['arg0']} tris={batch['arg1']}"
            )
        if last_tri:
            print(
                f"TRIGGER_TRI gfx_sp_tri1({last_tri['arg0']},{last_tri['arg1']},{last_tri['arg2']}) "
                f"event={last_tri['serial']} cmd={last_tri['cmd']} op=0x{last_tri['opcode']:02x}"
            )
        print("CALL_CHAIN " + " -> ".join(fun.FUNCTION.get(fn, f"func#{fn}") for fn in stack))
        print(fun.describe(ev))
        print()

    # The ring is frozen while the main thread is stuck.  Its final unmatched
    # enter/before event therefore identifies the call that did not return.
    final = events[-1]
    final_stack, final_tri = fun.stack_at(events, final["serial"])
    pending = {}
    for ev in events:
        fn = ev["function"]
        if ev["phase"] == 6:       # before
            pending[fn] = ev
        elif ev["phase"] == 7:     # after
            pending.pop(fn, None)

    print("=== STALL POINT FROM FROZEN FUNCTION TAIL ===")
    print("LAST_EVENT " + fun.describe(final))
    print("OPEN_CALL_CHAIN " + " -> ".join(fun.FUNCTION.get(fn, f"func#{fn}") for fn in final_stack))
    if final_tri:
        print("LAST_TRI " + fun.describe(final_tri))
    if pending:
        for fn, ev in sorted(pending.items(), key=lambda kv: kv[1]["serial"]):
            print(
                "UNRETURNED_BEFORE "
                + fun.FUNCTION.get(fn, f"func#{fn}")
                + " " + fun.describe(ev)
            )
    else:
        print("UNRETURNED_BEFORE none")
    print()

    print("=== FINAL 48 FUNCTION EVENTS ===")
    for ev in events[-48:]:
        print(fun.describe(ev))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"analyze_frozen_gpu_hang: {exc}", file=sys.stderr)
        raise SystemExit(2)
