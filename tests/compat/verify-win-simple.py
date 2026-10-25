"""Check migrated original application geometry; native-font glyphs are excluded."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "e2e"))
from report import read_pgm  # noqa: E402


def layout(path, selected):
    width, height, pixels = read_pgm(path)
    assert width in (131, 132) and height in (64, 80), f"{path}: geometry"
    bars = (
        (0, 10, 80, 5, 0),
        (20, 20, 5, height - 40, 1),
        (60, 20, 5, height - 40, 3),
        (0, height - 15, 80, 5, 2),
    )
    fraction = 0.1 if selected else 0
    for y in range(10, height - 8):
        for x in range(width):
            ink = False
            for left, top, w, h, orientation in bars:
                local_x, local_y = x - left, y - top
                if not (0 <= local_x < w and 0 <= local_y < h):
                    continue
                ink = local_x in (0, w - 1) or local_y in (0, h - 1)
                ink |= (
                    local_x <= int((w - 1) * fraction)
                    if orientation == 0
                    else local_y >= int(h * (1 - fraction))
                    if orientation == 1
                    else local_x >= int(w * (1 - fraction))
                    if orientation == 2
                    else local_y <= int((h - 1) * fraction)
                )
            assert pixels[y * width + x] == (0 if ink else 255), (
                f"{path}: slider/background {x},{y}"
            )
    pattern = (0, 0x20, 0x30, 0x1A, 0x0E, 4, 0x3E, 0) if selected else (0, 0, 0, 0, 0, 0, 0x3E, 0)
    for y in range(8):
        for x in range(8):
            expected = 0 if pattern[y] & (1 << x) else 255
            assert pixels[y * width + 75 + x] == expected, f"{path}: checkbox {x},{y}"
    assert any(pixels[y * width + x] == 0 for y in range(8) for x in range(22)), f"{path}: label"
    assert any(pixels[y * width + x] == 0 for y in range(height - 7, height) for x in range(131)), (
        f"{path}: menu"
    )


def verify(prefix, final):
    for frame in range(2100, 2115):
        path = Path(f"{prefix}-{frame}.pgm")
        width, height, _ = read_pgm(path)
        assert width == 132 and height in (64, 80), f"{path}: intermediate capture"
    layout(Path(f"{prefix}-2100.pgm"), False)
    layout(Path(f"{prefix}-2114.pgm"), True)
    layout(final, True)
    print(
        f"PASS {prefix}: 15 application frames, responsive slider geometry, values, checkbox and final rendering"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--final", type=Path, required=True)
    args = parser.parse_args()
    verify(args.prefix, args.final)
