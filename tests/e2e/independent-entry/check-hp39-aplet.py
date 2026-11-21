#!/usr/bin/env python3
"""Verify ENTER writeback in the named HP39 Aplet directory after Stock exit."""

import argparse
from pathlib import Path


def word(value, width=5):
    return f"{value:0{width}x}"[::-1]


def nibbles(data):
    return "".join(f"{b & 15:x}{b >> 4:x}" for b in data)


def check(aplet, sram, copy_state=None):
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
        if memory.count(size + nibbles(name) + size + body) != 1:
            raise ValueError(f"saved named Aplet directory did not match {title!r}")

    require_entry(title, with_custom_data(saved))
    if copy_state is not None:
        require_entry(b"2", with_custom_data(word(0x5DA2) if copy_state == "reset" else saved))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aplet", type=Path)
    parser.add_argument("sram", type=Path)
    parser.add_argument("--copy-state", choices=("saved", "reset"))
    args = parser.parse_args()
    check(args.aplet.read_bytes(), args.sram.read_bytes(), args.copy_state)


if __name__ == "__main__":
    main()
