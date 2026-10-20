#!/usr/bin/env python3
"""Internal HP49g+/HP50g recovery CODE envelope; not an HPC1 application launcher.

The payload must be a trusted position-independent ARM entry at offset zero.
It owns BSS initialization, its stack, hardware state and return through LR.
This envelope supplies aligned object-owned storage, not MMU/page allocation.
Only the HP49g+/HP50g Stock main-RAM mapping has been validated for this entry.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


MAX_INITIALIZED = 32768
MAX_NIBBLES = 0xFFFFF
# This entry converts Saturn addresses through the Stock main-RAM window
# [0x80000, 0x100000). Count the entire enclosing program, not just its ARM copy.
# This is a physical ceiling, not the amount of free RAM at installation time.
MAIN_RAM_NIBBLES = 0x100000 - 0x80000
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


def entry(initialized_size: int, source_offset: int, *, fresh_copy: bool = False) -> str:
    """Encode the independently implemented Saturn SAVE/copy/ARMSYS/GETPTR path.

    Offsets and lengths are in nibbles except initialized_size (bytes).
    The counted copy transfers complete 16-nibble blocks. Clearing the low
    four destination bits aligns the ARM image to eight bytes at every CODE
    nibble alignment. Fifteen extra reserved nibbles cover the rounding.
    With fresh_copy the destination rounds the source downward instead; the
    caller supplies fifteen nibbles between instructions and initialized bytes.
    """
    if not 8 <= initialized_size <= MAX_INITIALIZED or initialized_size % 8:
        raise ValueError("initialized image must contain 1..4096 eight-byte blocks")
    code = "8f" + immediate(0x679B) + "20" + "81b5"  # SAVE, P=0, C=PC
    code += "da" + "34" + immediate(source_offset) + "c2" + "134"
    code += "da" + "34" + immediate(0 if fresh_copy else initialized_size * 2 + 15) + "c2"
    code += "80880" + "80881" + "80882" + "80883" + "135" + "d5"
    code += "34" + immediate(initialized_size // 8 - 1)
    code += "1527" + "1517" + "16f" + "17f" + "ce" + "5fe"
    # Convert the object-owned Saturn destination to the hp4950 ARM SRAM address.
    code += "af2" + "d9" + "81e" + "25" + "32" + "080" + "20"
    code += "80bfe" + "8f" + immediate(0x67D2) + "142164808c"
    return code


def package(
    initialized: bytes, runtime_size: int, *, model: str, fresh_copy: bool = False
) -> bytes:
    """Return an HP binary RPL program with one CODE object and no result marker.

    runtime_size includes initialized bytes, BSS, workspace and owned stack.
    The caller must check ELF relocation safety before providing raw bytes.
    Bounds here prevent truncated Saturn lengths, copy/reservation overlap and
    programs larger than the supported main-RAM window. They do not establish
    free calculator RAM or a sufficient transfer budget.

    fresh_copy is only for a configuration entry copied from immutable Flash
    on EVERY invocation. Forward copying to an aligned destination below the
    source consumes that source; the resulting RAM object cannot be reused.
    The default retains an immutable source for ordinary reusable RAM programs.
    """
    if model not in MODELS:
        raise ValueError("independent recovery entry is validated only for hp49gplus/hp50g")
    if not initialized or len(initialized) > MAX_INITIALIZED:
        raise ValueError("initialized image size must be 1..32768 bytes")
    initialized += bytes((-len(initialized)) % 8)
    if runtime_size < len(initialized) or runtime_size % 8:
        raise ValueError("runtime size must be eight-byte aligned and contain initialized image")
    code = entry(len(initialized), 0, fresh_copy=fresh_copy)
    # C=PC captures the address 13 nibbles after the start of this entry.
    leading = 15 if fresh_copy else 0
    code = entry(len(initialized), len(code) + leading - 13, fresh_copy=fresh_copy)
    reserve = (runtime_size - len(initialized)) * 2 if fresh_copy else runtime_size * 2 + 15
    body_size = len(code) + leading + len(initialized) * 2 + reserve
    if body_size + 5 > MAX_NIBBLES:
        raise ValueError("recovery CODE object exceeds Saturn length capacity")
    # DOCOL, CODE prologue, CODE length and SEMI each occupy five nibbles.
    if body_size + 20 > MAIN_RAM_NIBBLES:
        raise ValueError("recovery program exceeds the Stock main-RAM window")
    payload = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in initialized)
    body = code + "0" * leading + payload + "0" * reserve
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


def _recovery_program(program: bytes) -> str:
    if program[:8] != b"HPHP49-C":
        raise ValueError("expected an HP49 binary recovery program")
    code = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in program[8:])
    if len(code) < 20 or code[:10] != immediate(0x2D9D) + immediate(0x2DCC):
        raise ValueError("recovery configuration must contain one CODE object")
    extent = int(code[10:15][::-1], 16)
    end = 10 + extent
    if extent < 5 or code[end : end + 5] != immediate(0x312B) or code[end + 5 :] not in ("", "0"):
        raise ValueError("invalid recovery CODE extent or program tail")
    return code[: end + 5]


def _configuration_library(configuration: str, library_id: int, commands=()) -> bytes:
    if type(library_id) is not int or not 769 <= library_id <= 1791:
        raise ValueError("recovery library ID must be in the user-library range 769..1791")
    title = b"HPCRecovery"
    header = (
        immediate(len(title), 2)
        + "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in title)
        + immediate(len(title), 2)
        + immediate(library_id, 3)
    )
    # All offsets are relative to their own fields. No command names or message
    # table are exported; internal ROMPTR calls use only the command-link table.
    fields = 5 + len(header)
    links_at = fields + 20
    links = ""
    command_data = "".join(commands)
    if commands:
        cursor = links_at + 10 + 5 * len(commands)
        offsets = []
        for index, command in enumerate(commands):
            offsets.append(immediate(cursor - (links_at + 10 + 5 * index)))
            cursor += len(command)
        links = immediate(0x2A4E) + immediate(5 + 5 * len(commands)) + "".join(offsets)
    configuration_at = links_at + len(links) + len(command_data)
    header += immediate(0) * 2 + immediate(10 if commands else 0)
    header += immediate(configuration_at - (fields + 15))
    payload = header + links + command_data + configuration
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


def configuration_library(program: bytes, library_id: int) -> bytes:
    """Wrap recovery without exported commands or a calculator-side compiler.

    The caller establishes ID availability and safe startup ordering. Encoding
    a library does not reserve an ID or install recovery.
    """
    return _configuration_library(_recovery_program(program), library_id)


def guarded_configuration_library(
    program: bytes,
    library_id: int,
    *,
    name: str,
    capacity: int,
    prefix: bytes,
    admission: bytes | None = None,
) -> bytes:
    """Release a matching HOME string before loading the hidden recovery command.

    Internal HP49g+/HP50g Stock guard, not a trusted installer. The caller must
    bind the expected name/capacity/prefix to its installation and transaction,
    preserve recovery-time capacity, and establish global startup ordering.
    A matching header alone does not prove ownership. An absent object permits
    ordinary recovery; a wrong type, size or prefix preserves it and skips replay.
    Optional admission is a small program returning real 0/1 before the HOME
    object is inspected or released. Its own RAM requirement must be admitted
    separately; a false result skips both release and recovery.
    The configuration remains small: the ARM program is a separate hidden command
    so its RAM copy is not needed before releasing the reservation.
    """
    code = _recovery_program(program)
    encoded_name = name.encode("ascii")
    if not encoded_name or len(encoded_name) > 127 or any(b < 33 or b > 126 for b in encoded_name):
        raise ValueError("reservation name must contain 1..127 printable non-space ASCII bytes")
    if type(capacity) is not int or not (len(code) + 1) // 2 <= capacity < MAIN_RAM_NIBBLES // 2:
        raise ValueError("reservation capacity must fit the recovery program and Stock RAM")
    if not isinstance(prefix, bytes) or not 1 <= len(prefix) <= capacity:
        raise ValueError("reservation prefix must be nonempty bytes within its capacity")

    # Stock UserRPL entries shared by the validated HP49g+/HP50g fixtures.
    # Real constants and SAME deliberately avoid mixed exact-integer comparison
    # during startup. These programs are independently encoded from their RPL
    # structure, not copied from a historical library or calculator-state blob.
    entries = {
        "begin": 0x389B9,
        "end": 0x389D4,
        "if": 0x37F48,
        "then": 0x37F7F,
        "else": 0x3805D,
        "endif": 0x3807D,
        "dup": 0x3DBEA,
        "drop": 0x3DC3B,
        "type": 0x3BC39,
        "size": 0x3BB1F,
        "same": 0x3C9E5,
        "sub": 0x3B8D7,
        "quote": 0x389EF,
        "endquote": 0x38A14,
        "purge": 0x3E87C,
        "vtype": 0x3BDB2,
        "rcl": 0x3E6F1,
        "home": 0x39405,
    }

    def op(name):
        return immediate(entries[name])

    def sequence(*parts):
        return immediate(0x2D9D) + "".join(parts) + immediate(0x312B)

    def user(*parts):
        return sequence(op("begin"), *parts, op("end"))

    def real(value):
        constants = {-1: 0x2FA09, 0: 0x2F937, 1: 0x2F94C, 2: 0x2F961}
        if value in constants:
            return immediate(constants[value])
        digits = str(value)
        # All callers supply positive integral metadata below the RAM ceiling.
        return (
            immediate(0x2933)
            + str(len(digits) - 1).zfill(3)[::-1]
            + digits.ljust(12, "0")[::-1]
            + "0"
        )

    def text(data):
        return "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in data)

    def call(index):
        return immediate(0x2E92) + immediate(library_id, 3) + immediate(index, 3)

    recover, check_type, check_size, check_header, release, present = range(6)
    variable = op("quote") + immediate(0x2E48) + immediate(len(encoded_name), 2)
    variable += text(encoded_name) + op("endquote")
    header = immediate(0x2A2C) + immediate(5 + len(prefix) * 2) + text(prefix)
    commands = (
        code,
        user(
            op("if"),
            op("dup"),
            op("type"),
            real(2),
            op("same"),
            op("then"),
            call(check_size),
            op("else"),
            sequence(op("drop"), real(0)),
            op("endif"),
        ),
        user(
            op("if"),
            op("dup"),
            op("size"),
            real(capacity),
            op("same"),
            op("then"),
            call(check_header),
            op("else"),
            sequence(op("drop"), real(0)),
            op("endif"),
        ),
        user(real(1), real(len(prefix)), op("sub"), header, op("same")),
        user(
            op("if"),
            call(check_type),
            op("then"),
            sequence(variable, op("purge"), real(1)),
            op("else"),
            real(0),
            op("endif"),
        ),
        user(
            op("if"),
            variable,
            op("vtype"),
            real(-1),
            op("same"),
            op("then"),
            real(1),
            op("else"),
            sequence(variable, op("rcl"), call(release)),
            op("endif"),
        ),
    )
    configuration = user(
        op("home"), op("if"), call(present), op("then"), call(recover), op("endif")
    )
    if admission is not None:
        admission_code = _recovery_program(admission)
        # The full recovery command stays at index zero; all existing guard
        # commands retain their identities. Admission executes before present.
        configuration = user(op("if"), call(len(commands)), op("then"), configuration, op("endif"))
        commands += (admission_code,)
    return _configuration_library(configuration, library_id, commands)


def bind_image(initialized: bytes, offset: int, journal: int, authority: int) -> bytes:
    """Bind an owned ELF descriptor, not a trusted installation or reservation.

    The installer must own both allocations and their durable lifetime. Geometry
    checks here cannot prove allocation boundaries, freshness or startup safety.
    """
    if type(offset) is not int or offset < 0 or offset % 4 or offset + 16 > len(initialized):
        raise ValueError("Recovery binding is outside initialized aligned storage")
    if initialized[offset : offset + 8] != b"HPCBIND1":
        raise ValueError("Recovery binding has an unsupported format")
    for address, size in ((journal, 4124), (authority, 32)):
        if type(address) is not int or not 0x140000 <= address < 0x200000:
            raise ValueError("Recovery binding leaves user Flash")
        within = address & 0x1FFFF
        if within < 0x105 or within % 4 != 1 or within + size >= 0x20000:
            raise ValueError("Recovery binding overlaps a bank header or end marker")
    if journal < authority + 32 and authority < journal + 4124:
        raise ValueError("Recovery binding allocations overlap")
    return (
        initialized[: offset + 8]
        + journal.to_bytes(4, "little")
        + authority.to_bytes(4, "little")
        + initialized[offset + 16 :]
    )


def elf_symbols(elf: Path, nm: str) -> dict[str, int]:
    symbols = {}
    for line in subprocess.check_output([nm, elf], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    return symbols


def recovery_layout(library, images, *, identity_offset=None):
    """Locate owned ELF descriptors through generated command links, never signatures.

    Nibble offsets include the native allocation's four-byte envelope. Consumers
    must validate this exact expected library; metadata is not root publication.
    Optional identity_object_nibble instead excludes the allocation envelope,
    matching the coordinate carried inside the HPCROOT1 descriptor.
    """
    data = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in library[8:])

    def word(at, width=5):
        return int(data[at : at + width][::-1], 16)

    fields = 17 + 2 * word(10, 2)
    links = fields + 10 + word(fields + 10)
    if word(links) != 0x2A4E or word(links + 5) != 40:
        raise ValueError("Recovery layout requires the seven-command guarded library")
    layout = dict(
        version=1, library_id=word(fields - 3, 3), record_bytes=((5 + word(5) + 15) & ~7) // 2
    )
    for name, index, initialized, offset, fresh_copy in images:
        link = links + 10 + 5 * index
        start = link + word(link)
        size = (len(initialized) + 7) & ~7
        source = 15 + len(entry(size, 0, fresh_copy=fresh_copy)) + (15 if fresh_copy else 0)
        descriptor = start + source + 2 * offset
        expected = "".join(
            f"{byte & 15:x}{byte >> 4:x}" for byte in initialized[offset : offset + 16]
        )
        if len(expected) != 32 or data[descriptor : descriptor + 32] != expected:
            raise ValueError("Recovery descriptor does not match its generated command")
        layout[name + "_binding_nibble"] = 8 + descriptor
        if name == "admission" and identity_offset is not None:
            if (
                type(identity_offset) is not int
                or identity_offset < 0
                or identity_offset % 4
                or identity_offset + 48 > len(initialized)
                or initialized[identity_offset : identity_offset + 48] != b"HPCROOT1" + bytes(40)
            ):
                raise ValueError("Recovery identity symbol is not an owned initialized placeholder")
            identity = start + source + 2 * identity_offset
            expected_identity = "".join(
                f"{byte & 15:x}{byte >> 4:x}"
                for byte in initialized[identity_offset : identity_offset + 48]
            )
            if data[identity : identity + 96] != expected_identity:
                raise ValueError("Recovery identity does not match its generated admission command")
            layout["identity_object_nibble"] = identity
    if identity_offset is not None and "identity_object_nibble" not in layout:
        raise ValueError("Recovery identity requires the owned admission image")
    return layout


def seal_recovery_identity(library: bytes, identity_nibble: int) -> bytes:
    """Seal an owned HPCROOT1 placeholder inside a generated library.

    Coordinates are relative to the library object, excluding the transfer
    header and native allocation envelope. The caller derives this location
    from the owned ELF symbol/command layout, never a signature search.
    The 48-byte descriptor contains magic, object span, descriptor coordinate
    and SHA-256. Hash canonical packed object nibbles with only the digest and
    native CRC fields zeroed; an odd final high nibble is zero. This avoids
    embedding a library inside its own admission command.

    This is content identity, not installation provenance, transaction ownership
    or authentication against someone able to replace the executing program.
    No runtime consumer or public installation permission is implied.
    """
    if not isinstance(library, bytes) or library[:8] != b"HPHP49-C":
        raise ValueError("Recovery identity requires a generated HP49 library")
    data = [nibble for byte in library[8:] for nibble in (byte & 15, byte >> 4)]

    def word(at, count=5):
        return sum(data[at + i] << (4 * i) for i in range(count))

    def packed(values):
        values = list(values)
        if len(values) % 2:
            values.append(0)
        return bytes(values[i] | values[i + 1] << 4 for i in range(0, len(values), 2))

    if len(data) < 14 or word(0) != 0x2B40:
        raise ValueError("Recovery identity requires a complete library object")
    span = 5 + word(5)
    if span < 14 or len(data) != (span + 1) // 2 * 2:
        raise ValueError("Recovery identity library length does not match its object")
    crc = 0
    for nibble in data[5 : span - 4]:
        crc = (crc >> 4) ^ (((crc ^ nibble) & 15) * 0x1081)
    if word(span - 4, 4) != crc:
        raise ValueError("Recovery identity library CRC is invalid")
    if type(identity_nibble) is not int or identity_nibble < 10 or identity_nibble + 96 > span - 4:
        raise ValueError("Recovery identity descriptor leaves the library body")
    descriptor = packed(data[identity_nibble : identity_nibble + 96])
    if descriptor != b"HPCROOT1" + bytes(40):
        raise ValueError("Recovery identity requires an untouched owned placeholder")
    descriptor = b"HPCROOT1" + span.to_bytes(4, "little") + identity_nibble.to_bytes(4, "little")
    for index, byte in enumerate(descriptor):
        data[identity_nibble + 2 * index : identity_nibble + 2 * index + 2] = [byte & 15, byte >> 4]
    data[span - 4 : span] = [0] * 4
    digest = hashlib.sha256(packed(data[:span])).digest()
    digest_at = identity_nibble + 32
    data[digest_at : digest_at + 64] = [
        nibble for byte in digest for nibble in (byte & 15, byte >> 4)
    ]
    crc = 0
    for nibble in data[5 : span - 4]:
        crc = (crc >> 4) ^ (((crc ^ nibble) & 15) * 0x1081)
    data[span - 4 : span] = [(crc >> (4 * i)) & 15 for i in range(4)]
    return b"HPHP49-C" + packed(data[:span])


def package_elf(
    elf: Path,
    *,
    model: str,
    nm: str,
    readelf: str,
    objcopy: str,
    fresh_copy: bool = False,
    binding: tuple[int, int] | None = None,
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
    symbols = elf_symbols(elf, nm)
    if not {"entry", "__zero_start", "__image_end"} <= symbols.keys():
        raise ValueError("Recovery ELF is missing required entry/layout symbols")
    if symbols["entry"] != 0:
        raise ValueError("Recovery ARM entry must be at image offset zero")
    with tempfile.TemporaryDirectory(prefix="hpclang-recovery-") as temporary:
        binary = Path(temporary) / "recovery.bin"
        subprocess.run(
            [objcopy, "-O", "binary", "-j", ".text", "-j", ".tdata", elf, binary],
            check=True,
        )
        initialized = binary.read_bytes()
    zero, end = symbols["__zero_start"], symbols["__image_end"]
    if not len(initialized) <= zero <= end or zero % 4 or end % 8:
        raise ValueError(
            "Recovery BSS clearing must preserve initialized data and stay in the image"
        )
    offset = symbols.get("hpclang_recovery_binding")
    if binding is not None and offset is None:
        raise ValueError("Recovery ELF has no installation binding descriptor")
    if offset is not None:
        if binding is None:
            binding = tuple(
                int.from_bytes(initialized[offset + i : offset + i + 4], "little") for i in (8, 12)
            )
        initialized = bind_image(initialized, offset, *binding)
    return package(initialized, end, model=model, fresh_copy=fresh_copy), initialized


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, choices=MODELS)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--runtime-size",
        type=lambda value: int(value, 0),
        help="trusted raw binary mode",
    )
    mode.add_argument(
        "--elf", action="store_true", help="validate and package an owned recovery ELF"
    )
    parser.add_argument("--binary-output", type=Path, help="optional extracted image in ELF mode")
    parser.add_argument(
        "--journal-slot",
        type=lambda value: int(value, 0),
        help="caller-reserved physical journal address (ELF library only)",
    )
    parser.add_argument(
        "--authority-slot",
        type=lambda value: int(value, 0),
        help="caller-reserved physical authority address (ELF library only)",
    )
    parser.add_argument(
        "--flash-copy",
        action="store_true",
        help="compact configuration requiring a fresh copy from immutable Flash on every call",
    )
    parser.add_argument(
        "--program-output",
        type=Path,
        help="optional configuration program before library wrapping",
    )
    parser.add_argument(
        "--library-id",
        type=int,
        help="wrap the recovery program in a native configuration library (does not install it)",
    )
    parser.add_argument(
        "--reservation-name", help="HOME string expected by the internal recovery guard"
    )
    parser.add_argument(
        "--reservation-bytes", type=int, help="exact reserved string capacity in bytes"
    )
    parser.add_argument(
        "--reservation-prefix", help="expected opaque header, encoded as UTF-8 bytes"
    )
    admission_source = parser.add_mutually_exclusive_group()
    admission_source.add_argument(
        "--admission-program",
        type=Path,
        help="small real-valued predicate to run before guarded RAM release",
    )
    admission_source.add_argument(
        "--admission-elf",
        type=Path,
        help="predicate ELF bound to the same explicit journal/authority pair as recovery",
    )
    parser.add_argument(
        "--layout-output",
        type=Path,
        help="write descriptor coordinates for the exact generated guarded library",
    )
    for tool in ("nm", "readelf", "objcopy"):
        parser.add_argument("--" + tool, default="llvm-" + tool)
    parser.add_argument("input", type=Path, help="recovery ELF or trusted PIC binary, not HPC1")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    binding = None
    if args.journal_slot is not None or args.authority_slot is not None:
        if (
            args.journal_slot is None
            or args.authority_slot is None
            or not args.elf
            or args.library_id is None
        ):
            parser.error("both binding slots require --elf and --library-id")
        binding = (args.journal_slot, args.authority_slot)
    if (args.flash_copy or args.program_output is not None) and args.library_id is None:
        parser.error("--flash-copy and --program-output require --library-id")
    reservation = (args.reservation_name, args.reservation_bytes, args.reservation_prefix)
    guarded = any(value is not None for value in reservation)
    if guarded and (any(value is None for value in reservation) or args.library_id is None):
        parser.error("all three --reservation-* options and --library-id are required together")
    if args.admission_program is not None and not guarded:
        parser.error("--admission-program requires a guarded reservation")
    if args.admission_elf is not None and (not guarded or binding is None):
        parser.error("--admission-elf requires a guarded reservation and both binding slots")
    if args.admission_program is not None and binding is not None:
        parser.error(
            "explicit recovery binding requires --admission-elf, not a prepacked predicate"
        )
    if args.layout_output is not None and args.admission_elf is None:
        parser.error("--layout-output requires --admission-elf")
    paths = [
        args.input,
        args.output,
        args.binary_output,
        args.program_output,
        args.admission_program,
        args.admission_elf,
        args.layout_output,
    ]
    resolved = [path.resolve() for path in paths if path is not None]
    if len(set(resolved)) != len(resolved):
        parser.error("input and all output paths must be different files")
    if args.binary_output is not None:
        if not args.elf:
            parser.error("--binary-output requires --elf")
    if args.elf:
        result, initialized = package_elf(
            args.input,
            model=args.model,
            nm=args.nm,
            readelf=args.readelf,
            objcopy=args.objcopy,
            fresh_copy=args.flash_copy,
            binding=binding,
        )
    else:
        result = package(
            args.input.read_bytes(),
            args.runtime_size,
            model=args.model,
            fresh_copy=args.flash_copy,
        )
    program = result
    admission = args.admission_program.read_bytes() if args.admission_program else None
    identity_offset = None
    if args.admission_elf is not None:
        # A hidden command always starts with a fresh immutable Flash copy.
        # Validate both ELFs and bind both before writing any output file.
        admission, admission_initialized = package_elf(
            args.admission_elf,
            model=args.model,
            nm=args.nm,
            readelf=args.readelf,
            objcopy=args.objcopy,
            fresh_copy=True,
            binding=binding,
        )
        identity_offset = elf_symbols(args.admission_elf, args.nm).get("hpclang_recovery_identity")
    if guarded:
        result = guarded_configuration_library(
            result,
            args.library_id,
            name=args.reservation_name,
            capacity=args.reservation_bytes,
            prefix=args.reservation_prefix.encode("utf-8"),
            admission=admission,
        )
    elif args.library_id is not None:
        result = configuration_library(result, args.library_id)
    if args.layout_output is not None or identity_offset is not None:
        layout = recovery_layout(
            result,
            (
                (
                    "recovery",
                    0,
                    initialized,
                    elf_symbols(args.input, args.nm)["hpclang_recovery_binding"],
                    args.flash_copy,
                ),
                (
                    "admission",
                    6,
                    admission_initialized,
                    elf_symbols(args.admission_elf, args.nm)["hpclang_recovery_binding"],
                    True,
                ),
            ),
            identity_offset=identity_offset,
        )
        if identity_offset is not None:
            result = seal_recovery_identity(result, layout["identity_object_nibble"])
    if args.binary_output is not None:
        args.binary_output.write_bytes(initialized)
    if args.program_output is not None:
        args.program_output.write_bytes(program)
    if args.layout_output is not None:
        args.layout_output.write_text(json.dumps(layout, indent=2) + "\n")
    args.output.write_bytes(result)


if __name__ == "__main__":
    main()
