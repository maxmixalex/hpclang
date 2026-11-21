#!/usr/bin/env python3
"""Build a library-free HP39 Aplet to exercise Stock Start, ENTER and Exit."""

import argparse
import importlib.util
from pathlib import Path


def word(value, width=5):
    return f"{value:0{width}x}"[::-1]


def nibbles(data):
    return "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in data)


def pack(text):
    if len(text) % 2:
        raise ValueError("incomplete Aplet byte")
    return bytes.fromhex("".join(text[i : i + 2][::-1] for i in range(0, len(text), 2)))


def independent_template(data):
    code = nibbles(data)
    semi, drop = word(0x312B), word(0x3244)

    def replace(before, after, count):
        nonlocal code
        if len(before) != len(after) or code.count(before) != count:
            raise ValueError("unsupported HP39 Aplet template")
        code = code.replace(before, after)

    def command(index):
        return word(0x2E92) + word(275 + 4096 * index, 6)

    # Preserve the original directory links and all six handler offsets.
    # Both splash sites leave the stack unchanged. Push and drop a hex string
    # of exactly the same encoded size as the original fetch and splash call.
    splash = "".join(map(word, (0x2892D, 0x5C68, 0xF863, 0x3244))) + command(1)
    replace(splash, word(0x2A4E) + word(21) + "0" * 16 + drop, 2)

    # ENTER evaluates the first data-list item with view and current data on
    # the stack. Preserve the normal TopicVar1! writeback and view drop.
    tail = word(0x12CA1) + drop + semi
    replace(command(0) + tail, word(0x775C) + tail + "0" * 6, 1)
    # Padding after a handler's SEMI is outside its executed instruction stream.
    replace(command(2) + semi, semi + "0" * 11, 1)
    return pack(code)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    controls = parser.add_mutually_exclusive_group()
    controls.add_argument(
        "--attached-control", action="store_true", help="retain the historical library ID"
    )
    controls.add_argument(
        "--zero-control", action="store_true", help="use zero as the attachment ID"
    )
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[3] / "tools/package-aplet.py"
    spec = importlib.util.spec_from_file_location("package_aplet", path)
    writer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(writer)
    assets = {
        key: (args.assets / key).read_bytes()
        for key in ("apletcode.bin", "apletepilog.bin", "blank.gro")
    }
    assets["apletcode.bin"] = independent_template(assets["apletcode.bin"])
    marker = b"hpclang-enter"
    # :: DROP "hpclang-enter" ;  ( view old-data -- view new-data )
    action = pack(
        word(0x2D9D)
        + word(0x3244)
        + word(0x2A2C)
        + word(5 + 2 * len(marker))
        + nibbles(marker)
        + word(0x765C)
        + word(0x312B)
    )
    # The program occupies an odd number of nibbles. Precede SEMI with a
    # five-nibble NOP inside the program to make the data-list item byte aligned.
    title = b"EntryProbe"
    output = bytearray(writer.encode_aplet(action, assets, title, b"hpclang"))
    # Dir_Head packs DORRP (five nibbles) and the attached library (three).
    # Stock stores 0x7ff here for a user-saved copy of Function.
    header = 9 + len(title)
    if int.from_bytes(output[header : header + 4], "little") != 0x11302A96:
        raise ValueError("unexpected transfer header")
    if not args.attached_control:
        output[header : header + 4] = (0x02A96 if args.zero_control else 0x7FF02A96).to_bytes(
            4, "little"
        )
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "HPGCC000.000").write_bytes(output)
    (args.output / "HP39DIR.000").write_bytes(
        b"HP39AscA B 12 HPGCC000.000"
        + str(len(title)).encode()
        + b" "
        + title
        + b" D 12 HPCRESET.0009 .HPCRESET "
    )
    (args.output / "HP39DIR.CUR").write_bytes(b"HP39AscA A 11 HP39DIR.0004 Root")
    (args.output / "HPCRESET.000").write_bytes((args.assets / "HPCRESET.000").read_bytes())


if __name__ == "__main__":
    main()
