#!/usr/bin/env python3
import argparse
import struct
import sys
from pathlib import Path


HEADER = struct.Struct("<8sIIIIQQQ")
EVENT = struct.Struct("<QQQQQQQQIIIiiI24s")

PHASE = {
    1: "enter",
    2: "exit",
    3: "call",
    4: "batch",
    5: "gpu_link",
    6: "before",
    7: "after",
}

FUNCTION = {
    0: "-",
    1: "gfx_run",
    2: "gfx_run_dl",
    3: "gfx_sp_tri1",
    4: "gfx_flush_reason",
    5: "gfx_flush",
    6: "gfx_opengl_draw_triangles",
    7: "glBufferData",
    8: "glDrawArrays",
}


def label(raw):
    return raw.split(b"\0", 1)[0].decode("utf-8", "replace")


def load_events(path):
    data = Path(path).read_bytes()
    if len(data) < HEADER.size:
        raise ValueError("file is smaller than the function-flight header")

    magic, version, header_bytes, event_bytes, slots, total, batches, start_ns = HEADER.unpack_from(data)
    if magic != b"GEFF0001":
        raise ValueError(f"unexpected magic {magic!r}")
    if version != 1:
        raise ValueError(f"unsupported version {version}")
    if header_bytes != HEADER.size:
        raise ValueError(f"header size mismatch: file={header_bytes} decoder={HEADER.size}")
    if event_bytes != EVENT.size:
        raise ValueError(f"event size mismatch: file={event_bytes} decoder={EVENT.size}")

    need = header_bytes + slots * event_bytes
    if len(data) < need:
        raise ValueError(f"truncated file: need {need} bytes, have {len(data)}")

    first = max(1, total - slots + 1)
    events = []
    for serial in range(first, total + 1):
        slot = (serial - 1) % slots
        off = header_bytes + slot * event_bytes
        values = EVENT.unpack_from(data, off)
        if values[0] != serial:
            continue
        events.append({
            "serial": values[0],
            "mono_ns": values[1],
            "batch": values[2],
            "gpu_serial": values[3],
            "ptr0": values[4],
            "arg0": values[5],
            "arg1": values[6],
            "arg2": values[7],
            "function": values[8],
            "phase": values[9],
            "frame": values[10],
            "cmd": values[11],
            "depth": values[12],
            "opcode": values[13],
            "label": label(values[14]),
        })
    return events, {
        "slots": slots,
        "total": total,
        "batches": batches,
        "start_ns": start_ns,
    }


def describe(ev):
    fn = FUNCTION.get(ev["function"], f"func#{ev['function']}")
    phase = PHASE.get(ev["phase"], f"phase#{ev['phase']}")
    extra = ""
    if ev["phase"] == 4:
        extra = f" reason={ev['label'] or '?'} floats={ev['arg0']} tris={ev['arg1']}"
    elif ev["function"] == 3 and ev["phase"] == 3:
        extra = f" v=({ev['arg0']},{ev['arg1']},{ev['arg2']})"
    elif ev["phase"] == 5:
        packed = ev["arg2"]
        extra = (
            f" shader={ev['arg0']:016x} prog={ev['arg1']} "
            f"floats={packed & 0xffffffff} tris={(packed >> 32) & 0xffffffff}"
        )
    elif ev["function"] == 7:
        extra = f" bytes={ev['arg0']} vbo={ev['arg1']} usage=0x{ev['arg2']:x}"
    elif ev["function"] == 8:
        extra = f" mode=0x{ev['arg0']:x} first={ev['arg1']} count={ev['arg2']}"

    return (
        f"event={ev['serial']} t={ev['mono_ns']} frame={ev['frame']} "
        f"batch={ev['batch']} gpu={ev['gpu_serial']} {phase} {fn} "
        f"cmd={ev['cmd']} depth={ev['depth']} op=0x{ev['opcode']:02x} "
        f"ptr=0x{ev['ptr0']:x}{extra}"
    )


def stack_at(events, stop_serial):
    stack = []
    last_tri = None
    for ev in events:
        if ev["serial"] > stop_serial:
            break
        fn = ev["function"]
        phase = ev["phase"]
        if phase == 1:
            stack.append(fn)
        elif phase == 2:
            for i in range(len(stack) - 1, -1, -1):
                if stack[i] == fn:
                    del stack[i:]
                    break
        elif phase == 3 and fn == 3:
            last_tri = ev
    return stack, last_tri


def main():
    ap = argparse.ArgumentParser(description="Decode GETV_FUNFLIGHT kill-safe renderer traces")
    ap.add_argument("file")
    ap.add_argument("--gpu-serial", type=int, help="show the C chain linked to this GPU-flight serial")
    ap.add_argument("--last", type=int, default=80, help="events to show without --gpu-serial")
    ap.add_argument("--around", type=int, default=16, help="events before/after a matched GPU serial")
    args = ap.parse_args()

    try:
        events, meta = load_events(args.file)
    except (OSError, ValueError) as exc:
        print(f"decode_function_flight: {exc}", file=sys.stderr)
        return 2

    print(
        f"events={meta['total']} retained={len(events)} slots={meta['slots']} "
        f"batches={meta['batches']} start_mono_ns={meta['start_ns']}"
    )

    if args.gpu_serial is None:
        for ev in events[-max(0, args.last):]:
            print(describe(ev))
        return 0

    matches = [ev for ev in events if ev["phase"] == 5 and ev["gpu_serial"] == args.gpu_serial]
    if not matches:
        print(f"gpu_serial={args.gpu_serial} not retained", file=sys.stderr)
        return 1

    target = matches[-1]
    stack, last_tri = stack_at(events, target["serial"])
    batch = next(
        (ev for ev in reversed(events)
         if ev["serial"] <= target["serial"]
         and ev["phase"] == 4 and ev["batch"] == target["batch"]),
        None,
    )

    print()
    print(f"TARGET gpu_serial={args.gpu_serial} event={target['serial']} batch={target['batch']}")
    if batch:
        print(
            f"BATCH reason={batch['label'] or '?'} cmd={batch['cmd']} "
            f"depth={batch['depth']} op=0x{batch['opcode']:02x} "
            f"cmdptr=0x{batch['ptr0']:x} floats={batch['arg0']} tris={batch['arg1']}"
        )
    if last_tri:
        print(
            f"TRIGGER_TRI gfx_sp_tri1({last_tri['arg0']},{last_tri['arg1']},{last_tri['arg2']}) "
            f"event={last_tri['serial']} cmd={last_tri['cmd']} "
            f"op=0x{last_tri['opcode']:02x}"
        )
    print("CALL_CHAIN " + " -> ".join(FUNCTION.get(fn, f"func#{fn}") for fn in stack))
    print(describe(target))

    idx = events.index(target)
    lo = max(0, idx - max(0, args.around))
    hi = min(len(events), idx + max(0, args.around) + 1)
    print()
    print("=== EVENT WINDOW ===")
    for ev in events[lo:hi]:
        mark = ">>" if ev is target else "  "
        print(f"{mark} {describe(ev)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
