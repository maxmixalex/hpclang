#!/usr/bin/env python3
"""Check every gray-bar pixel, status segment and DMA layout independently."""

import argparse
from pathlib import Path


def verify(prefix: Path, height: int) -> None:
    addresses = []
    for levels, bits in ((2, 1), (4, 2), (16, 4)):
        for reverse in (0, 1):
            stem = f"{prefix}-{levels * 2 + reverse}"
            data = Path(stem + ".pgm").read_bytes()
            header = f"P5\n132 {height}\n255\n".encode()
            assert data.startswith(header), f"{stem}: PGM geometry"
            pixels = data[len(header) :]
            assert len(pixels) == 132 * height, f"{stem}: pixel count"
            lut = (
                (0, 7, 10, 15)
                if levels == 4
                else tuple(i * 15 // (levels - 1) for i in range(levels))
            )
            for y in range(height):
                for x in range(132):
                    color = x * levels // 131
                    if x == 131:
                        expected = 0 if y < 6 and (y + reverse) % 2 else 255
                    else:
                        if reverse:
                            color = levels - 1 - color
                        expected = 255 - 17 * lut[color]
                    assert pixels[y * 132 + x] == expected, (
                        f"{stem}: pixel ({x},{y}) {pixels[y * 132 + x]} != {expected}"
                    )
            registers = {
                int(k): int(v, 16)
                for k, v in (line.split() for line in Path(stem + ".lcd").read_text().splitlines())
            }
            assert registers[0] & 0x61 == 0x21, f"{stem}: enabled STN"
            assert 1 << ((registers[0] >> 1) & 15) == bits, f"{stem}: hardware bpp"
            assert registers[4] & 3 == 2 - (bits.bit_length() - 1), f"{stem}: swap"
            scan_width = (((registers[2] >> 8) & 2047) + 1) * 4
            rows = ((registers[1] >> 14) & 1023) + 1
            assert rows >= height and scan_width >= 132
            stride = scan_width * bits // 8
            assert registers[7] == stride // 2, f"{stem}: stride"
            start = registers[5] << 1
            end = start + rows * stride
            assert registers[6] == (end >> 1) & 0x1FFFFF, f"{stem}: exclusive end"
            assert start >> 22 == end >> 22, f"{stem}: bank boundary"
            addresses.append(start)
        assert addresses[-1] != addresses[-2], f"{prefix}: no page exchange"
    for frame in (100, 101, 102):
        data = Path(f"{prefix}-{frame}.pgm").read_bytes()
        header = f"P5\n132 {height}\n255\n".encode()
        assert data.startswith(header) and len(data) == len(header) + 132 * height
        pixels = data[len(header) :]
        if frame == 100:
            assert pixels[130] == pixels[(height - 1) * 132] == 0
            assert pixels[65] == 255
        elif frame == 101:
            # Original example ends at x=131, outside the 131-pixel display.
            # The endpoint is clipped before rasterization with integer y.
            clipped_y = (height - 1) * 130 // 131
            assert pixels[0] == pixels[clipped_y * 132 + 130] == 0
            assert pixels[65] == 255
        else:
            assert pixels[5 * 132 + 55] == 255 - 0x88
            assert pixels[(height - 10) * 132 + 70] == 255 - 0x55
            assert pixels[15 * 132 + 70] == 0
            assert pixels[(height - 20) * 132 + 50] == 0
            for top, bottom in ((0, 10), (20, 30), (35, 43)):
                assert any(
                    pixels[y * 132 + x] == 0 for y in range(top, bottom) for x in range(45)
                ), f"{prefix}: missing historical font band {top}"
    adapter_frames = {}
    for frame in range(110, 115):
        data = Path(f"{prefix}-{frame}.pgm").read_bytes()
        header = f"P5\n132 {height}\n255\n".encode()
        assert data.startswith(header) and len(data) == len(header) + 132 * height
        pixels = data[len(header) :]
        adapter_frames[frame] = pixels
        if frame == 110:
            assert pixels[5 * 132 + 55] == 119
            assert pixels[15 * 132 + 70] == 0
            assert pixels[5 * 132 + 80 : 5 * 132 + 83] == bytes((0, 255, 136))
            assert pixels[6 * 132 + 80 : 6 * 132 + 83] == bytes((136, 255, 0))
            assert pixels[3 * 132 + 131] == 0
            for top, bottom in ((0, 6), (10, 18), (20, 28)):
                assert any(pixels[y * 132 + x] == 0 for y in range(top, bottom) for x in range(32))
        else:
            for y in range(height):
                for x in range(132):
                    if frame == 114:
                        expected = 136 if 1 <= x <= 10 and 1 <= y <= 10 else 255
                    else:
                        left = 20 if frame == 112 else 2
                        segment = 2 if frame == 112 else 1
                        expected = (
                            0
                            if (left <= x <= left + 6 and 2 <= y <= 8 or x == 131 and y == segment)
                            else 255
                        )
                    assert pixels[y * 132 + x] == expected, (frame, x, y)
    assert adapter_frames[111] == adapter_frames[113], "HPG lost the previous front page"
    print(f"PASS: {prefix}: gray/DMA checks and historical graphics")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    for target in Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines():
        verify(Path(f"build/{target}-frame"), 80 if args.family == "hp4950" else 64)


if __name__ == "__main__":
    main()
