#!/usr/bin/env python3
"""Collect returned HP39g+ aplets/objects into one verified 1 MiB flash image."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

MAGIC = b"H39ROM01"
CHUNK_BYTES = 0x4000
FLASH_BYTES = 0x100000
PASS = 0x50415353
CHUNKS = FLASH_BYTES // CHUNK_BYTES


def scan(blob: bytes):
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
        if (status ^ checksum) == PASS:
            yield offset, data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    chunks: dict[int, bytes] = {}
    sources: dict[int, str] = {}

    for path in sorted(p for p in args.input_dir.rglob("*") if p.is_file()):
        try:
            blob = path.read_bytes()
        except OSError:
            continue
        for offset, data in scan(blob):
            old = chunks.get(offset)
            if old is not None and old != data:
                raise SystemExit(
                    f"conflicting chunk at 0x{offset:05x}: "
                    f"{sources[offset]} vs {path}"
                )
            chunks[offset] = data
            sources[offset] = str(path)

    missing = [
        index for index in range(CHUNKS)
        if index * CHUNK_BYTES not in chunks
    ]
    if missing:
        tags = " ".join(f"{index:02X}" for index in missing)
        raise SystemExit(
            f"found {len(chunks)}/{CHUNKS} chunks; missing: {tags}"
        )

    image = b"".join(
        chunks[index * CHUNK_BYTES]
        for index in range(CHUNKS)
    )
    if len(image) != FLASH_BYTES:
        raise SystemExit("internal size mismatch")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)

    manifest = {
        "bytes": len(image),
        "sha256": hashlib.sha256(image).hexdigest(),
        "chunk_bytes": CHUNK_BYTES,
        "chunks": [
            {
                "index": index,
                "offset": f"0x{index * CHUNK_BYTES:05x}",
                "sha256": hashlib.sha256(
                    chunks[index * CHUNK_BYTES]
                ).hexdigest(),
                "source": sources[index * CHUNK_BYTES],
            }
            for index in range(CHUNKS)
        ],
    }
    manifest_path = args.output.with_suffix(args.output.suffix + ".json")
    manifest_path.write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )

    print(
        f"{args.output}: {len(image)} bytes "
        f"sha256={manifest['sha256']}"
    )
    print(f"{manifest_path}: manifest")


if __name__ == "__main__":
    main()
