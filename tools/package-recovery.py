#!/usr/bin/env python3
"""Internal HP49g+/HP50g recovery CODE envelope; not an HPC1 application launcher.

The payload must be a trusted position-independent ARM entry at offset zero.
It owns BSS initialization, its stack, hardware state and return through LR.
This envelope supplies aligned object-owned storage, not MMU/page allocation.
Only the HP49g+/HP50g Stock main-RAM mapping has been validated for this entry.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


MAX_INITIALIZED = 32768
MAX_NIBBLES = 0xFFFFF
MODELS = ("hp49gplus", "hp50g")

# The owned recovery image uses PC-relative calls/data and local-exec TLS.
# LLD can retain relaxed TLS_IE32 entries in --emit-relocs output.
PIC_RELOCATIONS = {
    "R_ARM_ALU_PC_G0",
    "R_ARM_CALL",
    "R_ARM_JUMP24",
    "R_ARM_REL32",
    "R_ARM_THM_CALL",
    "R_ARM_TLS_IE32",
    "R_ARM_TLS_LE32",
}


def immediate(value: int, width: int = 5) -> str:
    if not 0 <= value < 1 << (4 * width):
        raise ValueError("Saturn immediate overflow")
    return "".join(f"{(value >> (4 * i)) & 15:x}" for i in range(width))


def entry(initialized_size: int, source_offset: int) -> str:
    """Encode the independently implemented Saturn SAVE/copy/ARMSYS/GETPTR path.

    Offsets and lengths are in nibbles except initialized_size (bytes).
    The counted copy transfers complete 16-nibble blocks. Clearing the low
    four destination bits aligns the ARM image to eight bytes at every CODE
    nibble alignment. Copy toward lower addresses, allowing the initialized
    payload and destination to overlap without a second image reservation.
    """
    if not 8 <= initialized_size <= MAX_INITIALIZED or initialized_size % 8:
        raise ValueError("initialized image must contain 1..4096 eight-byte blocks")
    code = "8f" + immediate(0x679B) + "20" + "81b5"  # SAVE, P=0, C=PC
    code += "da" + "34" + immediate(source_offset) + "c2" + "134"
    code += "80880" + "80881" + "80882" + "80883" + "135" + "d5"
    code += "34" + immediate(initialized_size // 8 - 1)
    code += "1527" + "1517" + "16f" + "17f" + "ce" + "5fe"
    # Convert the object-owned Saturn destination to the hp4950 ARM SRAM address.
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
    if model not in MODELS:
        raise ValueError("independent recovery entry is validated only for hp49gplus/hp50g")
    if not initialized or len(initialized) > MAX_INITIALIZED:
        raise ValueError("initialized image size must be 1..32768 bytes")
    initialized += bytes((-len(initialized)) % 8)
    if runtime_size < len(initialized) or runtime_size % 8:
        raise ValueError("runtime size must be eight-byte aligned and contain initialized image")
    code = entry(len(initialized), 0)
    # C=PC captures the address 13 nibbles after the start of this entry.
    # A full alignment block separates the source from the Saturn instructions.
    # Rounding its address down cannot overwrite the still-running entry, even
    # when the containing CODE object begins on an odd nibble.
    code = entry(len(initialized), len(code) + 16 - 13)
    reserve = (runtime_size - len(initialized)) * 2
    body_size = len(code) + 16 + runtime_size * 2
    if body_size + 5 > MAX_NIBBLES:
        raise ValueError("recovery CODE object exceeds Saturn length capacity")
    payload = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in initialized)
    body = code + "0" * 16 + payload + "0" * reserve
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


def configuration_library(program: bytes, library_id: int) -> bytes:
    """Wrap an owned recovery program in a configuration-only native library.

    No compiler on the calculator or exported commands are needed. The caller
    must establish that the ID is available and the installed startup ordering
    is safe; encoding a library does not reserve an ID or install recovery.
    """
    if type(library_id) is not int or not 769 <= library_id <= 1791:
        raise ValueError("recovery library ID must be in the user-library range 769..1791")
    if program[:8] != b"HPHP49-C":
        raise ValueError("expected an HP49 binary recovery program")
    code = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in program[8:])
    if len(code) < 20 or code[:10] != immediate(0x2D9D) + immediate(0x2DCC):
        raise ValueError("recovery configuration must contain one CODE object")
    extent = int(code[10:15][::-1], 16)
    end = 10 + extent
    if extent < 5 or code[end : end + 5] != immediate(0x312B) or code[end + 5 :] not in ("", "0"):
        raise ValueError("invalid recovery CODE extent or program tail")
    title = b"HPCRecovery"
    header = (
        immediate(len(title), 2)
        + "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in title)
        + immediate(len(title), 2)
        + immediate(library_id, 3)
        + immediate(0) * 3  # No hash, message or command-link tables.
        + immediate(5)  # Configuration starts immediately after this offset.
    )
    payload = header + code[: end + 5]
    length = 5 + len(payload) + 4
    if length > MAX_NIBBLES:
        raise ValueError("recovery library exceeds Saturn length capacity")
    body = immediate(length) + payload
    crc = 0
    for nibble in body:
        crc = (crc >> 4) ^ (((crc ^ int(nibble, 16)) & 15) * 0x1081)
    encoded = immediate(0x2B40) + body + immediate(crc, 4)
    encoded += "0" * (len(encoded) % 2)
    return b"HPHP49-C" + bytes(int(encoded[i : i + 2][::-1], 16) for i in range(0, len(encoded), 2))


def package_elf(
    elf: Path, *, model: str, nm: str, readelf: str, objcopy: str
) -> tuple[bytes, bytes]:
    """Check the owned recovery ELF contract and return envelope and raw image.

    The linker must reject unhandled sections. This is the recovery entry's
    layout, not a general-purpose ARM application or HPC1 packager.
    """
    if model not in MODELS:
        raise ValueError("independent recovery entry is validated only for hp49gplus/hp50g")
    listing = subprocess.check_output([readelf, "-r", elf], text=True)
    relocations = {word for word in listing.split() if word.startswith("R_ARM_")}
    if not relocations or relocations - PIC_RELOCATIONS:
        raise ValueError(
            f"Unverified recovery relocations: {sorted(relocations - PIC_RELOCATIONS)}"
        )
    symbols = {}
    for line in subprocess.check_output([nm, elf], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    if not {"entry", "__zero_start", "__image_end"} <= symbols.keys():
        raise ValueError("Recovery ELF is missing required entry/layout symbols")
    if symbols["entry"] != 0:
        raise ValueError("Recovery ARM entry must be at image offset zero")
    with tempfile.TemporaryDirectory(prefix="hpclang-recovery-") as temporary:
        binary = Path(temporary) / "recovery.bin"
        subprocess.run(
            [objcopy, "-O", "binary", "-j", ".text", "-j", ".tdata", elf, binary], check=True
        )
        initialized = binary.read_bytes()
    zero, end = symbols["__zero_start"], symbols["__image_end"]
    if not len(initialized) <= zero <= end or zero % 4 or end % 8:
        raise ValueError(
            "Recovery BSS clearing must preserve initialized data and stay in the image"
        )
    return package(initialized, end, model=model), initialized


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, choices=MODELS)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--runtime-size", type=lambda value: int(value, 0), help="trusted raw binary mode"
    )
    mode.add_argument(
        "--elf", action="store_true", help="validate and package an owned recovery ELF"
    )
    parser.add_argument("--binary-output", type=Path, help="optional extracted image in ELF mode")
    parser.add_argument(
        "--library-id",
        type=int,
        help="wrap the recovery program in a native configuration library (does not install it)",
    )
    for tool in ("nm", "readelf", "objcopy"):
        parser.add_argument("--" + tool, default="llvm-" + tool)
    parser.add_argument("input", type=Path, help="recovery ELF or trusted PIC binary, not HPC1")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error("input and output must be different files")
    if args.binary_output is not None:
        if not args.elf:
            parser.error("--binary-output requires --elf")
        if args.binary_output.resolve() in (args.input.resolve(), args.output.resolve()):
            parser.error("binary output must differ from input and packaged output")
    if args.elf:
        result, initialized = package_elf(
            args.input, model=args.model, nm=args.nm, readelf=args.readelf, objcopy=args.objcopy
        )
        if args.binary_output is not None:
            args.binary_output.write_bytes(initialized)
    else:
        result = package(args.input.read_bytes(), args.runtime_size, model=args.model)
    if args.library_id is not None:
        result = configuration_library(result, args.library_id)
    args.output.write_bytes(result)


if __name__ == "__main__":
    main()
