#!/usr/bin/env python3
"""Check saved-note LCD content across the three Stock launches."""

import argparse
from pathlib import Path


def font_rows(source: Path) -> dict:
    glyphs = {}
    for line in source.read_text().splitlines():
        if not line.startswith("G "):
            continue
        code, x, y, width, height, advance, stride, *data = map(int, line[2:].split(","))
        glyphs[code] = (x, y, width, height, advance, stride, data)
    return glyphs


def check_line(pixels: bytes, glyphs: dict, top: int, text: str, path: Path) -> None:
    expected = bytearray([255] * (131 * 6))
    origin = 0
    for character in text:
        x, y, width, height, advance, stride, data = glyphs[ord(character)]
        for row in range(height):
            for column in range(width):
                if data[row * stride + column // 8] & (0x80 >> (column % 8)):
                    px, py = origin + x + column, y + row
                    if 0 <= px < 131 and 0 <= py < 6:
                        expected[py * 131 + px] = 0
        origin += advance
    actual = bytes(
        pixels[row * 131 + column] for row in range(top, top + 6) for column in range(131)
    )
    assert actual == expected, f"{path}: missing or incorrect line {text!r} at y={top}"


def verify(build: Path, target: str, height: int, source: Path) -> None:
    glyphs = font_rows(source)
    for index, (text, status) in enumerate(
        (
            ("First saved note", "Saved; exit normally"),
            ("Second saved note", "Saved; exit normally"),
            ("Second saved note", "Persistence verified"),
        ),
        1,
    ):
        name = target if index == 1 else f"{target}-lifecycle-{index}"
        path = build / f"{name}-pass.pgm"
        frame = path.read_bytes()
        header = f"P5\n131 {height}\n255\n".encode()
        assert frame.startswith(header) and len(frame) == len(header) + 131 * height
        pixels = frame[len(header) :]
        for top, line in ((0, "Notebook"), (16, "Alpha to type"), (24, text), (32, status)):
            check_line(pixels, glyphs, top, line, path)
    print(f"PASS: {target}: notebook content across three Stock launches")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("stock",))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    for target in Path(f"build/stock-{args.family}-targets.txt").read_text().splitlines():
        verify(
            Path("build"),
            target,
            80 if args.family == "hp4950" else 64,
            root / "runtime/fonts/generated/newrpl_6a.S",
        )


if __name__ == "__main__":
    main()
