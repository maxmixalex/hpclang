#!/usr/bin/env python3
"""Verify the actual Notebook editor before ON and settled Stock return."""

import argparse
import json
from pathlib import Path
import runpy


def verify_resources(build, target):
    path = build / f"{target}-frame-memory-1.json"
    report = json.loads(path.read_text())
    assert set(report) == {"version", "id", "words"}
    assert type(report["version"]) is int and report["version"] == 1
    assert type(report["id"]) is int and report["id"] == 1
    words = report["words"]
    assert isinstance(words, list) and len(words) == 8
    assert all(type(value) is int and 0 <= value <= 0xFFFFFFFF for value in words)
    version, entry, main, final, stack_limit, heap, heap_limit, rw_bytes = words
    assert version == 1 and stack_limit == 8192
    assert 0 < entry <= main <= final < stack_limit
    assert all(value % 4 == 0 for value in (entry, main, final))
    assert rw_bytes % 4096 == 0 and rw_bytes > stack_limit
    assert 0 < heap <= heap_limit < rw_bytes - stack_limit
    print(f"PASS: {target}: touched stack {final}/{stack_limit}, reserved heap {heap}/{heap_limit}")
    return words


def verify(build, target, height, root):
    helpers = runpy.run_path(str(root / "tests/e2e/notebook/verify.py"))
    glyphs = helpers["font_rows"](root / "runtime/fonts/generated/newrpl_6a.S")
    frame = build / f"{target}-frame-1.pgm"
    magic, dimensions, maximum, pixels = frame.read_bytes().split(b"\n", 3)
    width, rows = map(int, dimensions.split())
    assert magic == b"P5" and maximum == b"255"
    assert width in (131, 132) and rows == height and len(pixels) == width * height
    # Hardware captures may include one padding column beyond the visible LCD.
    visible = b"".join(pixels[row * width : row * width + 131] for row in range(height))
    for top, text in ((0, "Notebook"), (16, "Alpha to type"), (24, "")):
        helpers["check_line"](visible, glyphs, top, text, frame, focused=top == 24)
    report = json.loads((build / f"{target}-stock.json").read_text())
    assert report["status"] == "pass"
    assert all(report[key] is True for key in ("saw_pass", "returned", "return_settled"))
    verify_resources(build, target)
    print(f"PASS: {target}: real focused editor, ON exit and settled Stock return")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("stock",))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    for target in Path(f"build/stock-{args.family}-targets.txt").read_text().splitlines():
        verify(Path("build"), target, 80 if args.family == "hp4950" else 64, root)


if __name__ == "__main__":
    main()
