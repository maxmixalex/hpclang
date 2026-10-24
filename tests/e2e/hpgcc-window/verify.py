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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", nargs="+", type=Path)
    for frame in parser.parse_args().frames:
        verify(frame)


if __name__ == "__main__":
    main()
