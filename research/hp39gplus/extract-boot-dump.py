#!/usr/bin/env python3
"""Extract a validated H39BOOT1 boot block from a returned HP39 aplet/object."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

MAGIC = b"H39BOOT1"
DUMP_BYTES = 0x4000
PASS = 0x50415353


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    blob = args.input.read_bytes()
    matches: list[bytes] = []
    start = 0

    while True:
        at = blob.find(MAGIC, start)
        if at < 0:
            break
        start = at + 1
        if at + 16 + DUMP_BYTES > len(blob):
            continue

        length, status = struct.unpack_from("<II", blob, at + 8)
        if length != DUMP_BYTES or status != PASS:
            continue
        matches.append(blob[at + 16 : at + 16 + DUMP_BYTES])

    if len(matches) != 1:
        raise SystemExit(f"expected one complete H39BOOT1 dump, found {len(matches)}")

    dump = matches[0]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(dump)
    print(f"{args.output}: {len(dump)} bytes sha256={hashlib.sha256(dump).hexdigest()}")


if __name__ == "__main__":
    main()
