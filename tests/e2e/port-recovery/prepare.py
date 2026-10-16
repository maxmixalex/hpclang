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


def prepare(source: Path, destination: Path, slot: int, target: int, *, bound=False) -> None:
    flash = bytearray((source / "flash").read_bytes())
    if len(flash) != 0x200000:
        raise ValueError("Expected a complete hp4950 Flash image")
    if target != 0x140000:
        raise ValueError("This scenario erases the first user bank's header sector")
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
