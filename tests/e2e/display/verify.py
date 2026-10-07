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
    print(f"PASS: {prefix}: six frames, pixels/status/DMA layout/page exchange")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    for target in Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines():
        verify(Path(f"build/{target}-frame"), 80 if args.family == "hp4950" else 64)


if __name__ == "__main__":
    main()
