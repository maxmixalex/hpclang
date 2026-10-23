"""Cancel an invocation-owned unpublished transaction and discard its fresh RAM."""

import struct

from installation import AUTHORITY_SLOT, JOURNAL_SLOT, verify_probe
from reservation import reader, require_absent
from staging import staged_flash
from verify import home_string, require, symbol_offsets


def run_abort(args, before, root, slot, *, transfer, verify_busy_reads):
    phase = root / "reservation-abort"
    expected = bytearray(staged_flash(before))
    for address, size in ((JOURNAL_SLOT, 4124), (AUTHORITY_SLOT, 32)):
        expected[address : address + 4] = b"\0" + (2 * size - 2).to_bytes(3, "little")
    transfer(
        args,
        phase,
        before,
        args.build,
        "ABORT",
        package_name="installation-abort.hp",
        # Stock preserves the executable on the stack while removing its HOME
        # binding. Its CODE then resides in TEMPOB rather than movable USEROB.
        remote_commands=[
            "-95 CF HOME CLEAR \"unrelated data\" 'HPCKEEP' STO 'ABORT' RCL 'ABORT' PURGE"
        ],
    )
    after = phase / "state"
    base = verify_probe(
        before,
        after,
        args.build,
        args.nm,
        0,
        slot,
        image="installation-abort",
        expected_flash=expected,
    )
    symbols = symbol_offsets(args.build / "installation-abort.elf", args.nm)
    result = struct.unpack_from(
        "<12I", (after / "sram").read_bytes(), base + symbols["publication_result"]
    )
    require(
        result[:4] == (0,) * 4 and result[4] > 0 and result[5:7] == (0, 0),
        "Cancellation did not discard and consume its fresh reservation",
    )
    require(verify_busy_reads(args, phase) == result[4], "Unexpected abort Flash operations")
    require_absent(reader(after), b"HPCLEASE")
    require(
        home_string(reader(after), b"HPCKEEP") == b"unrelated data",
        "Reservation cancellation changed user data",
    )
    print(
        "Native fresh reservation discarded after unpublished transaction cancellation", flush=True
    )
