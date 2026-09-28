#!/usr/bin/env python3
"""Convert a binary PGM (P5) into a deterministic grayscale PNG."""

from __future__ import annotations

import argparse
import binascii
import struct
import zlib
from pathlib import Path


def chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF)
    )


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"P5"):
        raise ValueError("expected binary PGM/P5")

    pos = 2
    values: list[bytes] = []
    while len(values) < 3:
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        if pos < len(data) and data[pos] == ord("#"):
            while pos < len(data) and data[pos] not in b"\r\n":
                pos += 1
            continue
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n":
            pos += 1
        values.append(data[start:pos])

    width, height, maxval = map(int, values)
    if width <= 0 or height <= 0 or maxval != 255:
        raise ValueError("unsupported PGM geometry/maxval")

    while pos < len(data) and data[pos] in b" \t\r\n":
        pos += 1
    pixels = data[pos : pos + width * height]
    if len(pixels) != width * height:
        raise ValueError("truncated PGM pixels")
    return width, height, pixels


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    scanlines = b"".join(b"\x00" + pixels[y * width : (y + 1) * width] for y in range(height))
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png += chunk(
        b"IHDR",
        struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0),
    )
    png += chunk(b"IDAT", zlib.compress(scanlines, 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    width, height, pixels = read_pgm(args.input)
    write_png(args.output, width, height, pixels)
    print(f"{args.output}: {width}x{height}")


if __name__ == "__main__":
    main()
