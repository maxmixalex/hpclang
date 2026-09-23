#!/usr/bin/env python3
"""Extract one validated 16 KiB HP39g+ physical-NOR chunk."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

MAGIC = b"H39ROM01"
CHUNK_BYTES = 0x4000
FLASH_BYTES = 0x100000
PASS = 0x50415353


def candidates(blob: bytes):
    start = 0
    while True:
        at = blob.find(MAGIC, start)
        if at < 0:
            return
        start = at + 1
        end = at + 16 + CHUNK_BYTES
        if end > len(blob):
            continue
        offset, status = struct.unpack_from("<II", blob, at + 8)
        if offset % CHUNK_BYTES or offset >= FLASH_BYTES:
            continue
        data = blob[at + 16 : end]
        checksum = sum(data) & 0xFFFFFFFF
        if (status ^ checksum) != PASS:
            continue
        yield offset, data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    found = list(candidates(args.input.read_bytes()))
    if len(found) != 1:
        raise SystemExit(
            f"expected one complete H39ROM01 chunk, found {len(found)}"
        )

    offset, data = found[0]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(
        f"{args.output}: offset=0x{offset:05x} bytes={len(data)} "
        f"sha256={hashlib.sha256(data).hexdigest()}"
    )


if __name__ == "__main__":
    main()
