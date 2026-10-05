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

RESET_PROGRAM_NAME = ".HPCRESET"


def word(value: int, width: int = 5) -> list[int]:
    return [(value >> (4 * i)) & 15 for i in range(width)]


def rpl_string(text: str) -> list[int]:
    data = text.encode("ascii")
    return (
        word(0x2A2C)
        + word(5 + 2 * len(data))
        + [nibble for byte in data for nibble in (byte & 15, byte >> 4)]
    )


def reset_views_epilog(data: bytes) -> bytes:
    """Install native Start/Reset views and relocate the following field link."""
    nibbles = [nibble for byte in data for nibble in (byte & 15, byte >> 4)]
    empty_list = word(0x2A74) + word(0x312B)
    # DirCode, backward field link, anonymous NoteText, backward link,
    # anonymous sketch list. Reject a different SDK layout before packaging.
    expected = empty_list + word(12) + [0, 0] + rpl_string("") + word(12) + [0, 0] + empty_list
    if nibbles != expected:
        raise ValueError("unsupported HPGCC39 Aplet epilog layout")

    def view(prompt: str, program: str, number: int) -> list[int]:
        return (
            word(0x2A74)
            + rpl_string(prompt)
            + rpl_string(program)
            + word(0x2911)
            + word(number)
            + word(0x312B)
        )

    views = (
        word(0x2A74) + view("Start", "", 15) + view("Reset", RESET_PROGRAM_NAME, 10) + word(0x312B)
    )
    result = views + word(12 + len(views) - len(empty_list)) + nibbles[15:]
    if len(result) % 2:
        raise ValueError("Aplet epilog must end on a byte boundary")
    return bytes(result[i] | result[i + 1] << 4 for i in range(0, len(result), 2))


def reset_program() -> bytes:
    # Stock's named Reset program runs in the program context. Save that
    # context, select the active Aplet, call the template's _Reset (INT_04),
    # then restore the program context. The library supplies confirmation.
    entries = (0x9660, 0x12706, 0x960E, 0x38655, 0x960E)
    source = "".join(f"SYSEVAL {entry}:" for entry in entries)
    return f"HP39AscD {len(RESET_PROGRAM_NAME)} {RESET_PROGRAM_NAME}{source}".encode("ascii")


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
        "\tfclose(inputFile); //clean up\n\t/* outputFile is already closed by the writer. */",
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

    epilog = data / "apletepilog.bin"
    epilog.write_bytes(reset_views_epilog(epilog.read_bytes()))
    (data / "HPCRESET.000").write_bytes(reset_program())


if __name__ == "__main__":
    main()
