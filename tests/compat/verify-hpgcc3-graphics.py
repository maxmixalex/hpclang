"""Check original HPGCC3 geometry/colors; font glyph identity is not asserted."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "e2e"))
from report import read_pgm  # noqa: E402


def verify(path, program):
    width, height, pixels = read_pgm(path)
    assert width == 132 and height in (64, 80), f"{path}: geometry"

    def pixel(x, y, value):
        assert pixels[y * width + x] == value, f"{path}: pixel {x},{y}"

    if program in ("hello", "gray"):
        ink = 0 if program == "hello" else 85
        assert any(pixels[y * width + x] == ink for y in range(16) for x in range(100)), (
            f"{path}: text missing"
        )
        for y in range(height):
            for x in range(131):
                value = pixels[y * width + x]
                assert value in (255, ink), f"{path}: text color"
                if y >= 16:
                    pixel(x, y, 255)
    else:
        for top, bottom, right in ((0, 10, 40), (20, 32, 65), (35, 48, 65)):
            assert any(
                pixels[y * width + x] == 0 for y in range(top, bottom) for x in range(right)
            ), f"{path}: font region at {top} missing"
        for y in range(height):
            for x in range(49, 131):
                if 20 <= y < 50 and x < 65:
                    continue  # Native/fallback text metrics vary by model/context.
                if 66 <= x <= 74 and 66 <= y <= 74:
                    continue  # Circle center/axes are checked below, independent of raster rounding.
                expected = 255
                if 50 <= x <= 60 and y <= 10:
                    expected = 119
                if x == 70 and 5 <= y <= 25:
                    expected = 0
                if (x in (50, 55) and 60 <= y <= 70) or (50 <= x <= 55 and y in (60, 70)):
                    expected = 0
                pixel(x, y, expected)
        if height == 80:
            for x, y in ((70, 70), (66, 70), (74, 70), (70, 66), (70, 74)):
                pixel(x, y, 170)
    print(f"PASS {program}: original geometry, colors, text regions and {height}-line clipping")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frame", type=Path)
    parser.add_argument("program", choices=("hello", "gray", "shapes"))
    args = parser.parse_args()
    verify(args.frame, args.program)
