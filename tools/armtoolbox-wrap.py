#!/usr/bin/env python3
"""Wrap one hpclang stock-ARMToolbox stage0 image in Launcher V3 simple form.

This is an internal platform envelope, not an hpclang application ABI.
There are intentionally no legacy format switches or compatibility profiles.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

MEMORY_REQUEST = 0x01C00000
LINK_MARKER = 0x3176B34C


def wrap(image: bytes) -> bytes:
    l3 = bytearray(b"\x4c\xb3\x80A>CP")
    l3 += image

    l3 += struct.pack("<I", 0)  # no secondary Launcher V3 entry points
    l3 += struct.pack("<I", MEMORY_REQUEST)
    l3 += struct.pack("<III", 0, 0, LINK_MARKER)

    nibbles = (2 * len(l3) + 5) << 4
    if nibbles > 0xFFFFFF:
        raise ValueError("stage0 object is too large")

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
        f"{args.output}: stage0={len(image)} bytes, "
        f"wrapped={len(output)} bytes"
    )


if __name__ == "__main__":
    main()
