#!/usr/bin/env python3
"""Check visible physical-input text against the independently supplied literal."""

import argparse
from pathlib import Path


def verify(prefix: Path, height: int) -> None:
    header = f"P5\n132 {height}\n255\n".encode()
    initial = Path(f"{prefix}-199.pgm").read_bytes()
    assert initial.startswith(header), f"{prefix}: initial PGM geometry"
    initial = initial[len(header) :]
    assert len(initial) == 132 * height, f"{prefix}: initial pixel count"
    assert all(initial[y * 132 + x] == 255 for y in range(height) for x in range(131)), (
        f"{prefix}: initial display was not cleared"
    )
    for frame in (200, 201):
        final = Path(f"{prefix}-{frame}.pgm").read_bytes()
        assert final.startswith(header), f"{prefix}/{frame}: PGM geometry"
        final = final[len(header) :]
        assert len(final) == 132 * height, f"{prefix}/{frame}: pixel count"
        actual = bytes(final[y * 132 + x] for y in range(2, 18) for x in range(131))
        expected = bytes(final[y * 132 + x] for y in range(20, 36) for x in range(131))
        assert actual == expected, f"{prefix}/{frame}: input text differs from reference band"
        assert min(expected) < max(expected), f"{prefix}/{frame}: blank reference band"
    print(f"PASS: {prefix}: physical-input text and reference pixels agree")


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
