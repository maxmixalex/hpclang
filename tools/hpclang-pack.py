#!/usr/bin/env python3
"""Pack a linked hpclang v1 ELF into the model-independent HPC1 image."""
from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path

PT_LOAD = 1
PF_W = 2
CODE_BASE = 0x01220000
DATA_BASE = 0x01101000


@dataclass(frozen=True)
class Segment:
    offset: int
    vaddr: int
    paddr: int
    filesz: int
    memsz: int
    flags: int


def parse_elf32(path: Path) -> tuple[bytes, int, list[Segment]]:
    blob = path.read_bytes()
    if blob[:4] != b"\x7fELF" or blob[4:6] != b"\x01\x01":
        raise ValueError("expected ELF32 little-endian")

    (
        e_type,
        e_machine,
        _,
        entry,
        phoff,
        _,
        _,
        _,
        phentsize,
        phnum,
        _,
        _,
        _,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", blob, 16)

    if e_type != 2 or e_machine != 0x28:
        raise ValueError("expected ARM executable")

    segments: list[Segment] = []
    for i in range(phnum):
        off = phoff + i * phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, _ = (
            struct.unpack_from("<IIIIIIII", blob, off)
        )
        if p_type == PT_LOAD:
            segments.append(
                Segment(p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags)
            )

    return blob, entry, segments


def pack(src: Path, dst: Path) -> None:
    blob, entry, segments = parse_elf32(src)

    initialized = [s for s in segments if s.filesz]
    if not initialized:
        raise ValueError("ELF has no initialized PT_LOAD payload")

    image_end = max(s.paddr + s.filesz for s in initialized)
    image = bytearray(image_end - CODE_BASE)

    for s in initialized:
        start = s.paddr - CODE_BASE
        if start < 0:
            raise ValueError("initialized segment lies below hpclang code base")
        image[start : start + s.filesz] = blob[s.offset : s.offset + s.filesz]

    rw_end = max(
        (s.vaddr + s.memsz for s in segments if s.flags & PF_W),
        default=DATA_BASE,
    )
    rw_static_size = rw_end - DATA_BASE

    header = struct.pack(
        "<4sIII",
        b"HPC1",
        len(image),
        entry - CODE_BASE,
        rw_static_size,
    )

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(header + image)

    print(
        f"{dst}: payload={len(image)} bytes "
        f"entry=0x{entry:08x} rw_static={rw_static_size} bytes"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    pack(args.input, args.output)


if __name__ == "__main__":
    main()
