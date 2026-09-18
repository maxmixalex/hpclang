#!/usr/bin/env python3
"""Verify HP39gs/40gs native Gallery glyphs against the stock 2.22 NOR.

The oracle intentionally reconstructs the reviewed physical-ROM main/mini
rasters directly from the packed NOR image. It does not call hpclang font code,
so a broken runtime lookup, extended-code map, or glyph index cannot satisfy the
check merely by producing non-empty pixels.
"""
from __future__ import annotations

import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

CONTENT_LEFT = 13
CONTENT_TOP = 17
CONTENT_BOTTOM_MARGIN = 3
MINI_GLYPH_COUNT = 155
MINI_RECORD_NIBBLES = 11
MINI_MAP_FIRST_CODE = 0xA0
EDITLINE_RECORD_NIBBLES = 20
EDITLINE_TABLE_NIBBLES = 256 * EDITLINE_RECORD_NIBBLES
EDITLINE_REFERENCE_SHA256 = "f106efa599a5bb8a783f29cbc2dfa3afbace750975e783c223f53864a2945f53"
SPECIMEN = (0xA3, 0xA5, 0xA9, 0xB0, 0xB1, 0xB5, 0xD7, 0xF7)


@dataclass(frozen=True)
class FontBases:
    main: int
    mini: int
    editline: int


BASES = {
    # Physical nibble addresses inside the official packed 2.22 NOR.
    "hp39gs": FontBases(main=0x000FCAE9, mini=0x000FEB2D, editline=0x000FD882),
    "hp40gs": FontBases(main=0x000FCC9C, mini=0x000FECE0, editline=0x000FDA35),
}


@dataclass(frozen=True)
class Pgm:
    width: int
    height: int
    pixels: bytes


def _token(data: bytes, pos: int) -> tuple[bytes, int]:
    size = len(data)
    while True:
        while pos < size and data[pos] in b" \t\r\n":
            pos += 1
        if pos < size and data[pos] == ord("#"):
            while pos < size and data[pos] not in b"\r\n":
                pos += 1
            continue
        break
    start = pos
    while pos < size and data[pos] not in b" \t\r\n#":
        pos += 1
    if start == pos:
        raise ValueError("missing PGM header token")
    return data[start:pos], pos


def read_pgm(path: Path) -> Pgm:
    data = path.read_bytes()
    pos = 0
    magic, pos = _token(data, pos)
    width_raw, pos = _token(data, pos)
    height_raw, pos = _token(data, pos)
    maxval, pos = _token(data, pos)
    if magic != b"P5" or maxval != b"255":
        raise ValueError(f"{path}: expected binary P5/255 PGM")
    width = int(width_raw)
    height = int(height_raw)
    if pos >= len(data) or data[pos] not in b" \t\r\n":
        raise ValueError(f"{path}: malformed PGM raster separator")
    # The Gallery writer emits a single '\n'. Accept CRLF without consuming
    # arbitrary leading pixel values that happen to be whitespace bytes.
    if data[pos:pos + 2] == b"\r\n":
        pos += 2
    else:
        pos += 1
    pixels = data[pos:pos + width * height]
    if len(pixels) != width * height:
        raise ValueError(f"{path}: truncated PGM raster")
    return Pgm(width=width, height=height, pixels=pixels)


def nibble(nor: bytes, address: int) -> int:
    offset = address >> 1
    if offset >= len(nor):
        raise ValueError(
            f"NOR is too small for nibble 0x{address:x} "
            f"(needs byte 0x{offset:x}, has 0x{len(nor):x})"
        )
    value = nor[offset]
    return value >> 4 if address & 1 else value & 0x0F


def saturn_byte(nor: bytes, address: int) -> int:
    return nibble(nor, address) | (nibble(nor, address + 1) << 4)


def verify_editline_fingerprint(nor: bytes, bases: FontBases, target: str) -> None:
    payload = bytes(
        nibble(nor, bases.editline + offset)
        for offset in range(EDITLINE_TABLE_NIBBLES)
    )
    digest = hashlib.sha256(payload).hexdigest()
    if digest != EDITLINE_REFERENCE_SHA256:
        raise SystemExit(
            f"{target}: HP39 EditLine ROM fingerprint mismatch at "
            f"0x{bases.editline:x}: {digest} != {EDITLINE_REFERENCE_SHA256}"
        )
    print(
        f"PASS HP39 EditLine ROM fingerprint: {target} "
        f"base=0x{bases.editline:x} sha256={digest}"
    )


def mini_index(nor: bytes, base: int, codepoint: int) -> int:
    cp = codepoint if 0 <= codepoint <= 0xFF else ord("?")
    if ord("a") <= cp <= ord("z"):
        return cp - (ord("a") - ord("A"))
    if cp >= MINI_MAP_FIRST_CODE:
        map_first = base + MINI_GLYPH_COUNT * MINI_RECORD_NIBBLES
        return saturn_byte(nor, map_first + (cp - MINI_MAP_FIRST_CODE) * 2)
    if cp >= 0x7B:
        return cp - 0x1A
    return cp


def render_specimen(nor: bytes, bases: FontBases, kind: str) -> tuple[int, int, frozenset[tuple[int, int]]]:
    points: set[tuple[int, int]] = set()
    pen_x = 0
    if kind == "mini":
        height = 5
        for cp in SPECIMEN:
            index = mini_index(nor, bases.mini, cp)
            if index >= MINI_GLYPH_COUNT:
                raise ValueError(
                    f"mini map U+{cp:04X} returned out-of-atlas index {index}"
                )
            record = bases.mini + index * MINI_RECORD_NIBBLES
            advance = nibble(nor, record) or 4
            for yy in range(height):
                row = saturn_byte(nor, record + 1 + yy * 2)
                for xx in range(8):
                    if row & (1 << xx):
                        points.add((pen_x + xx, yy))
            pen_x += advance
    elif kind == "main":
        height = 8
        for cp in SPECIMEN:
            record = bases.main + cp * 16
            for yy in range(height):
                row = saturn_byte(nor, record + yy * 2)
                for xx in range(5):
                    if row & (1 << xx):
                        points.add((pen_x + xx, yy))
            pen_x += 6
    elif kind == "editline":
        height = 10
        for cp in SPECIMEN:
            record = bases.editline + cp * EDITLINE_RECORD_NIBBLES
            for yy in range(height):
                row = saturn_byte(nor, record + yy * 2)
                for xx in range(6):
                    if row & (1 << xx):
                        points.add((pen_x + xx, yy))
            pen_x += 6
    else:
        raise ValueError(f"unsupported native font kind: {kind}")

    if not points:
        raise ValueError(f"NOR oracle produced an empty {kind} specimen")
    painted_width = max(x for x, _ in points) + 1
    width = max(pen_x, painted_width)
    return width, height, frozenset(points)


def mismatch_count(
    pgm: Pgm,
    origin_y: int,
    expected_width: int,
    expected_height: int,
    expected: frozenset[tuple[int, int]],
) -> int:
    mismatch = 0
    for yy in range(expected_height):
        row = (origin_y + yy) * pgm.width
        for xx in range(expected_width):
            actual = pgm.pixels[row + CONTENT_LEFT + xx] != 255
            wanted = (xx, yy) in expected
            mismatch += actual != wanted
    return mismatch


def verify_kind(
    nor: bytes,
    bases: FontBases,
    kind: str,
    frame_paths: Sequence[Path],
) -> None:
    expected_width, expected_height, expected = render_specimen(nor, bases, kind)
    best: tuple[int, Path | None, int | None] = (10**9, None, None)

    for path in frame_paths:
        pgm = read_pgm(path)
        if pgm.width < CONTENT_LEFT + expected_width:
            raise ValueError(
                f"{path}: width {pgm.width} is too small for {kind} specimen"
            )
        last_y = pgm.height - CONTENT_BOTTOM_MARGIN - expected_height
        for y in range(CONTENT_TOP, last_y + 1):
            mismatch = mismatch_count(
                pgm, y, expected_width, expected_height, expected
            )
            if mismatch < best[0]:
                best = (mismatch, path, y)
            if mismatch == 0:
                print(
                    f"PASS HP39 native {kind} ROM pixel oracle: "
                    f"{path.name} at ({CONTENT_LEFT},{y}), "
                    f"{len(expected)} foreground pixels"
                )
                return

    mismatch, path, y = best
    where = f"{path} at ({CONTENT_LEFT},{y})" if path is not None else "no frame"
    raise SystemExit(
        f"HP39 native {kind} ROM pixel mismatch: best candidate {where} "
        f"still differs at {mismatch} pixel(s)"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("target", choices=sorted(BASES))
    parser.add_argument("nor", type=Path)
    parser.add_argument("--mini", type=Path, nargs="+", required=True)
    parser.add_argument("--main", type=Path, nargs="+", required=True)
    parser.add_argument("--editline", type=Path, nargs="+", required=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    nor = args.nor.read_bytes()
    bases = BASES[args.target]
    verify_editline_fingerprint(nor, bases, args.target)
    verify_kind(nor, bases, "mini", args.mini)
    verify_kind(nor, bases, "main", args.main)
    verify_kind(nor, bases, "editline", args.editline)


if __name__ == "__main__":
    main()
