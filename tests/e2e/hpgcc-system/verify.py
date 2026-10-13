#!/usr/bin/env python3
"""Check the historical timed-sieve report independently of its status code."""

import argparse
from pathlib import Path


def verify(prefix: Path, height: int) -> None:
    header = f"P5\n132 {height}\n255\n".encode()
    data = Path(f"{prefix}-204.pgm").read_bytes()
    assert data.startswith(header) and len(data) == len(header) + 132 * height
    pixels = data[len(header) :]
    # x=131 is DMA padding outside the 131-pixel physical display.
    actual = bytes(pixels[y * 132 + x] for y in range(12) for x in range(131))
    expected = bytes(pixels[y * 132 + x] for y in range(32, 44) for x in range(131))
    assert actual == expected and 0 in expected, f"{prefix}: historical sieve report"
    assert any(pixels[y * 132 + x] == 0 for y in range(12, 18) for x in range(131)), (
        f"{prefix}: missing elapsed time"
    )
    print(f"PASS: {prefix}: historical timed-sieve client")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    targets = Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines()
    for target in targets:
        verify(Path(f"build/{target}-frame"), 80 if args.family == "hp4950" else 64)


if __name__ == "__main__":
    main()
