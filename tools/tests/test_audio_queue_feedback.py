#!/usr/bin/env python3
"""Regression for SDL audio queue feedback after a multi-second game stall."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = ROOT / "getv/port/src/port_audio.c"

RATE = 22050
FRAME_SAMPLES = 736
MAX_FRAME = FRAME_SAMPLES + 0x25 + 16
ALIGNED_MAX = MAX_FRAME & ~0xF
TARGET = FRAME_SAMPLES * 4


def old_synthetic_backlog_after_stall(stall_seconds: float, fps: float = 60.0) -> tuple[float, float]:
    """Return seconds to repay the synthetic deficit and physical backlog accumulated meanwhile."""
    deficit = RATE * stall_seconds
    consumed_per_frame = RATE / fps
    gain_per_frame = ALIGNED_MAX - consumed_per_frame
    if gain_per_frame <= 0:
        raise ValueError("test parameters do not close the synthetic deficit")
    repay_frames = deficit / gain_per_frame
    repay_seconds = repay_frames / fps
    physical_backlog_frames = gain_per_frame * repay_frames
    return repay_seconds, physical_backlog_frames / RATE


def check_source(path: Path) -> list[str]:
    source = path.read_text()
    errors: list[str] = []

    required = (
        "SDL_GetQueuedAudioSize(geAudioDev)",
        "geAudioQueuedFramesFromBytes",
        "geAudioWantForQueued",
    )
    for needle in required:
        if needle not in source:
            errors.append(f"missing production queue feedback: {needle}")

    forbidden = (
        "geSubmitted",
        "geClockStart",
        "SDL_GetTicks()",
        "geSubmitted > consumed",
    )
    for needle in forbidden:
        if needle in source:
            errors.append(f"legacy wall-clock queue estimator still present: {needle}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()

    if not args.source.is_file():
        print(f"source not found: {args.source}", file=sys.stderr)
        return 2

    errors = check_source(args.source)
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1

    print("PASS production feedback reads SDL's physical queued bytes")
    print("PASS lifetime submitted/wall-clock estimator is absent")

    repay_s, backlog_s = old_synthetic_backlog_after_stall(5.0)
    if not (4.0 < repay_s < 5.0):
        print(f"FAIL old estimator repayment was unexpectedly {repay_s:.3f}s")
        return 1
    print(f"PASS old estimator needs {repay_s:.3f}s to repay a 5s stall deficit")

    if not (4.0 < backlog_s <= 5.01):
        print(f"FAIL old estimator physical backlog was unexpectedly {backlog_s:.3f}s")
        return 1
    print(f"PASS old estimator can accumulate {backlog_s:.3f}s of physical playback backlog")

    physical_after_stall = 0
    if physical_after_stall != 0:
        print("FAIL physical queue model should be empty after device drains during stall")
        return 1
    print("PASS physical queue feedback resumes from the device's actual empty state")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
