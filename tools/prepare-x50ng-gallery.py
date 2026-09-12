#!/usr/bin/env python3
"""Add the focused Font Gallery frontend to an already prepared x50ng tree."""

from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(text: str, old: str, new: str, what: str) -> str:
    if text.count(old) != 1:
        raise SystemExit(f"Font Gallery x50ng patch: expected one {what} anchor")
    return text.replace(old, new, 1)


def copy_frontend(source: Path, destination: Path) -> None:
    text = source.read_text()
    text = replace_once(
        text,
        "#define TEST_RAM_SIZE 0x00100000u",
        "#define TEST_RAM_SIZE 0x00200000u",
        "reserved test RAM size",
    )
    text = replace_once(
        text,
        "#define STAGE_PAGES 96u",
        "#define STAGE_PAGES 224u",
        "synthetic stage page count",
    )
    text = replace_once(
        text,
        "#define INJECT_PAGES 159u",
        "#define INJECT_PAGES 224u",
        "synthetic launcher page count",
    )
    text = replace_once(
        text,
        "int main(int argc, char **argv)",
        "int hpclang_gallery_main(int argc, char **argv)",
        "frontend main()",
    )
    destination.write_text(text)


def expand_reserved_ram(root: Path) -> None:
    path = root / "src" / "module.c"
    text = path.read_text()
    text = replace_once(
        text,
        "phys_ram_size += 0x00100000;",
        "phys_ram_size += 0x00200000;",
        "reserved hpclang E2E RAM",
    )
    path.write_text(text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--gallery-frontend", type=Path, required=True)
    args = parser.parse_args()

    root = args.x50ng
    expand_reserved_ram(root)
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
