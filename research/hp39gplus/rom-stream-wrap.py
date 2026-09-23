#!/usr/bin/env python3
"""Wrap the HP39g+ USB ROM streamer for ARMToolbox39 old PrRUN."""
from __future__ import annotations
import argparse, struct
from pathlib import Path

# Streamer does not return a ROM object and requests no launcher result buffer.
REQUEST_BYTES = 0
FLAGS = 0x000
MEMORY_WORD = (FLAGS << 20) | REQUEST_BYTES
LINK_MARKER = 0x3176B34C


def wrap(image: bytes) -> bytes:
    l3 = bytearray(b"\x4c\xb3\x80A>CP")
    l3 += image
    l3 += struct.pack("<I", 0)
    l3 += struct.pack("<I", MEMORY_WORD)
    l3 += struct.pack("<III", 0, 0, LINK_MARKER)
    nibbles = (2 * len(l3) + 5) << 4
    if nibbles > 0xFFFFFF:
        raise ValueError("ROM stream stage is too large")
    return b"HPHP49-C" + b"\x2c\x2a" + nibbles.to_bytes(4,"little")[:3] + bytes(l3)


def main() -> None:
    ap=argparse.ArgumentParser(); ap.add_argument("input",type=Path); ap.add_argument("output",type=Path); a=ap.parse_args()
    image=a.input.read_bytes(); out=wrap(image); a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_bytes(out)
    print(f"{a.output}: ARM={len(image)} bytes request={REQUEST_BYTES} flags=0x{FLAGS:x}")

if __name__ == "__main__": main()
