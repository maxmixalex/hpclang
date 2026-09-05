#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import binascii
import json
import struct
import zlib
from pathlib import Path


DIGITS = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "010", "010", "010"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
}


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"P5"):
        raise ValueError(f"{path}: expected P5 PGM")

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
        raise ValueError(f"{path}: unsupported PGM geometry/maxval")

    while pos < len(data) and data[pos] in b" \t\r\n":
        pos += 1
    pixels = data[pos:pos + width * height]
    if len(pixels) != width * height:
        raise ValueError(f"{path}: truncated pixel data")
    return width, height, pixels


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    scanlines = b"".join(
        b"\x00" + pixels[y * width:(y + 1) * width]
        for y in range(height)
    )
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png += png_chunk(
        b"IHDR",
        struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0),
    )
    png += png_chunk(b"IDAT", zlib.compress(scanlines, 9))
    png += png_chunk(b"IEND", b"")
    path.write_bytes(png)


def scale_pixels(
    width: int, height: int, pixels: bytes, scale: int
) -> tuple[int, int, bytes]:
    out_width = width * scale
    rows: list[bytes] = []
    for y in range(height):
        src = pixels[y * width:(y + 1) * width]
        row = b"".join(bytes((value,)) * scale for value in src)
        rows.extend([row] * scale)
    return out_width, height * scale, b"".join(rows)


def draw_number(
    canvas: bytearray,
    canvas_width: int,
    x: int,
    y: int,
    number: int,
    scale: int = 2,
) -> None:
    cursor = x
    for digit in str(number):
        glyph = DIGITS[digit]
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit != "1":
                    continue
                for dy in range(scale):
                    for dx in range(scale):
                        px = cursor + gx * scale + dx
                        py = y + gy * scale + dy
                        canvas[py * canvas_width + px] = 0
        cursor += 4 * scale


def build_sheet(
    paths: list[Path],
    output: Path,
    scale: int,
    columns: int,
) -> list[Path]:
    existing = [path for path in paths if path.exists()]
    if not existing:
        return []

    frames = [read_pgm(path) for path in existing]
    frame_width = max(width for width, _, _ in frames)
    frame_height = max(height for _, height, _ in frames)
    scaled_width = frame_width * scale
    scaled_height = frame_height * scale
    label_height = 14
    gap = 4
    columns = max(1, min(columns, len(existing)))
    rows = (len(existing) + columns - 1) // columns
    tile_width = scaled_width
    tile_height = label_height + scaled_height
    sheet_width = columns * tile_width + (columns - 1) * gap
    sheet_height = rows * tile_height + (rows - 1) * gap
    canvas = bytearray(b"\xff" * (sheet_width * sheet_height))

    for index, ((width, height, pixels), path) in enumerate(zip(frames, existing)):
        col = index % columns
        row = index // columns
        x0 = col * (tile_width + gap)
        y0 = row * (tile_height + gap)
        draw_number(canvas, sheet_width, x0 + 3, y0 + 2, index + 1)

        sw, sh, scaled = scale_pixels(width, height, pixels, scale)
        px0 = x0 + (scaled_width - sw) // 2
        py0 = y0 + label_height + (scaled_height - sh) // 2
        for y in range(sh):
            src = scaled[y * sw:(y + 1) * sw]
            start = (py0 + y) * sheet_width + px0
            canvas[start:start + sw] = src

    output.parent.mkdir(parents=True, exist_ok=True)
    write_png(output, sheet_width, sheet_height, bytes(canvas))
    return existing


def title(path: Path) -> str:
    name = path.name[:-4] if path.name.endswith(".pgm") else path.name
    return name.replace("-", " ").replace("_", " ")


def emit_summary(
    heading: str,
    files: list[Path],
    image_url: str | None,
    image_file: Path | None,
    json_files: list[str],
) -> None:
    found = False
    existing = [path for path in files if path.exists()]

    print(f"## {heading}")
    if image_file and image_file.exists():
        image_url = "data:image/png;base64," + base64.b64encode(
            image_file.read_bytes()
        ).decode("ascii")
    if existing and image_url:
        found = True
        print(f'\n<img alt="{heading}" src="{image_url}">')
        print("\n**Frames:**")
        for index, path in enumerate(existing, 1):
            width, height, _ = read_pgm(path)
            print(f"{index}. `{title(path)}` — {width}×{height}")
    elif existing:
        found = True
        print("\n_LCD screenshots were produced, but no summary image URL was available._")

    for raw in json_files:
        path = Path(raw)
        if not path.exists():
            continue
        found = True
        print(f"\n### {title(path)}")
        print("~~~json")
        try:
            print(json.dumps(json.loads(path.read_text()), indent=2, sort_keys=True))
        except Exception:
            print(path.read_text().rstrip())
        print("~~~")

    if not found:
        print("\n_No LCD evidence was produced before this job stopped._")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--json", dest="json_files", action="append", default=[])
    parser.add_argument("--heading", default="Calculator LCD evidence")
    parser.add_argument("--image-out", type=Path)
    parser.add_argument("--image-url")
    parser.add_argument("--image-file", type=Path)
    parser.add_argument("--scale", type=int, default=2)
    parser.add_argument("--columns", type=int, default=4)
    parser.add_argument("--image-only", action="store_true")
    args = parser.parse_args()

    if args.scale < 1 or args.columns < 1:
        raise SystemExit("--scale and --columns must be positive")

    if args.image_out:
        build_sheet(args.files, args.image_out, args.scale, args.columns)

    if not args.image_only:
        emit_summary(
            args.heading,
            args.files,
            args.image_url,
            args.image_file,
            args.json_files,
        )


if __name__ == "__main__":
    main()
