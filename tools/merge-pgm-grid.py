#!/usr/bin/env python3
"""Merge same-size binary PGM frames into one compact contact sheet."""

from __future__ import annotations

import argparse
from pathlib import Path


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    parts = data.split(b"\n", 3)
    if len(parts) != 4 or parts[0] != b"P5":
        raise ValueError(f"{path}: expected binary P5 PGM")
    dims = parts[1].split()
    if len(dims) != 2 or parts[2].strip() != b"255":
        raise ValueError(f"{path}: unsupported PGM header")
    width, height = map(int, dims)
    pixels = parts[3]
    if len(pixels) != width * height:
        raise ValueError(f"{path}: pixel data size mismatch")
    return width, height, pixels


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Merge same-size P5 PGM frames into one contact sheet."
    )
    parser.add_argument("output", type=Path)
    parser.add_argument("frames", nargs="+", type=Path)
    parser.add_argument("--columns", type=int, default=6)
    parser.add_argument("--gap", type=int, default=2)
    args = parser.parse_args()
    if args.columns <= 0 or args.gap < 0:
        parser.error("invalid grid geometry")

    loaded = [read_pgm(path) for path in args.frames]
    width, height = loaded[0][:2]
    if any((w, h) != (width, height) for w, h, _pixels in loaded):
        raise SystemExit("all frames must have identical dimensions")

    rows = (len(loaded) + args.columns - 1) // args.columns
    out_width = args.columns * width + (args.columns - 1) * args.gap
    out_height = rows * height + (rows - 1) * args.gap
    canvas = bytearray([255]) * (out_width * out_height)

    for index, (_width, _height, pixels) in enumerate(loaded):
        row, column = divmod(index, args.columns)
        x0 = column * (width + args.gap)
        y0 = row * (height + args.gap)
        for y in range(height):
            source = y * width
            destination = (y0 + y) * out_width + x0
            canvas[destination:destination + width] = pixels[source:source + width]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(
        f"P5\n{out_width} {out_height}\n255\n".encode("ascii") + bytes(canvas)
    )
    print(f"{args.output}: {len(loaded)} frames -> {out_width}x{out_height}")


if __name__ == "__main__":
    main()
