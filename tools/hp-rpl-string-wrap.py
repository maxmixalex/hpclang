#!/usr/bin/env python3
"""Wrap raw bytes as a classic-RPL DOCSTR HP transfer object."""

from __future__ import annotations

import argparse
from pathlib import Path


def wrap_docstr(payload: bytes) -> bytes:
    # DOCSTR length counts the five-nibble length field plus payload nibbles.
    length_nibbles = 5 + 2 * len(payload)
    if length_nibbles > 0xFFFFF:
        raise ValueError("DOCSTR payload is too large")

    # HP transfer files pack the five-nibble prologue and five-nibble length
    # field on nibble boundaries. The length therefore starts in the high
    # nibble of the third byte.
    encoded_length = length_nibbles << 4
    return (
        b"HPHP49-C"
        + b"\x2c\x2a"
        + encoded_length.to_bytes(4, "little")[:3]
        + payload
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    payload = args.input.read_bytes()
    wrapped = wrap_docstr(payload)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(wrapped)
    print(f"{args.output}: DOCSTR payload={len(payload)} bytes, wrapped={len(wrapped)} bytes")


if __name__ == "__main__":
    main()
