#!/usr/bin/env python3
"""Build a library-free HP39 Aplet to exercise Stock Start, ENTER and Exit."""

import argparse
import importlib.util
import runpy
import struct
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


def return_probe():
    # Preserve incoming r0, load the marker, write it into the aligned copy,
    # restore r0 and return through lr. The two literal slots follow bx lr.
    return struct.pack(
        "<8I",
        0xE92D0001,
        0xE59F0008,
        0xE58F0008,
        0xE8BD0001,
        0xE12FFF1E,
        0x48504139,
        0x12345678,
        0,
    )


def arm_entry(blob):
    blob += bytes(-len(blob) % 8)
    code_for = runpy.run_path(str(Path(__file__).with_name("build.py")))["code_for"]
    offset = len(code_for(len(blob), 0, "hp39gplus")) - 13
    body = code_for(len(blob), offset, "hp39gplus") + nibbles(blob) + "0" * (2 * len(blob) + 7)
    return word(0x2DCC) + word(5 + len(body)) + body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    entry = parser.add_mutually_exclusive_group()
    entry.add_argument(
        "--arm-entry", action="store_true", help="exercise the HP39 ARM call and return"
    )
    entry.add_argument(
        "--c-probe", type=Path, help="embed probe.bin from the independent C entry build"
    )
    entry.add_argument(
        "--bitmap-pool", type=Path, help="run a C probe with a Stock bitmap on the data stack"
    )
    parser.add_argument(
        "--bitmap-image",
        type=Path,
        help="push a raw HPC1 image below the bitmap for a staging probe",
    )
    controls = parser.add_mutually_exclusive_group()
    controls.add_argument(
        "--attached-control", action="store_true", help="retain the historical library ID"
    )
    controls.add_argument(
        "--zero-control", action="store_true", help="use zero as the attachment ID"
    )
    parser.add_argument("--initial-note", help="seed the Aplet data with a Notebook note")
    parser.add_argument(
        "--aplet-writeback", action="store_true", help="publish the editor's current Aplet data"
    )
    args = parser.parse_args()
    if args.aplet_writeback and not args.bitmap_image:
        parser.error("--aplet-writeback requires --bitmap-image")
    if args.bitmap_image and not args.bitmap_pool:
        parser.error("--bitmap-image requires --bitmap-pool")
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
    action = word(0x2D9D)
    if args.aplet_writeback:
        # TWO{}N creates an owned list containing the input and a padding object.
        # 256 bytes cover a full 191-byte note, signature and String header.
        padding = 512
        action += word(0x2A4E) + word(5 + padding) + "0" * padding + word(0xFAA1)
    if args.arm_entry:
        action += arm_entry(return_probe())
    elif args.bitmap_pool is not None:
        if args.bitmap_image:
            image = args.bitmap_image.read_bytes()
            if len(image) < 16 or len(image) * 2 + 5 > 0xFFFFF:
                parser.error("image does not fit a Stock string")
            magic, size, entry_offset, _ = struct.unpack_from("<4sIII", image)
            if magic != b"HPC1" or size != len(image) - 16 or entry_offset >= size:
                parser.error("invalid raw HPC1 image")
            action += word(0x2A2C) + word(5 + 2 * len(image)) + nibbles(image)
        # Stock MAKEGROB consumes two system binaries; DROP releases the stack reference.
        rows = runpy.run_path(str(Path(__file__).with_name("build.py")))["bitmap_rows"]
        action += (
            word(0x2911)
            + word(1024)
            + word(0x2911)
            + word(rows(args.aplet_writeback))
            + word(0x1D622)
        )
        action += arm_entry(args.bitmap_pool.read_bytes()) + word(0x3244)
        if args.bitmap_image:
            action += word(0x3244)
    elif args.c_probe is not None:
        action += arm_entry(args.c_probe.read_bytes())
    if args.aplet_writeback:
        action += word(0x4053) + word(0xF476) + word(0x6E2F)  # ONE NTHCOMPDROP TOTEMPOB
    elif args.initial_note is None:
        action += word(0x3244) + word(0x2A2C) + word(5 + 2 * len(marker)) + nibbles(marker)
    if len(action) % 2 == 0:
        action += word(0x765C)  # NOP keeps the complete data-list item byte aligned.
    action = pack(action + word(0x312B))
    title = b"EntryProbe"
    note = (
        None if args.initial_note is None else b"hpclang-notebook/1\n" + args.initial_note.encode()
    )
    output = bytearray(writer.encode_aplet(action, assets, title, b"hpclang", note))
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
