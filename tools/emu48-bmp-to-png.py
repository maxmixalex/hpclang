#!/usr/bin/env python3
"""Convert the harness's 24-bit BMP into a deterministic RGB PNG."""

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


def read_bmp24(path: Path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError("not BMP")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    planes, bpp = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]
    if planes != 1 or bpp != 24 or compression != 0:
        raise ValueError("expected uncompressed 24-bit BMP")
    if width <= 0 or height == 0:
        raise ValueError("invalid dimensions")

    top_down = height < 0
    h = abs(height)
    stride = (width * 3 + 3) & ~3
    out = bytearray(width * h * 3)

    for y in range(h):
        file_y = y if top_down else h - 1 - y
        row = data[
            pixel_offset + file_y * stride:
            pixel_offset + file_y * stride + width * 3
        ]
        if len(row) != width * 3:
            raise ValueError("truncated pixels")
        for x in range(width):
            b, g, r = row[x * 3:x * 3 + 3]
            p = (y * width + x) * 3
            out[p:p + 3] = bytes((r, g, b))
    return width, h, bytes(out)


def write_png(path: Path, width: int, height: int, rgb: bytes):
    stride = width * 3
    raw = b"".join(
        b"\x00" + rgb[y * stride:(y + 1) * stride]
        for y in range(height)
    )
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_bmp", type=Path)
    parser.add_argument("output_png", type=Path)
    args = parser.parse_args()
    width, height, rgb = read_bmp24(args.input_bmp)
    write_png(args.output_png, width, height, rgb)
    print(f"{args.output_png}: {width}x{height}")


if __name__ == "__main__":
    main()
