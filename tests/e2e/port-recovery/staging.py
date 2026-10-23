"""Verify a durable transaction binding that still forbids replay."""

import shutil
import struct
import zlib

from installation import AUTHORITY_SLOT, JOURNAL_SLOT, TARGET, verify_probe
from reservation import reader
from verify import require, require_stock_pc, symbol_offsets


def staged_flash(before):
    flash = bytearray((before / "flash").read_bytes())
    journal = bytearray.fromhex("7e362030") + b"HPCSECT1"
    journal += struct.pack("<3I", JOURNAL_SLOT, TARGET, 4096)
    journal += flash[TARGET : TARGET + 4096]
    journal += struct.pack("<I", zlib.crc32(journal[1:]))
    authority = bytearray.fromhex("7f3e0040") + b"HPCAUTH1"
    authority += struct.pack(
        "<4I", AUTHORITY_SLOT, JOURNAL_SLOT, TARGET, int.from_bytes(journal[-4:], "little")
    )
    authority += struct.pack("<I", zlib.crc32(authority[1:]))
    require(
        flash[JOURNAL_SLOT : AUTHORITY_SLOT + len(authority) + 1]
        == b"\xff" * (len(journal) + len(authority) + 1),
        "Staging fixture does not own an empty contiguous allocation",
    )
    flash[JOURNAL_SLOT : JOURNAL_SLOT + len(journal)] = journal
    flash[AUTHORITY_SLOT : AUTHORITY_SLOT + len(authority)] = authority
    return flash


def verify_staging(before, after, build, nm, slot):
    base = verify_probe(
        before,
        after,
        build,
        nm,
        0,
        slot,
        image="installation-staging",
        expected_flash=staged_flash(before),
    )
    symbols = symbol_offsets(build / "installation-staging.elf", nm)
    result = struct.unpack_from(
        "<12I", (after / "sram").read_bytes(), base + symbols["publication_result"]
    )
    require(
        result[:4] == (0, 0, 0xFFFFFFFC, 0)
        and result[4] > 0
        and result[5:] == (0x7F, 0, slot, JOURNAL_SLOT, AUTHORITY_SLOT, TARGET, 0),
        "Staging enabled replay, rewrote a complete stage, or failed publication",
    )
    return result[4]


def run_staging(args, before, root, slot, *, transfer, emulator, completed, verify_busy_reads):
    phase = root / "unarmed-staging"
    transfer(args, phase, before, args.build, "STAGE", package_name="installation-staging.hp")
    operations = verify_staging(before, phase / "state", args.build, args.nm, slot)
    require(verify_busy_reads(args, phase) == operations, "Staging bypassed Flash busy polling")
    restart = root / "unarmed-staging-restart"
    restart.mkdir()
    shutil.copytree(phase / "state", restart / "state")
    with emulator(
        args, restart, "10000 capture home\n12000 finish-install\n", ["--reset"], flash_busy=True
    ) as child:
        completed(child, 90)
    after = restart / "state"
    require_stock_pc(after)
    require(
        (after / "flash").read_bytes() == staged_flash(before), "Restart changed unarmed staging"
    )
    original, actual = reader(before), reader(after)
    count = original(0x8611D, 3)
    require(
        actual(0x8611D, 3) == count
        and all(
            actual(0x86120 + 16 * i, 16) == original(0x86120 + 16 * i, 16) for i in range(count)
        ),
        "Unarmed stage changed the catalog after restart",
    )
    require(verify_busy_reads(args, restart) == 0, "Restart wrote unarmed transaction storage")
    print("Durable unarmed staging rejects replay and survives Stock restart unchanged", flush=True)
