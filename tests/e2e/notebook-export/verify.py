#!/usr/bin/env python3
"""Check both export launches after independent raw-card verification."""

import argparse
from pathlib import Path
import runpy


def verify(build, target, root):
    resources = runpy.run_path(str(root / "tests/e2e/notebook-entry/verify.py"))["verify_resources"]
    lifecycle = runpy.run_path(str(root / "tests/e2e/harness/stock-lifecycle.py"))["verify_result"]
    pixels = runpy.run_path(str(root / "tests/e2e/notebook/verify.py"))
    glyphs = pixels["font_rows"](root / "runtime/fonts/generated/newrpl_6a.S")
    for index in range(2):
        name = target if index == 0 else f"{target}-lifecycle-2"
        lifecycle(build / f"{name}-stock.json", f"0x4f53b00{index}")
        resources(build, name)
        frame = build / f"{name}-pass.pgm"
        header = b"P5\n131 80\n255\n"
        data = frame.read_bytes()
        assert data.startswith(header) and len(data) == len(header) + 131 * 80
        for top, text in ((0, "Notebook"), (16, "Alpha to type"), (32, "Export verified")):
            pixels["check_line"](data[len(header) :], glyphs, top, text, frame)
    print(f"PASS: {target}: export resources, viewport and settled return across two launches")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("stock",))
    parser.add_argument("family", choices=("hp4950",))
    parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    for target in Path("build/stock-hp4950-targets.txt").read_text().splitlines():
        verify(Path("build"), target, root)


if __name__ == "__main__":
    main()
