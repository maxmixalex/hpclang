"""Offline creation-claim allocation fixtures; not publication or ownership proof."""

import shutil
import struct
import zlib

from prepare import free_tail
from publication import reservation_value
from reservation import reader
from verify import require, require_stock_pc


def creation_record(binding, name, slot, phase):
    """Independent fixed-format oracle; phase is excluded from the immutable CRC."""
    if len(binding) != 80 or not 1 <= len(name) <= 127 or any(not 33 <= c <= 126 for c in name):
        raise ValueError("Invalid creation-claim fixture binding or name")
    data = bytearray(240)
    data[:4] = b"\0" + (478).to_bytes(3, "little")
    data[4:12] = b"HPCOWN1\0"
    struct.pack_into("<I", data, 12, slot)
    data[16] = phase
    data[17] = len(name)
    data[20:100] = binding
    data[100 : 100 + len(name)] = name
    struct.pack_into("<I", data, 236, zlib.crc32(data[:16] + data[17:236]))
    return data


def run_creation_visibility(args, before, root, slot, *, emulator, completed, verify_busy_reads):
    flash = bytearray((before / "flash").read_bytes())
    binding = reservation_value(flash, slot)[:80]
    first = free_tail(flash, 0x1E0000)
    phases = (0xFF, 0x7F, 0x7E, 0x7C, 0)
    require(first + 240 * len(phases) < 0x200000, "No tail capacity for creation-claim fixtures")
    for index, status in enumerate(phases):
        address = first + index * 240
        flash[address : address + 240] = creation_record(binding, b"HPCLEASE", address, status)
    phase = root / "creation-claim-visibility"
    phase.mkdir()
    shutil.copytree(before, phase / "state")
    (phase / "state/flash").write_bytes(flash)
    with emulator(
        args, phase, "10000 capture home\n12000 finish-install\n", ["--reset"], flash_busy=True
    ) as child:
        completed(child, 90)
    after = phase / "state"
    require_stock_pc(after)
    require((after / "flash").read_bytes() == flash, "Stock changed a creation-claim allocation")
    original, actual = reader(before), reader(after)
    count = original(0x8611D, 3)
    require(
        actual(0x8611D, 3) == count
        and all(
            actual(0x86120 + 16 * i, 16) == original(0x86120 + 16 * i, 16) for i in range(count)
        ),
        "Creation-claim allocations changed the library catalog",
    )
    require(verify_busy_reads(args, phase) == 0, "Creation-claim visibility probe wrote Flash")
    print("Five creation-claim phases remain invisible to Stock after restart", flush=True)
