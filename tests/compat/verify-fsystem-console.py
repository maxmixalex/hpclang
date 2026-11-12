"""Verify original filesystem console frames against ROM or packaged font glyphs."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "e2e"))
from report import read_pgm  # noqa: E402


def load_glyphs(path):
    packed = path.read_bytes()
    nibbles = bytes(value for byte in packed for value in (byte & 15, byte >> 4))
    # Independently reviewed 4x6 ASCII shapes, low bit at the left edge.
    anchors = {" ": bytes(6), "A": bytes((2, 5, 7, 5, 5, 0)), "E": bytes((7, 1, 3, 1, 7, 0))}
    candidates = []
    position = nibbles.find(anchors["A"])
    while position >= 0:
        base = position - ord("A") * 6
        if (
            base >= 0
            and base + 127 * 6 <= len(nibbles)
            and all(
                nibbles[base + ord(char) * 6 : base + (ord(char) + 1) * 6] == shape
                for char, shape in anchors.items()
            )
        ):
            candidates.append(base)
        position = nibbles.find(anchors["A"], position + 1)
    if len(candidates) != 1:
        raise ValueError("ROM must contain exactly one reviewed mini ASCII atlas")
    base = candidates[0]
    glyphs = {
        chr(code): bytes(
            0 if nibbles[base + code * 6 + row] & (1 << column) else 255
            for row in range(6)
            for column in range(4)
        )
        for code in range(32, 127)
    }
    return glyphs, {"rom_sha256": hashlib.sha256(packed).hexdigest(), "atlas_nibble": base}


def load_packaged_glyphs(path):
    source = path.read_bytes()
    glyphs = {}
    for line in source.decode("utf-8").splitlines():
        if not line.startswith("G "):
            continue
        code, x, y, width, height, advance, stride, *data = map(int, line[2:].split(","))
        if not 32 <= code < 127:
            continue
        if (x, y, height, stride) != (0, 0, 6, 1) or not 1 <= width <= 5 or len(data) != 6:
            raise ValueError("Unexpected packaged mini-font geometry")
        cell = bytearray([255] * 24)
        for row in range(6):
            for column in range(width):
                if data[row] & (128 >> column):
                    destination = column if width <= 4 else column * 4 // width
                    cell[row * 4 + destination] = 0
        glyphs[chr(code)] = bytes(cell)
    if len(glyphs) != 95:
        raise ValueError("Incomplete packaged ASCII atlas")
    return glyphs, {"font_sha256": hashlib.sha256(source).hexdigest()}


def line_pixels(text, glyphs):
    if len(text) > 32:
        raise ValueError("Expected console line exceeds 32 cells")
    return bytes(
        glyphs[char][row * 4 + column]
        for row in range(6)
        for char in text.ljust(32)
        for column in range(4)
    )


def verify(directory, model, rom=None, *, mode="stock", font=None):
    if mode == "stock":
        if rom is None:
            raise ValueError("Stock verification requires --rom")
        glyphs, report = load_glyphs(rom)
    elif mode == "direct":
        if font is None:
            font = (
                Path(__file__).resolve().parents[2] / "runtime/fonts/generated/newrpl_font6_menus.S"
            )
        glyphs, report = load_packaged_glyphs(font)
    else:
        raise ValueError("Unknown console execution mode")
    first = [
        "Volume 0 (:3: C:) mounted.",
        None,
        "Curr. vol. changed to :3:",
        "Dir:: :3:/",
        "KEEP.TXT | 9 b",
        "SEED0.TXT | 6 b",
        "SEED1.TXT | 6 b",
        "SEED2.TXT | 6 b",
        "More...",
    ]
    listing = (
        first[2:]
        + [f"SEED{i}.TXT | 6 b" for i in range(3, 6)]
        + [
            "Test Dir | 0 b",
            "8 files, 45 bytes",
        ]
    )
    writes = (
        first[:3]
        + ["Test dir already present"]
        + [
            "Test file 1 open returned:",
            "No error",
        ]
        * 3
        + ["rename=No error"]
    )
    expected = [first, listing, listing[1:] + ["Exit"], writes, writes + ["Exit"]]
    if mode == "direct":
        # The Direct harness seeds a directory-backed card with three entries;
        # its 32-cell capacity line wraps before the explicit newline.
        first = (
            first[:2]
            + [""]
            + first[2:4]
            + [
                "HPCLANG.OUT | 30 b",
                "HPCLANG.TST | 16 b",
                "ZZHPDIR | 0 b",
                "SEED0.TXT | 6 b",
                "More...",
            ]
        )
        listing = (
            first[5:]
            + [f"SEED{i}.TXT | 6 b" for i in range(1, 6)]
            + [
                "Test Dir | 0 b",
                "More...",
            ]
        )
        totals = listing[1:] + ["10 files, 82 bytes"]
        writes = (
            first[:4]
            + ["Test dir already present"]
            + [
                "Test file 1 open returned:",
                "No error",
            ]
            * 3
            + ["rename=No error"]
        )
        expected = [first, listing, totals, totals[1:] + ["Exit"], writes, writes[1:] + ["Exit"]]
    report.update(model=model, mode=mode, passed=False, frames=[])
    for index, lines in enumerate(expected, 2300):
        path = directory / f"{model}-frame-{index}.pgm"
        width, height, pixels = read_pgm(path)
        if (width, height) != (132, 80):
            raise ValueError(f"{path}: expected HP49g+/HP50g 132x80 capture")
        for row, text in enumerate(lines + [""] * (13 - len(lines))):
            actual = bytes(pixels[(row * 6 + y) * width + x] for y in range(6) for x in range(128))
            if text is None:
                # Card capacity/free space can vary with fixture formatting.
                decoded = ""
                for cell in range(32):
                    raster = bytes(
                        actual[y * 128 + cell * 4 + x] for y in range(6) for x in range(4)
                    )
                    matches = [
                        char for char in "Size: Kb,kBfre.0123456789" if glyphs[char] == raster
                    ]
                    matches = set(matches)
                    if len(matches) != 1:
                        raise ValueError(f"{path}: undecodable capacity cell {cell}")
                    decoded += matches.pop()
                match = re.fullmatch(r"Size: (\d+) Kb, (\d+) kB free\. *", decoded)
                if not match or not 0 < int(match[2]) < int(match[1]):
                    raise ValueError(f"{path}: invalid capacity/free-space output {decoded!r}")
            elif actual != line_pixels(text, glyphs):
                raise ValueError(f"{path}: incorrect console row {row}: expected {text!r}")
        # The capture has a padded 132nd column; only 131 columns are visible.
        if any(
            pixels[y * width + x] != 255
            for y in range(80)
            for x in range(131)
            if x >= 128 or y >= 78
        ):
            raise ValueError(f"{path}: ink outside console cells")
        report["frames"].append(
            {"name": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        )
    report["passed"] = True
    print(f"PASS {mode}/{model}: original directory pagination, file results and both exit screens")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
    parser.add_argument("--mode", choices=("direct", "stock"), default="stock")
    parser.add_argument("--rom", type=Path, help="Caller-provided packed full NOR image (Stock)")
    parser.add_argument("--font", type=Path, help="Packaged mini-font source (Direct)")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    report = verify(args.directory, args.model, args.rom, mode=args.mode, font=args.font)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
