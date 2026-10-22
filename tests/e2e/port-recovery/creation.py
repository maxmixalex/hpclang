"""Creation-claim native fixtures; production restart ownership remains separate."""

import shutil
import struct
import zlib

from prepare import free_tail
from publication import reservation_value
from reservation import reader
from verify import home_string, require, require_stock_pc, symbol_offsets


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


def run_creation_publication(args, before, root, slot, *, transfer, verify_busy_reads):
    from installation import verify_probe

    claim_slot = 0x1E0105
    expected = bytearray((before / "flash").read_bytes())
    require(free_tail(expected, 0x1E0000) == claim_slot, "Creation fixture tail is occupied")
    reservation = reservation_value(expected, slot)
    expected[claim_slot : claim_slot + 240] = creation_record(
        reservation[:80], b"HPCLEASE", claim_slot, 0x7E
    )
    phase = root / "creation-published"
    image = "installation-creation"
    transfer(
        args,
        phase,
        before,
        args.build,
        "CREATE",
        package_name=image + ".hp",
        remote_commands=[
            "-95 CF HOME CLEAR \"unrelated data\" 'HPCKEEP' STO 'CREATE' RCL 'CREATE' PURGE"
        ],
    )
    after = phase / "state"
    base = verify_probe(
        before, after, args.build, args.nm, 0, slot, image=image, expected_flash=expected
    )
    symbols = symbol_offsets(args.build / (image + ".elf"), args.nm)
    ram = (after / "sram").read_bytes()
    result = struct.unpack_from("<5I", ram, base + symbols["creation_result"])
    require(result[:4] == (0xFFFFFFFC, 0, 0, 0), "Creation intent/receipt commit failed")
    require(
        result[4] > 0 and verify_busy_reads(args, phase) == result[4],
        "Creation Flash operations or busy-read budget differ",
    )
    require(
        home_string(reader(after), b"HPCLEASE") == reservation,
        "Created reservation differs from its bound snapshot",
    )
    require(
        home_string(reader(after), b"HPCKEEP") == b"unrelated data",
        "Creation publication changed unrelated HOME data",
    )
    print("Native intent retry, fresh RAM creation and receipt-gated CREATED verified", flush=True)


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
