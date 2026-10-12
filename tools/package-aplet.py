#!/usr/bin/env python3
"""Package an HP39 Aplet using an explicitly supplied historical host tool/assets."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ASSETS = ("apletcode.bin", "apletepilog.bin", "blank.gro", "LIB275L.L39", "HPCRESET.000")
OUTPUTS = ("HPGCC000.000", "LIB275L.000", "HPCRESET.000", "HP39DIR.000", "HP39DIR.CUR")


def label(value: str, maximum: int, field: str) -> bytes:
    if not 1 <= len(value) <= maximum or any(not 32 <= ord(char) <= 126 for char in value):
        raise ValueError(f"{field} must contain 1..{maximum} printable ASCII characters")
    return value.encode("ascii")


def package(payload: Path, output: Path, tool: Path, assets: Path, name: str, author: str) -> None:
    title = label(name, 48, "name")
    # The historical splash uses 66 - 2*length without clipping negative x.
    label(author, 33, "author")
    image = payload.read_bytes()
    if image[:8] != b"HPHP49-C":
        raise ValueError("expected a wrapped ARMToolbox launcher")
    data = {name: (assets / name).read_bytes() for name in ASSETS}
    # Bound the historical writer's 300000-byte buffer before invoking it.
    extent = len(image) + sum(len(data[name]) for name in ASSETS[:3])
    if extent + 2 * len(title) + len(author) + 128 > 300000:
        raise ValueError("application exceeds the historical Aplet writer capacity")
    if not data["HPCRESET.000"].startswith(b"HP39AscD "):
        raise ValueError("assets must include the prepared Aplet Reset program")
    with tempfile.TemporaryDirectory(prefix="hpclang-aplet-") as directory:
        work = Path(directory)
        (work / "hp39prj.dat").write_text(
            f"HP39 Aplet Project\n{name}\n{author}\nN\n", encoding="ascii"
        )
        result = subprocess.run(
            [str(tool.resolve()), str(payload.resolve())],
            cwd=work,
            env=dict(os.environ, HPGCC=str(assets.resolve())),
            capture_output=True,
            text=True,
        )
        if result.returncode:
            raise RuntimeError(f"Aplet writer failed:\n{result.stdout}\n{result.stderr}")
        aplet = (work / "HPGCC000.000").read_bytes()
        if not aplet.startswith(b"HP39BinB" + bytes([len(title)]) + title):
            raise ValueError("Aplet writer produced an invalid title/header")
        if image[8:] not in aplet:
            raise ValueError("Aplet writer omitted the application")
        (work / "LIB275L.000").write_bytes(data["LIB275L.L39"])
        (work / "HPCRESET.000").write_bytes(data["HPCRESET.000"])

        def entry(kind: str, filename: str, title: str) -> str:
            return f"{kind} {len(filename)} {filename}{len(title)} {title}"

        listing = (
            "HP39AscA "
            + " ".join(
                (
                    entry("B", "HPGCC000.000", name),
                    entry("H", "LIB275L.000", "LIB275L"),
                    entry("D", "HPCRESET.000", ".HPCRESET"),
                )
            )
            + " "
        )
        (work / "HP39DIR.000").write_bytes(listing.encode("ascii"))
        (work / "HP39DIR.CUR").write_bytes(b"HP39AscA A 11 HP39DIR.0004 Root")
        output.mkdir(parents=True, exist_ok=True)
        for filename in OUTPUTS:
            shutil.copyfile(work / filename, output / filename)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--author", default="hpclang")
    args = parser.parse_args()
    package(args.payload, args.output, args.tool, args.assets, args.name, args.author)


if __name__ == "__main__":
    main()
