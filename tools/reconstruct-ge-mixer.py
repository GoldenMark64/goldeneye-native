#!/usr/bin/env python3
"""Reconstruct the 0064 GoldenEye mixer without publishing its dense coefficient table.

The historical fetched mixer already contains the stock 64x4 libultra resampler
coefficients. The frozen 0064 mixer uses the same 256 values, but its surrounding
implementation and signed-literal formatting differ. This tool validates those
upstream-derived coefficients, formats them exactly as the 0064 source expects,
and inserts them into a table-free public template.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
from pathlib import Path

EXPECTED_TABLE_SHA256 = "f5010bc5d7798e7a059af43623dfcdb26c34b5bf2d282ad253db5bdc32ce591b"
MARKER = "@@GE_RESAMPLE_TABLE@@"

OLD_TABLE = re.compile(
    r"static\s+int16_t\s+resample_table\s*\[64\]\[4\]\s*=\s*\{(.*?)\n\};",
    re.DOTALL,
)
HEX_LITERAL = re.compile(r"0x([0-9A-Fa-f]{1,4})")


def table_values(text: str) -> list[int]:
    match = OLD_TABLE.search(text)
    if match is None:
        raise SystemExit("reconstruct-ge-mixer: historical resample_table not found")

    values = [int(item, 16) for item in HEX_LITERAL.findall(match.group(1))]
    if len(values) != 256:
        raise SystemExit(
            f"reconstruct-ge-mixer: expected 256 coefficients, found {len(values)}"
        )

    digest = hashlib.sha256(
        b"".join(struct.pack(">H", value) for value in values)
    ).hexdigest()
    if digest != EXPECTED_TABLE_SHA256:
        raise SystemExit(
            "reconstruct-ge-mixer: coefficient table hash mismatch "
            f"({digest} != {EXPECTED_TABLE_SHA256})"
        )
    return values


def literal(value: int) -> str:
    token = f"0x{value:04x}"
    return f"(int16_t){token}" if value >= 0x8000 else token


def render_table(values: list[int]) -> str:
    rows = []
    for offset in range(0, len(values), 4):
        row = values[offset : offset + 4]
        rows.append("{" + ", ".join(literal(value) for value in row) + "}")

    lines = ["static const int16_t geResampleTable[64][4] = {"]
    for offset in range(0, len(rows), 2):
        pair = ", ".join(rows[offset : offset + 2])
        if offset + 2 < len(rows):
            pair += ","
        lines.append("    " + pair)
    lines.append("};")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("historical_mixer", type=Path)
    parser.add_argument("template", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    historical = args.historical_mixer.read_text(encoding="utf-8")
    template = args.template.read_text(encoding="utf-8")

    if template.count(MARKER) != 1:
        raise SystemExit(
            f"reconstruct-ge-mixer: template must contain exactly one {MARKER}"
        )

    result = template.replace(MARKER, render_table(table_values(historical)))
    args.output.write_text(result, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
