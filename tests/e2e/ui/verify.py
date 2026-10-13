#!/usr/bin/env python3
"""Verify the device-manager LCD checkpoints independently of guest assertions."""

import argparse
from pathlib import Path


def verify(prefix: Path, height: int) -> None:
    frames = []
    for phase in range(1, 7):
        path = Path(f"{prefix}-{phase}.pgm")
        data = path.read_bytes()
        header = f"P5\n132 {height}\n255\n".encode()
        assert data.startswith(header), f"{path}: geometry"
        pixels = data[len(header) :]
        assert len(pixels) == 132 * height, f"{path}: truncated pixels"
        visible = bytes(pixels[y * 132 + x] for y in range(height) for x in range(131))
        assert set(visible) == {0, 255}, f"{path}: expected populated monochrome UI"
        frames.append(pixels)
    assert len(set(frames)) == 6, f"{prefix}: distinct application states missing"

    # The final action increments the model revision from 1 to 2. Its focused
    # eight-pixel button sits at the viewport bottom; a two-pixel gap separates
    # it from the 24-pixel canvas. Check the model's exact waveform vertices on
    # the physical LCD, without calling the renderer or matching an image hash.
    top = height - 34
    pixels = frames[-1]
    for i in range(32):
        phase = (i + 6) % 32
        sample = min(phase, 31 - phase) * 64
        x = i * 130 // 31
        y = top + 23 - sample * 23 // 1023
        assert pixels[y * 132 + x] == 0, f"{prefix}: missing wave vertex {i} at {x},{y}"
    assert pixels[top * 132 + 65] == 255, f"{prefix}: canvas background overwritten"
    print(f"PASS: {prefix}: six UI states and model-derived LCD waveform")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    for target in (
        Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines()
    ):
        verify(Path(f"build/{target}-frame"), 80 if args.family == "hp4950" else 64)


if __name__ == "__main__":
    main()
