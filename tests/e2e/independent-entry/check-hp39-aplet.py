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


def check(aplet, sram, copy_state=None, arm_entry=False, c_probe=None):
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
            check_payload(actual, meta, base)
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
    args = parser.parse_args()
    check(
        args.aplet.read_bytes(),
        args.sram.read_bytes(),
        args.copy_state,
        args.arm_entry,
        args.c_probe,
    )


if __name__ == "__main__":
    main()
