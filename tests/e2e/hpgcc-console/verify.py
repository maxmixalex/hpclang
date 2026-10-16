#!/usr/bin/env python3
"""Check historical console cells, scrolling and edited input independently."""

import argparse
from pathlib import Path


def verify(prefix: Path, height: int) -> None:
    console_frames = {}
    for frame in (120, 121, 122):
        data = Path(f"{prefix}-{frame}.pgm").read_bytes()
        header = f"P5\n132 {height}\n255\n".encode()
        assert data.startswith(header) and len(data) == len(header) + 132 * height
        console_frames[frame] = data[len(header) :]
    text = console_frames[120]
    assert any(text[y * 132 + x] == 0 for y in range(6) for x in range(8))
    for y in range(6):
        assert text[y * 132 : y * 132 + 8] == text[(y + 18) * 132 : (y + 18) * 132 + 8]
        assert text[y * 132 + 8 : y * 132 + 128] == bytes([255]) * 120
        assert console_frames[122][y * 132 : y * 132 + 8] == text[y * 132 : y * 132 + 8]
    for y in range(8, 13):
        assert text[y * 132 + 100] == 0, "text output erased unrelated graphics"
        assert console_frames[121][(y - 6) * 132 + 100] == 0, "graphics did not scroll"
    for y in range(height):
        for x in range(128):
            expected = text[(y + 6) * 132 + x] if y + 6 < height // 6 * 6 else 255
            assert console_frames[121][y * 132 + x] == expected, ("console scroll", x, y)
    print(f"PASS: {prefix}: historical console pixels")


def verify_editor(prefix: Path, height: int) -> None:
    """Compare the physical-key editor result with an independently drawn frame."""
    frames = {}
    header = f"P5\n132 {height}\n255\n".encode()
    for frame in (123, 124):
        data = Path(f"{prefix}-{frame}.pgm").read_bytes()
        assert data.startswith(header) and len(data) == len(header) + 132 * height, (
            f"{prefix}-{frame}: invalid editor capture"
        )
        frames[frame] = data[len(header) :]
    assert 0 in frames[124], f"{prefix}: empty editor reference"
    assert frames[123] == frames[124], (
        f"{prefix}: edited console differs from independently drawn expected frame"
    )
    print(f"PASS: {prefix}: physical-key line editing, bounded input and scroll undo")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    for target in Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines():
        verify(Path(f"build/{target}-frame"), 80 if args.family == "hp4950" else 64)
        verify_editor(Path(f"build/{target}-frame"), 80 if args.family == "hp4950" else 64)


if __name__ == "__main__":
    main()
