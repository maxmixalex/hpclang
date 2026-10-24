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


def verify_controls(prefix):
    for phase in (300, 400):
        path = Path(f"{prefix}-{phase}.pgm")
        width, height, pixels = read_pgm(path)
        if width != 132 or height not in (64, 80):
            raise AssertionError(f"{path}: unsupported control frame geometry")
        registers = dict(
            (int(k), int(v, 16))
            for k, v in (line.split() for line in path.with_suffix(".lcd").read_text().splitlines())
        )
        if (1 << ((registers[0] >> 1) & 15)) != 4:
            raise AssertionError(f"{path}: expected 16-level control frame")
        for y in range(height):
            for x in range(width):
                ink = False
                for orientation in range(4):
                    local_x, local_y = x - (5 + 30 * orientation), y - 15
                    if 0 <= local_x < 20 and 0 <= local_y < 20:
                        border = local_x in (0, 19) or local_y in (0, 19)
                        # Independently evaluated 60% thresholds for a 20x20 bar.
                        fill = (
                            local_x <= 11
                            if orientation == 0
                            else local_y >= 8
                            if orientation == 1
                            else local_x >= 8
                            if orientation == 2
                            else local_y <= 11
                        )
                        ink = border or fill
                if pixels[y * width + x] != (0 if ink else 255):
                    raise AssertionError(f"{path}: unexpected control pixel at {x},{y}")
    print(f"PASS {prefix}: four progress orientations in both buffer modes")


def verify_text(prefix):
    for phase in (500, 600):
        path = Path(f"{prefix}-{phase}.pgm")
        width, height, pixels = read_pgm(path)
        if width != 132 or height not in (64, 80):
            raise AssertionError(f"{path}: unsupported text frame geometry")
        expected = bytearray([255]) * (width * height)
        # Three-pixel advance: multiline label at (6,16)/(6,19),
        # adjacent entry glyphs at (26,16)/(29,16), including one-pixel inset.
        for x, y, rows in (
            (6, 16, (1, 2, 4)),
            (6, 19, (7, 5, 7)),
            (26, 16, (1, 2, 4)),
            (29, 16, (7, 5, 7)),
        ):
            for dy, row in enumerate(rows):
                for dx in range(3):
                    if row & (1 << dx):
                        expected[(y + dy) * width + x + dx] = 0
        if bytes(pixels) != bytes(expected):
            raise AssertionError(f"{path}: unexpected label/entry pixels")
    print(f"PASS {prefix}: multiline label and entry in both buffer modes")


def verify_image(prefix):
    for phase in (700, 800):
        path = Path(f"{prefix}-{phase}.pgm")
        width, height, pixels = read_pgm(path)
        if width != 132 or height not in (64, 80):
            raise AssertionError(f"{path}: unsupported image frame geometry")
        for y in range(height):
            for x in range(width):
                expected = 255
                if 5 <= x < 14 and 15 <= y < 21:
                    expected = 0 if (x - 5 + y - 15) % 2 == 0 else 255
                if 5 <= x < 7 and 15 <= y < 17:
                    expected = 136 if (x, y) == (5, 15) else 0 if (x, y) == (6, 16) else 255
                if pixels[y * width + x] != expected:
                    raise AssertionError(f"{path}: unexpected image/pattern pixel at {x},{y}")
    print(f"PASS {prefix}: image/pattern composition in both buffer modes")


def verify_menu(prefix):
    for base in (900, 1000):
        for page in (0, 1):
            path = Path(f"{prefix}-{base + page}.pgm")
            width, height, pixels = read_pgm(path)
            if width != 132 or height not in (64, 80):
                raise AssertionError(f"{path}: unsupported menu frame geometry")
            for y in range(height):
                for x in range(width):
                    cell, local = divmod(x, 22)
                    row = y - (height - 6)
                    ink = 0
                    if x < 131 and y >= height - 7 and local <= 20 and (page == 0 or cell == 0):
                        ink = 238 if page else 102 if cell == 1 else 255
                        if 0 <= row < 4 and local == 9 + row:
                            ink = 17 if page else 51 if cell == 1 else 0
                    if pixels[y * width + x] != 255 - ink:
                        raise AssertionError(f"{path}: unexpected menu pixel at {x},{y}")
    print(f"PASS {prefix}: menu pages, disabled colors and labels in both buffer modes")


def verify_checkbox(prefix):
    checked = (0, 0x20, 0x30, 0x1A, 0x0E, 4, 0x3E, 0)
    empty = (0, 0, 0, 0, 0, 0, 0x3E, 0)
    for base in (1100, 1200):
        for focus in (0, 1):
            path = Path(f"{prefix}-{base + focus}.pgm")
            width, height, pixels = read_pgm(path)
            if width != 132 or height not in (64, 80):
                raise AssertionError(f"{path}: unsupported checkbox geometry")
            for y in range(height):
                for x in range(width):
                    ink = 0
                    for index, pattern in enumerate((checked, empty)):
                        local, row = x - (5 + index * 20), y - 15
                        if not (0 <= local < 12 and 0 <= row < 9):
                            continue
                        if focus == index and local <= 6:
                            ink = 255
                        if local <= 8 and pattern[row % 8] & (1 << (local % 8)):
                            ink = 0 if focus == index else 255
                        if index == 0 and 1 <= row <= 3 and local == 7 + row:
                            ink = 255
                    if pixels[y * width + x] != 255 - ink:
                        raise AssertionError(f"{path}: unexpected checkbox pixel at {x},{y}")
    print(f"PASS {prefix}: selected/unselected checkbox focus and labels in both buffer modes")


def verify_choose(prefix):
    for base in (1300, 1400):
        for frame in range(4):
            path = Path(f"{prefix}-{base + frame}.pgm")
            width, height, pixels = read_pgm(path)
            if width != 132 or height not in (64, 80):
                raise AssertionError(f"{path}: unsupported chooser geometry")
            rows = 13 if height == 64 else 17
            first = 40 - rows if frame == 2 else 0
            selected = 39 if frame == 2 else 0
            bottom = rows * 4 + 3
            # The model's built-in menu font is tested separately. Check every
            # content pixel, including the status column, and footer cell edges.
            for y in range(height - 7):
                for x in range(width):
                    ink = 255 if 1 <= x < 6 and 1 <= y < 6 else 0
                    if 2 <= y <= 4 and x == y:
                        ink = 0
                    if frame in (1, 2) and 15 <= x <= 115 and y <= bottom + 1:
                        ink = 0
                        if x <= 114 and y <= bottom and (y in (0, bottom) or x in (15, 114)):
                            ink = 255
                        row, local_y = divmod(y - 2, 4)
                        if 0 <= row < rows and 17 <= x <= 112:
                            chosen = first + row == selected
                            ink = 255 if chosen else 0
                            glyph_x = 3 - local_y if (first + row) % 2 else local_y - 1
                            if local_y >= 1 and x == 18 + glyph_x:
                                ink = 0 if chosen else 255
                    if pixels[y * width + x] != 255 - ink:
                        raise AssertionError(f"{path}: unexpected chooser pixel at {x},{y}")
            if frame in (1, 2):
                for x, expected in ((0, 255), (88, 0), (110, 0), (131, 255)):
                    if pixels[(height - 7) * width + x] != expected:
                        raise AssertionError(f"{path}: unexpected modal footer at {x}")
    print(f"PASS {prefix}: chooser labels, modal scroll/selection and footer edges in both buffers")


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
        verify_controls(Path(f"build/{target}-frame"))
        verify_text(Path(f"build/{target}-frame"))
        verify_image(Path(f"build/{target}-frame"))
        verify_menu(Path(f"build/{target}-frame"))
        verify_checkbox(Path(f"build/{target}-frame"))
        verify_choose(Path(f"build/{target}-frame"))


if __name__ == "__main__":
    main()
