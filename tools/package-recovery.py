#!/usr/bin/env python3
"""Internal HP50g recovery CODE envelope; not an HPC1 application launcher.

The payload must be a trusted position-independent ARM entry at offset zero.
It owns BSS initialization, its stack, hardware state and return through LR.
This envelope supplies aligned object-owned storage, not MMU/page allocation.
Only the HP50g Stock main-RAM mapping has been validated for this entry.
"""

from __future__ import annotations

import argparse
from pathlib import Path


MAX_INITIALIZED = 32768
MAX_NIBBLES = 0xFFFFF


def immediate(value: int, width: int = 5) -> str:
    if not 0 <= value < 1 << (4 * width):
        raise ValueError("Saturn immediate overflow")
    return "".join(f"{(value >> (4 * i)) & 15:x}" for i in range(width))


def entry(initialized_size: int, source_offset: int) -> str:
    """Encode the independently implemented Saturn SAVE/copy/ARMSYS/GETPTR path.

    Offsets and lengths are in nibbles except initialized_size (bytes).
    The counted copy transfers complete 16-nibble blocks. Clearing the low
    four destination bits aligns the ARM image to eight bytes at every CODE
    nibble alignment. Fifteen extra reserved nibbles cover the rounding.
    """
    if not 8 <= initialized_size <= MAX_INITIALIZED or initialized_size % 8:
        raise ValueError("initialized image must contain 1..4096 eight-byte blocks")
    code = "8f" + immediate(0x679B) + "20" + "81b5"  # SAVE, P=0, C=PC
    code += "da" + "34" + immediate(source_offset) + "c2" + "134"
    code += "da" + "34" + immediate(initialized_size * 2 + 15) + "c2"
    code += "80880" + "80881" + "80882" + "80883" + "135" + "d5"
    code += "34" + immediate(initialized_size // 8 - 1)
    code += "1527" + "1517" + "16f" + "17f" + "ce" + "5fe"
    # Convert the object-owned Saturn destination to the HP50g ARM SRAM address.
    code += "af2" + "d9" + "81e" + "25" + "32" + "080" + "20"
    code += "80bfe" + "8f" + immediate(0x67D2) + "142164808c"
    return code


def package(initialized: bytes, runtime_size: int, *, model: str) -> bytes:
    """Return an HP binary RPL program with one CODE object and no result marker.

    runtime_size includes initialized bytes, BSS, workspace and owned stack.
    The caller must check ELF relocation safety before providing raw bytes.
    Bounds here prevent truncated Saturn lengths and copy/reservation overlap;
    they do not establish free calculator RAM or a sufficient transfer budget.
    """
    if model != "hp50g":
        raise ValueError("independent recovery entry is validated only for hp50g")
    if not initialized or len(initialized) > MAX_INITIALIZED:
        raise ValueError("initialized image size must be 1..32768 bytes")
    initialized += bytes((-len(initialized)) % 8)
    if runtime_size < len(initialized) or runtime_size % 8:
        raise ValueError("runtime size must be eight-byte aligned and contain initialized image")
    code = entry(len(initialized), 0)
    # C=PC captures the address 13 nibbles after the start of this entry.
    code = entry(len(initialized), len(code) - 13)
    reserve = runtime_size * 2 + 15
    body_size = len(code) + len(initialized) * 2 + reserve
    if body_size + 5 > MAX_NIBBLES:
        raise ValueError("recovery CODE object exceeds Saturn length capacity")
    payload = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in initialized)
    body = code + payload + "0" * reserve
    program = (
        immediate(0x2D9D)  # DOCOL
        + immediate(0x2DCC)  # CODE
        + immediate(5 + body_size)
        + body
        + immediate(0x312B)  # SEMI
    )
    if len(program) % 2:
        program += "0"
    return b"HPHP49-C" + bytes(
        int(program[i], 16) | (int(program[i + 1], 16) << 4) for i in range(0, len(program), 2)
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, choices=["hp50g"])
    parser.add_argument("--runtime-size", required=True, type=lambda value: int(value, 0))
    parser.add_argument("input", type=Path, help="trusted PIC ARM-entry binary, not HPC1")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = package(args.input.read_bytes(), args.runtime_size, model=args.model)
    args.output.write_bytes(result)


if __name__ == "__main__":
    main()
