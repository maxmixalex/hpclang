#!/usr/bin/env python3
"""Pack a clean hpclang ARM ELF into a Launcher V3 HP transfer string.

Unlike historical elf2hp, this tool does not depend on ELF section names,
ELFIO, GNU binutils, HPGCC runtime objects, or the old r10/GOT model.

The initialized ARM image is reconstructed from ELF PT_LOAD program headers in
LMA (p_paddr) order. This is the representation expected by the clean hp39gs
loader: code/rodata at load offset zero and initialized RWPI data at the LMA
assigned by runtime/hp39gs/link.ld.

The outer HP transfer-string framing and Launcher V3 simple M+R structure are
carried forward from the hardware-proven 39gsX Python packer.
"""
from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path

PT_LOAD = 1
MMU_PACKER_WITH_OS_RAM = 0x00C00000
STACK_DEPTH_BLOCKS = 1


@dataclass(frozen=True)
class Segment:
    offset: int
    vaddr: int
    paddr: int
    filesz: int
    memsz: int
    flags: int
    align: int


def parse_elf32(path: Path) -> tuple[bytes, int, list[Segment]]:
    blob = path.read_bytes()
    if len(blob) < 52 or blob[:4] != b"\x7fELF":
        raise ValueError("not an ELF file")
    if blob[4] != 1 or blob[5] != 1:
        raise ValueError("expected ELF32 little-endian")

    (
        e_type,
        e_machine,
        _e_version,
        e_entry,
        e_phoff,
        _e_shoff,
        _e_flags,
        _e_ehsize,
        e_phentsize,
        e_phnum,
        _e_shentsize,
        _e_shnum,
        _e_shstrndx,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", blob, 16)

    if e_type != 2 or e_machine != 0x28:
        raise ValueError("expected ARM ET_EXEC ELF")
    if e_phentsize < 32:
        raise ValueError("invalid ELF32 program-header size")

    segments: list[Segment] = []
    for index in range(e_phnum):
        offset = e_phoff + index * e_phentsize
        if offset + 32 > len(blob):
            raise ValueError("truncated program-header table")

        (
            p_type,
            p_offset,
            p_vaddr,
            p_paddr,
            p_filesz,
            p_memsz,
            p_flags,
            p_align,
        ) = struct.unpack_from("<IIIIIIII", blob, offset)

        if p_type != PT_LOAD or p_filesz == 0:
            continue
        if p_offset + p_filesz > len(blob):
            raise ValueError("truncated PT_LOAD contents")

        segments.append(
            Segment(
                offset=p_offset,
                vaddr=p_vaddr,
                paddr=p_paddr,
                filesz=p_filesz,
                memsz=p_memsz,
                flags=p_flags,
                align=p_align,
            )
        )

    if not segments:
        raise ValueError("ELF has no initialized PT_LOAD segments")
    return blob, e_entry, segments


def build_load_image(
    blob: bytes, segments: list[Segment]
) -> tuple[bytes, list[Segment]]:
    ordered = sorted(segments, key=lambda segment: (segment.paddr, segment.offset))

    # runtime/hp39gs/loader.S uses ADR as the packed-image origin and its
    # __data_load linker constant is expressed from that same origin.
    if ordered[0].paddr != 0:
        raise ValueError(
            f"first PT_LOAD LMA must be zero, got 0x{ordered[0].paddr:x}"
        )

    end = 0
    for segment in ordered:
        if segment.paddr < end:
            raise ValueError("overlapping PT_LOAD LMAs are not supported")
        end = segment.paddr + segment.filesz

    image = bytearray(end)
    for segment in ordered:
        image[segment.paddr : segment.paddr + segment.filesz] = blob[
            segment.offset : segment.offset + segment.filesz
        ]

    return bytes(image), ordered


def image_entry_offset(entry: int, segments: list[Segment]) -> int:
    for segment in segments:
        if segment.vaddr <= entry < segment.vaddr + segment.memsz:
            offset = segment.paddr + (entry - segment.vaddr)
            if offset >= segment.paddr + segment.filesz:
                raise ValueError("ELF entry point lies outside initialized bytes")
            return offset

    raise ValueError(f"ELF entry point 0x{entry:x} is outside PT_LOAD segments")


def convert(src: Path, dst: Path) -> None:
    blob, elf_entry, segments = parse_elf32(src)
    image, segments = build_load_image(blob, segments)
    entry = image_entry_offset(elf_entry, segments)

    # Launcher V3 simple format:
    # marker 0B34C (nibble order in the byte stream below) plus the
    # 8-nibble ARM-alignment scratch word "A>CP".
    l3 = bytearray(b"\x4c\xb3\x80A>CP")
    l3 += image

    # Optional function table: zero exported secondary entry points.
    l3 += struct.pack("<I", 0)

    memreq = MMU_PACKER_WITH_OS_RAM | ((STACK_DEPTH_BLOCKS & 0xF) << 24)
    l3 += struct.pack("<I", memreq)

    # Primary entry offset, base address, Launcher V3 linker marker.
    l3 += struct.pack("<III", entry, 0, 0x3176B34C)

    # HP binary-transfer framing for a string object.
    nibbles = (2 * len(l3) + 5) << 4
    if nibbles > 0xFFFFFF:
        raise ValueError("HP string is too large")

    output = (
        b"HPHP49-C"
        + b"\x2c\x2a"
        + nibbles.to_bytes(4, "little")[:3]
        + bytes(l3)
    )

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(output)
    print(
        f"{dst}: {len(output)} bytes, "
        f"ARM image={len(image)} bytes, entry=0x{entry:x}, "
        f"PT_LOADs={len(segments)}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="clean ARM ELF")
    parser.add_argument("output", type=Path, help="HP transfer string")
    args = parser.parse_args()
    convert(args.input, args.output)


if __name__ == "__main__":
    main()
