#!/usr/bin/env python3
"""Verify the device-manager LCD checkpoints independently of guest assertions."""

import argparse
from pathlib import Path


def read_frame(path: Path, height: int) -> bytes:
    data = path.read_bytes()
    header = f"P5\n132 {height}\n255\n".encode()
    assert data.startswith(header), f"{path}: geometry"
    pixels = data[len(header) :]
    assert len(pixels) == 132 * height, f"{path}: truncated pixels"
    visible = bytes(pixels[y * 132 + x] for y in range(height) for x in range(131))
    assert set(visible) == {0, 255}, f"{path}: expected populated monochrome UI"
    return pixels


def waveform(pixels: bytes, top: int, prefix: Path) -> None:
    for i in range(32):
        phase = (i + 6) % 32
        sample = min(phase, 31 - phase) * 64
        x = i * 130 // 31
        y = top + 23 - sample * 23 // 1023
        assert pixels[y * 132 + x] == 0, f"{prefix}: missing wave vertex {i} at {x},{y}"
    assert pixels[top * 132 + 65] == 255, f"{prefix}: canvas background overwritten"


def verify(prefix: Path, height: int, stock: bool = False) -> None:
    frames = [read_frame(Path(f"{prefix}-{phase}.pgm"), height) for phase in range(1, 8)]
    assert len(set(frames)) == 7, f"{prefix}: distinct application/theme states missing"

    # The final action increments the model revision from 1 to 2. Its focused
    # eight-pixel button sits at the viewport bottom; a two-pixel gap separates
    # it from the 24-pixel canvas. Check the model's exact waveform vertices on
    # the physical LCD, without calling the renderer or matching an image hash.
    top = height - 34
    pixels = frames[5]
    waveform(pixels, top, prefix)
    # Replacing the renderer keeps the same model, geometry and focused control.
    # Custom canvas paint owns its palette; the surrounding themed UI reverses.
    for y in range(height):
        for x in range(131):
            original = pixels[y * 132 + x]
            expected = original if top <= y < top + 24 else 255 - original
            assert frames[6][y * 132 + x] == expected, (
                f"{prefix}: theme/canvas boundary mismatch at {x},{y}"
            )
    # Stock default fonts have eight-row line metrics on all six models. A
    # Direct context without a ROM mapping must use the six-row packaged font.
    candidates = [size for size in (6, 8) if Path(f"{prefix}-{100 + size}.pgm").exists()]
    assert len(candidates) == 1, f"{prefix}: missing/ambiguous font checkpoint"
    line_height = candidates[0]
    assert not stock or line_height == 8, f"{prefix}: Stock native font unavailable"
    font_pixels = read_frame(Path(f"{prefix}-{100 + line_height}.pgm"), height)
    waveform(font_pixels, height - (28 + line_height), prefix)
    pressure_pixels = read_frame(Path(f"{prefix}-121.pgm"), height)
    assert pressure_pixels == font_pixels, f"{prefix}: allocator exhaustion changed the live UI"
    rebuilt_pixels = read_frame(Path(f"{prefix}-122.pgm"), height)
    assert rebuilt_pixels == font_pixels, f"{prefix}: renderer reconstruction changed the live UI"
    if line_height == 6:
        assert font_pixels == frames[6], f"{prefix}: packaged fallback changed the view"
    else:
        assert font_pixels != frames[6], f"{prefix}: native font did not alter layout"
    print(f"PASS: {prefix}: UI states, waveform, independent theme and font layout")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    for target in Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines():
        verify(
            Path(f"build/{target}-frame"),
            80 if args.family == "hp4950" else 64,
            args.mode == "stock",
        )


if __name__ == "__main__":
    main()
