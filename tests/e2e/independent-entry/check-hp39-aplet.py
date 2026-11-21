#!/usr/bin/env python3
"""Verify ENTER writeback in the named HP39 Aplet directory after Stock exit."""

import argparse
import json
import runpy
import struct
from pathlib import Path


def word(value, width=5):
    return f"{value:0{width}x}"[::-1]


def nibbles(data):
    return "".join(f"{b & 15:x}{b >> 4:x}" for b in data)


def check(aplet, sram, copy_state=None, arm_entry=False, c_probe=None, check_workspace=None):
    title = b"EntryProbe"
    header = b"HP39BinB" + bytes([len(title)]) + title
    if not aplet.startswith(header):
        raise ValueError("unexpected transfer title")
    directory = nibbles(aplet[len(header) :])
    if not directory.startswith(word(0x2A96) + word(0x7FF, 3)):
        raise ValueError("unexpected Aplet attachment")
    # The seventh application-data field is an empty list in the transfer.
    # It follows the two origin coordinates and precedes the outer list SEMI.
    empty = word(0x2A74) + word(0x312B)
    origins = (word(0x2911) + word(0)) * 2
    tail = origins + empty + word(0x312B)
    if directory.count(tail) != 1:
        raise ValueError("missing initial custom-data slot")
    slot = directory.index(tail) + len(origins)
    link = slot + len(empty) + 5
    marker = b"hpclang-enter"
    saved = word(0x2A2C) + word(5 + len(marker) * 2) + nibbles(marker)

    def with_custom_data(data):
        growth = len(data) - len(empty)
        result = directory
        for pos in (8, link):
            offset = int(result[pos : pos + 5][::-1], 16)
            result = result[:pos] + word(offset + growth) + result[pos + 5 :]
        return result[:slot] + data + result[slot + len(empty) :]

    # Both the length-prefixed directory name and its trailing length belong
    # to the Stock directory entry. Transfer buffers contain a different name.
    memory = nibbles(sram)

    def require_entry(title, body):
        name = b"'" + title
        size = word(len(name), 2)
        prefix = size + nibbles(name) + size
        variants = [body]
        if arm_entry:
            # The source retains the seed. Only the aligned execution copy may
            # overwrite it. Stock can move the enclosing directory during Exit.
            source = bytes.fromhex(
                "01002de908009fe508008fe50100bde81eff2fe1394150487856341200000000"
            )
            before = nibbles(source)
            reserve = len(before) + 7
            if body.count(before + "0" * reserve) != 1:
                raise ValueError("missing ARM source and reserved execution area")
            pos = body.index(before) + len(before)
            after = nibbles(source[:24] + source[20:24] + source[28:])
            variants = [
                body[:pos] + "0" * pad + after + "0" * (7 - pad) + body[pos + reserve :]
                for pad in range(8)
            ]
        elif c_probe is not None:
            meta = json.loads((c_probe / "metadata.json").read_text())
            blob = (c_probe / "probe.bin").read_bytes()
            blob += bytes(-len(blob) % 8)
            source = nibbles(blob)
            reserve = len(source) + 7
            if blob.hex() != meta["blob"] or body.count(source + "0" * reserve) != 1:
                raise ValueError("C probe does not match the Aplet")
            pos = body.index(source) + len(source)
            start = prefix + body[:pos]
            if memory.count(start) != 1:
                raise ValueError("missing named C probe directory")
            idx = memory.index(start) + len(start)
            area = memory[idx : idx + reserve]
            pad = area.find(source[:64])
            if not 0 <= pad <= 7:
                raise ValueError("missing executed C probe")
            packed = area[pad : pad + len(source)]
            actual = bytes(int(packed[i : i + 2][::-1], 16) for i in range(0, len(packed), 2))
            base = None
            if check_workspace is None:
                workspace = meta["symbols"]["workspace"]
                marker = struct.unpack_from("<I", actual, workspace)[0]
                base = struct.unpack_from("<I", actual, workspace + 16)[0] - workspace
                if (
                    marker != 0x48504343
                    or base % 4
                    or not 0x08000000 <= base <= 0x08000000 + len(sram) - len(blob)
                ):
                    raise ValueError("invalid C workspace or execution address")
            # Stock may relocate the directory after the payload records its addresses.
            check_payload = runpy.run_path(str(Path(__file__).with_name("verify.py")))[
                "check_payload"
            ]
            check_payload(actual, meta, base, check_workspace)
            variants = [body[:pos] + "0" * pad + packed + "0" * (7 - pad) + body[pos + reserve :]]
        if sum(memory.count(prefix + variant) for variant in variants) != 1:
            raise ValueError(f"saved named Aplet directory did not match {title!r}")

    require_entry(title, with_custom_data(saved))
    if copy_state is not None:
        require_entry(b"2", with_custom_data(word(0x5DA2) if copy_state == "reset" else saved))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aplet", type=Path)
    parser.add_argument("sram", type=Path)
    parser.add_argument("--copy-state", choices=("saved", "reset"))
    entry = parser.add_mutually_exclusive_group()
    entry.add_argument("--arm-entry", action="store_true")
    entry.add_argument("--c-probe", type=Path, help="directory with probe.bin and metadata.json")
    entry.add_argument(
        "--rpl-pool", type=Path, help="build directory for the paused RPL page probe"
    )
    entry.add_argument(
        "--bitmap-pool", type=Path, help="build directory for the stack-held bitmap probe"
    )
    parser.add_argument("--bitmap-image", type=Path, help="HPC1 image expected in the staged pages")
    args = parser.parse_args()
    if args.bitmap_image and not args.bitmap_pool:
        parser.error("--bitmap-image requires --bitmap-pool")
    workspace_check = None
    report = {}
    if args.rpl_pool or args.bitmap_pool:
        verifier = Path(__file__).resolve().parents[1] / "independent-pool/verify.py"
        validate = runpy.run_path(str(verifier))[
            "check_bitmap_workspace" if args.bitmap_pool else "check_rpl_workspace"
        ]

        def workspace_check(words):
            report.update(validate(words, sram))
            if args.bitmap_image:
                image = args.bitmap_image.read_bytes()
                if len(image) != 45472 or struct.unpack_from("<4sIII", image) != (
                    b"HPC1",
                    45456,
                    8,
                    1048,
                ):
                    raise ValueError("unexpected HP39g+ Notebook image")
                checksum = 2166136261
                for byte in image[16:]:
                    checksum = ((checksum ^ byte) * 16777619) & 0xFFFFFFFF
                if tuple(words[44:46]) != (checksum, len(image) - 16):
                    raise ValueError("Notebook readback differs from the expected code image")
                report.update(image_bytes=len(image) - 16, code_fnv1a=f"{checksum:08x}")

    sram = args.sram.read_bytes()
    check(
        args.aplet.read_bytes(),
        sram,
        args.copy_state,
        args.arm_entry,
        args.c_probe or args.rpl_pool or args.bitmap_pool,
        workspace_check,
    )
    if report:
        print(json.dumps(report))


if __name__ == "__main__":
    main()
