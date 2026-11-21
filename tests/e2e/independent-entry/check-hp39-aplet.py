#!/usr/bin/env python3
"""Verify ENTER writeback in the named HP39 Aplet directory after Stock exit."""

import argparse
from pathlib import Path


def word(value, width=5):
    return f"{value:0{width}x}"[::-1]


def nibbles(data):
    return "".join(f"{b & 15:x}{b >> 4:x}" for b in data)


def check(aplet, sram):
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
    growth = len(saved) - len(empty)
    for pos in (8, link):
        offset = int(directory[pos : pos + 5][::-1], 16)
        directory = directory[:pos] + word(offset + growth) + directory[pos + 5 :]
    expected = directory[:slot] + saved + directory[slot + len(empty) :]
    # Both the length-prefixed directory name and its trailing length belong
    # to the Stock directory entry. Transfer buffers contain a different name.
    name = b"'" + title
    size = word(len(name), 2)
    if nibbles(sram).count(size + nibbles(name) + size + expected) != 1:
        raise ValueError("saved named Aplet directory did not match")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aplet", type=Path)
    parser.add_argument("sram", type=Path)
    args = parser.parse_args()
    check(args.aplet.read_bytes(), args.sram.read_bytes())


if __name__ == "__main__":
    main()
