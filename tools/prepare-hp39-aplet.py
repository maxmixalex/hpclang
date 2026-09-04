#!/usr/bin/env python3
"""Extract only the stock HP39 Aplet packaging assets from the HPGCC39 kit."""

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

ASSETS = (
    "LIB275L.L39",
    "apletcode.bin",
    "apletepilog.bin",
    "blank.gro",
)


def find_one(root: Path, name: str) -> Path:
    matches = [p for p in root.rglob("*") if p.is_file() and p.name.lower() == name.lower()]
    if not matches:
        raise FileNotFoundError(name)
    matches.sort(key=lambda p: ("39" not in str(p).lower(), len(p.parts), str(p)))
    return matches[0]


def normalize_hp2aplet(source: str) -> str:
    text = source

    text = re.sub(
        r"char\s+grobFileName\s*\[[^\]]+\]\s*;[^\n]*",
        "char grobFileName[1024];",
        text,
        count=1,
    )

    for name in ("header", "apletName", "authorName", "graphics", "grobFileName"):
        text = text.replace(
            f"fgets({name},500,config);",
            f"fgets({name},sizeof({name}),config);",
        )

    text = text.replace(
        "strlen(apletName),apletName",
        "(int)strlen(apletName),apletName",
    )

    text = text.replace('strcat(slack,"\\\\bin\\\\");', 'strcat(slack,"\\\\");')
    text = text.replace('strcat(slack,"/bin/");', 'strcat(slack,"/");')

    for name in ASSETS:
        text = text.replace(f'"bin/{name}"', f'"{name}"')

    text = text.replace(
        "\tfclose(inputFile); //clean up\n\tfclose(outputFile);",
        "\tfclose(inputFile); //clean up\n"
        "\t/* outputFile is already closed by the writer. */",
        1,
    )

    return text


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    src = find_one(args.source, "hp2aplet.c")
    data = args.destination / "data"
    data.mkdir(parents=True, exist_ok=True)
    (args.destination / "src").mkdir(parents=True, exist_ok=True)

    (args.destination / "src" / "hp2aplet.c").write_text(
        normalize_hp2aplet(src.read_text(errors="replace"))
    )

    for name in ASSETS:
        shutil.copy2(find_one(args.source, name), data / name)


if __name__ == "__main__":
    main()
