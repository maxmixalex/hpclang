#!/usr/bin/env python3
"""Wrap the HP39g+ read-only boot dumper for ARMToolbox39 old PrRUN.

This is a research-only Launcher V3 envelope. It deliberately uses the old
launcher: ARMToolbox allocates 0x4010 output bytes and leaves the resulting
string on the Saturn stack after the ARM function returns.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

DUMP_BYTES = 0x4000
HEADER_BYTES = 0x10
OUTPUT_BYTES = DUMP_BYTES + HEADER_BYTES
FLAGS = 0x001  # old launcher, ARMToolbox-provided RAM, leave string on stack
MEMORY_WORD = (FLAGS << 20) | OUTPUT_BYTES
LINK_MARKER = 0x3176B34C


def wrap(image: bytes) -> bytes:
    l3 = bytearray(b"\x4c\xb3\x80A>CP")
    l3 += image
    l3 += struct.pack("<I", 0)  # no secondary entry points
    l3 += struct.pack("<I", MEMORY_WORD)
    l3 += struct.pack("<III", 0, 0, LINK_MARKER)

    nibbles = (2 * len(l3) + 5) << 4
    if nibbles > 0xFFFFFF:
        raise ValueError("boot-dump stage is too large")

    return (
        b"HPHP49-C"
        + b"\x2c\x2a"
        + nibbles.to_bytes(4, "little")[:3]
        + bytes(l3)
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    image = args.input.read_bytes()
    output = wrap(image)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)

    print(
        f"{args.output}: ARM={len(image)} bytes, output={OUTPUT_BYTES} bytes, "
        f"flags=0x{FLAGS:x}, memory-word=0x{MEMORY_WORD:08x}"
    )


if __name__ == "__main__":
    main()
