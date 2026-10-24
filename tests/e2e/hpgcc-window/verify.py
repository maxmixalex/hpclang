"""Check the independently captured final window frame, including cleared trails."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from report import read_pgm  # noqa: E402


def verify(path):
    width, height, pixels = read_pgm(path)
    if width != 131 or height not in (64, 80):
        raise AssertionError(f"{path}: unsupported frame geometry")
    for y in range(height):
        for x in range(width):
            expected = 0 if 19 <= x <= 28 and 12 <= y <= 21 else 255
            if pixels[y * width + x] != expected:
                raise AssertionError(f"{path}: unexpected pixel at {x},{y}")
    print(f"PASS {path}: final rectangle and cleared background")


def verify_keys(prefix):
    # Independent expected physical status intensities for the 22 GPIO edges.
    alpha = [
        255,
        255,
        0,
        0,
        0,
        0,
        255,
        255,
        255,
        153,
        153,
        153,
        153,
        153,
        153,
        153,
        255,
        0,
        0,
        0,
        0,
        0,
    ]
    for base in (100, 200):
        addresses = []
        for edge, a in enumerate(alpha, 1):
            path = Path(f"{prefix}-{base + edge}.pgm")
            width, height, pixels = read_pgm(path)
            if width != 132 or height not in (64, 80):
                raise AssertionError(f"{path}: unsupported status frame geometry")
            left = 255 if edge in (13, 14) else 0
            right = 255 if edge in (19, 20) else 0
            for y in range(height):
                for x in range(width):
                    ink = {1: left, 2: right, 3: a}.get(y, 0) if x == 131 else 0
                    if pixels[y * width + x] != 255 - ink:
                        raise AssertionError(f"{path}: unexpected key/status pixel at {x},{y}")
            regs = dict(
                (int(k), int(v, 16))
                for k, v in (
                    line.split() for line in path.with_suffix(".lcd").read_text().splitlines()
                )
            )
            if (1 << ((regs[0] >> 1) & 15)) != 4:
                raise AssertionError(f"{path}: expected 16-level LCD mode")
            addresses.append(regs[5])
        for before, after in zip(addresses, addresses[1:]):
            if (before != after) != (base == 200):
                raise AssertionError(f"{prefix}: incorrect page exchange for mode {base}")
    print(f"PASS {prefix}: 44 physical-key status frames and page exchange")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    targets = Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines()
    if not targets:
        raise AssertionError("No executed targets to verify")
    for target in targets:
        verify(Path(f"build/{target}-pass.pgm"))
        verify_keys(Path(f"build/{target}-frame"))


if __name__ == "__main__":
    main()
