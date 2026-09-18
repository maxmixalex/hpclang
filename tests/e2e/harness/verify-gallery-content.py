#!/usr/bin/env python3
"""Require every generated Font Gallery capture to paint its content area."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

FRAME_RE = re.compile(r"^(\d+)-(sample|text)\.pgm$")
LCD_WIDTH = 131
CONTENT_LEFT = 13
CONTENT_RIGHT = 118
CONTENT_TOP = 17
CONTENT_BOTTOM_MARGIN = 3


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


def content_ink(path: Path) -> int:
    width, height, pixels = read_pgm(path)
    if width != LCD_WIDTH or height not in (64, 80):
        raise ValueError(f"{path}: unexpected Gallery geometry {width}x{height}")

    bottom = height - CONTENT_BOTTOM_MARGIN
    ink = 0
    for y in range(CONTENT_TOP, bottom):
        row = y * width
        for x in range(CONTENT_LEFT, CONTENT_RIGHT + 1):
            if pixels[row + x] != 255:
                ink += 1
    return ink


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Verify generated Font Gallery pages paint the content rectangle"
    )
    parser.add_argument("frames", type=Path)
    parser.add_argument("--native-face-count", type=int, default=2)
    args = parser.parse_args()

    if args.native_face_count < 0:
        raise SystemExit("--native-face-count must be non-negative")

    if not args.frames.is_dir():
        raise SystemExit(f"Gallery frame directory not found: {args.frames}")

    pages: dict[int, dict[str, Path]] = {}
    for path in sorted(args.frames.glob("*.pgm")):
        match = FRAME_RE.match(path.name)
        if not match:
            continue
        index = int(match.group(1))
        if index < args.native_face_count:
            continue
        pages.setdefault(index, {})[match.group(2)] = path

    if not pages:
        raise SystemExit("no generated Font Gallery captures found")

    minimum: tuple[int, Path] | None = None
    checked = 0
    for index in sorted(pages):
        face_pages = pages[index]
        missing = {"sample", "text"} - face_pages.keys()
        if missing:
            raise SystemExit(
                f"Gallery face {index:04d} missing page(s): {', '.join(sorted(missing))}"
            )
        for page_name in ("sample", "text"):
            path = face_pages[page_name]
            try:
                ink = content_ink(path)
            except ValueError as exc:
                raise SystemExit(str(exc)) from exc
            if ink == 0:
                raise SystemExit(
                    f"{path}: generated Gallery {page_name} page has no content-area ink"
                )
            checked += 1
            if minimum is None or ink < minimum[0]:
                minimum = (ink, path)

    assert minimum is not None
    print(
        f"Gallery content oracle: {len(pages)} generated faces / {checked} captures; "
        f"minimum body ink {minimum[0]} pixels ({minimum[1].name})"
    )


if __name__ == "__main__":
    main()
