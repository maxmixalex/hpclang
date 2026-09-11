#!/usr/bin/env python3
"""Add the focused Font Gallery frontend to an already prepared x50ng tree."""

from __future__ import annotations

import argparse
from pathlib import Path


def copy_frontend(source: Path, destination: Path) -> None:
    text = source.read_text()
    old = "int main(int argc, char **argv)"
    if text.count(old) != 1:
        raise SystemExit(f"{source}: expected exactly one frontend main()")
    destination.write_text(text.replace(old, "int hpclang_gallery_main(int argc, char **argv)", 1))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--gallery-frontend", type=Path, required=True)
    args = parser.parse_args()

    root = args.x50ng
    copy_frontend(args.gallery_frontend, root / "src" / "hpclang_gallery.c")

    makefile = root / "Makefile"
    lines = makefile.read_text().splitlines(keepends=True)
    matches = []
    for index, line in enumerate(lines):
        logical = line.strip()
        if logical.endswith("\\"):
            logical = logical[:-1].rstrip()
        if logical.endswith("src/main.c"):
            matches.append(index)
    if len(matches) != 1:
        raise SystemExit(
            f"{makefile}: expected one src/main.c source line, found {len(matches)}"
        )

    index = matches[0]
    original = lines[index]
    leading = original[: len(original) - len(original.lstrip())]
    continuation = chr(92)
    lines[index:index + 1] = [
        f"{leading}./src/hpclang_gallery.c {continuation}\n",
        original,
    ]
    makefile.write_text("".join(lines))


if __name__ == "__main__":
    main()
