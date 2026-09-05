#!/usr/bin/env python3
"""Inspect and validate an HP49/50 ARMToolbox L3 transfer envelope."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

TRANSFER_MAGIC = b"HPHP49-C"
STRING_PROLOG = b"\x2c\x2a"
L3_PROLOG = b"\x4c\xb3\x80A>CP"
LINK_MARKER = 0x3176B34C


def inspect(path: Path) -> dict[str, int | str]:
    blob = path.read_bytes()
    if len(blob) < 33:
        raise ValueError(f"{path}: transfer file is too small")
    if blob[:8] != TRANSFER_MAGIC:
        raise ValueError(f"{path}: missing HPHP49-C transfer magic")
    if blob[8:10] != STRING_PROLOG:
        raise ValueError(f"{path}: expected HP string prolog 0x2a2c")

    payload = blob[13:]
    encoded_nibbles = int.from_bytes(blob[10:13] + b"\0", "little") >> 4
    expected_nibbles = 2 * len(payload) + 5
    if encoded_nibbles != expected_nibbles:
        raise ValueError(
            f"{path}: string size mismatch: encoded={encoded_nibbles}, "
            f"expected={expected_nibbles}"
        )
    if payload[:7] != L3_PROLOG:
        raise ValueError(f"{path}: missing Launcher V3 / A>CP prolog")

    functions, memory, entry, base, marker = struct.unpack_from(
        "<IIIII", payload, len(payload) - 20
    )
    if marker != LINK_MARKER:
        raise ValueError(f"{path}: bad Launcher V3 marker 0x{marker:08x}")
    if base != 0:
        raise ValueError(f"{path}: unexpected nonzero base 0x{base:08x}")
    if 20 + functions * 8 > len(payload):
        raise ValueError(f"{path}: impossible Launcher V3 function-table size")

    return {
        "file": str(path),
        "bytes": len(blob),
        "l3_bytes": len(payload),
        "functions": functions,
        "memory": f"0x{memory:08x}",
        "entry": f"0x{entry:08x}",
        "base": f"0x{base:08x}",
        "marker": f"0x{marker:08x}",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.files:
        print(json.dumps(inspect(path), sort_keys=True))


if __name__ == "__main__":
    main()
