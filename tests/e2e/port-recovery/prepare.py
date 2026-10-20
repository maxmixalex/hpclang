"""Create an erased-sector fault fixture from clean HP49g+/HP50g Stock state.

This is offline fault injection, not evidence of power loss while Flash is busy.
The source state is preserved. Boot the output with hardware reset before use;
its saved RAM still describes the original, intact Flash catalog.
"""

import argparse
from pathlib import Path
import shutil
import struct
import zlib


def changed_library_title(library: bytes) -> bytes:
    """Change only a generated library title and repair its native CRC."""
    if library[:8] != b"HPHP49-C":
        raise ValueError("Expected an HP49 library transfer envelope")
    data = [nibble for byte in library[8:] for nibble in (byte & 15, byte >> 4)]

    def word(at, count=5):
        return sum(data[at + i] << (4 * i) for i in range(count))

    def checksum():
        crc = 0
        for nibble in data[5 : span - 4]:
            for bit in range(4):
                carry = (crc ^ (nibble >> bit)) & 1
                crc >>= 1
                if carry:
                    crc ^= 0x8408
        return crc

    if len(data) < 14 or word(0) != 0x2B40:
        raise ValueError("Expected a complete native library")
    span = 5 + word(5)
    if span < 34 or len(data) != (span + 1) // 2 * 2 or not word(10, 2):
        raise ValueError("Invalid library length or empty title")
    if 34 + 2 * word(10, 2) > span or word(span - 4, 4) != checksum():
        raise ValueError("Invalid library title or native CRC")
    data[12] ^= 1
    crc = checksum()
    data[span - 4 : span] = [(crc >> (4 * i)) & 15 for i in range(4)]
    return b"HPHP49-C" + bytes(data[i] | data[i + 1] << 4 for i in range(0, len(data), 2))


def configuration_body_sector(flash: bytes, read) -> int:
    """Locate the Stock 2.15 library 229 callback sector in this fixture.

    Unlike the bank-header fault, this leaves the native library allocation and
    its configuration pointer intact. Validate the catalog's bank switch and
    the actual Flash header before using this ROM-specific witness.
    """
    if len(flash) != 0x200000:
        raise ValueError("Expected complete hp4950 Flash")
    count = read(0x8611D, 3)
    if not 0 < count < 128:
        raise ValueError("Invalid fixture library catalog")
    matches = [0x86120 + 16 * i for i in range(count) if read(0x86120 + 16 * i, 3) == 229]
    if len(matches) != 1:
        raise ValueError("Expected one Stock library 229")
    entry = matches[0]
    if read(entry + 8) != read(0x26BBB + 8 * 7):
        raise ValueError("Library 229 is not mapped to the first user Flash bank")

    def word(address, size=5):
        if not 0x40000 <= address <= 0x80000 - size:
            raise ValueError("Library field leaves its mapped bank")
        return sum(
            ((flash[0x140000 + (address + i - 0x40000) // 2] >> (4 * ((address + i) & 1))) & 15)
            << (4 * i)
            for i in range(size)
        )

    header = read(entry + 3)
    if word(header, 3) != 229:
        raise ValueError("Catalog does not identify the expected Flash library")
    offset = word(header + 18)
    callback = (header + 18 + offset) & 0xFFFFF
    if not offset or word(callback) != 0x2D9D:
        raise ValueError("Expected a native configuration program")
    sector = (0x140000 + (callback - 0x40000) // 2) & ~4095
    header_sector = (0x140000 + (header - 0x40000) // 2) & ~4095
    if sector == 0x140000 or sector == header_sector:
        raise ValueError("Callback fault would also erase the bank or library header")
    return sector


def reserve_bank_tail(source: Path, destination: Path, bank: int) -> None:
    """Occupy a fixture bank's erased tail so Stock installs recovery elsewhere.

    A native tombstone consumes capacity without introducing a library or a
    callback. Existing allocations and the final erased marker are preserved.
    This constructs test state; it is not an installation or reclamation API.
    """
    flash = bytearray((source / "flash").read_bytes())
    if len(flash) != 0x200000 or not 0x140000 <= bank < 0x200000 or bank % 0x20000:
        raise ValueError("Invalid fixture bank")
    identity = int.from_bytes(flash[bank + 0x100 : bank + 0x105], "little")
    if identity != 0xF0 + ((bank // 0x20000 - 2) << 8):
        raise ValueError("Invalid fixture bank identity")
    cursor, end = bank + 0x105, bank + 0x20000
    while cursor < end and flash[cursor] != 0xFF:
        if flash[cursor] not in (0, 0x53) or cursor + 4 > end:
            raise ValueError("Unresolved allocation in fixture bank")
        extent = (int.from_bytes(flash[cursor + 1 : cursor + 4], "little") & 0xFFFFF) + 2
        if extent < 8 or extent % 8 or extent // 2 >= end - cursor:
            raise ValueError("Invalid fixture allocation extent")
        cursor += extent // 2
    if cursor >= end or flash[cursor:end] != b"\xff" * (end - cursor):
        raise ValueError("Fixture bank has no intact erased tail")
    capacity = (end - cursor - 1) & ~3
    if capacity >= 4:
        flash[cursor : cursor + 4] = b"\0" + (capacity * 2 - 2).to_bytes(3, "little")
    shutil.copytree(source, destination)
    (destination / "flash").write_bytes(flash)


def prepare(
    source: Path, destination: Path, slot: int, target: int, *, bound=False, bank_status=None
) -> None:
    flash = bytearray((source / "flash").read_bytes())
    if len(flash) != 0x200000:
        raise ValueError("Expected a complete hp4950 Flash image")
    if not 0x140000 <= target < 0x160000 or target % 4096:
        raise ValueError("Target must be an aligned sector within the first user bank")
    if bank_status is not None and (
        type(bank_status) is not int or not 0xF0 <= bank_status <= 0xFF or target % 0x20000
    ):
        raise ValueError("Partial bank status requires a bank-header sector and an F0–FF byte")
    bank = slot & ~0x1FFFF
    if not 0x160000 <= bank < 0x200000 or slot != bank + 0x105:
        raise ValueError(
            "Journal must start at an intact, separate empty bank's allocation boundary"
        )
    for base in range(0x140000, 0x200000, 0x20000):
        identity = int.from_bytes(flash[base + 0x100 : base + 0x105], "little")
        if identity != 0xF0 + ((base // 0x20000 - 2) << 8):
            raise ValueError(f"Invalid native bank identity at {base:#x}")
    if flash[slot : bank + 0x20000] != b"\xff" * (bank + 0x20000 - slot):
        raise ValueError("Journal bank is not empty")
    record = bytearray(4124)
    record[:4] = bytes.fromhex("7e362030")
    record[4:12] = b"HPCSECT1"
    struct.pack_into("<3I", record, 12, slot, target, 4096)
    record[24:4120] = flash[target : target + 4096]
    struct.pack_into("<I", record, 4120, zlib.crc32(record[1:4120]))
    flash[slot : slot + len(record)] = record
    if bound:
        authority_slot = slot + len(record)
        authority = bytearray.fromhex("7e3e0040") + b"HPCAUTH1"
        authority += struct.pack(
            "<4I", authority_slot, slot, target, int.from_bytes(record[-4:], "little")
        )
        authority += struct.pack("<I", zlib.crc32(authority[1:]))
        flash[authority_slot : authority_slot + len(authority)] = authority
    flash[target : target + 4096] = b"\xff" * 4096
    if bank_status is not None:
        # NOR programming from FF to F0 can leave any of these sixteen values.
        # Keep the remaining identity bytes erased; the journal retains the
        # original complete sector, including its native bank identity.
        flash[target + 0x100] = bank_status
    # Refuse an existing output so stale RAM/Flash cannot enter the scenario.
    shutil.copytree(source, destination)
    (destination / "flash").write_bytes(flash)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--slot", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--target", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--bound", action="store_true", help="append an armed transaction binding")
    args = parser.parse_args()
    prepare(args.source, args.destination, args.slot, args.target, bound=args.bound)


if __name__ == "__main__":
    main()
