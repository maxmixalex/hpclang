"""Cancel fixture-owned unpublished records without an original desired image."""

import shutil
import struct

from installation import AUTHORITY_SLOT, JOURNAL_SLOT, verify_probe
from publication import reservation_value
from reservation import reader
from verify import home_string, require, symbol_offsets


def run_cancellation(args, baseline, records, slot, *, transfer, verify_busy_reads):
    root = args.output / "installation" / "cancellation"
    root.mkdir()
    symbols = symbol_offsets(args.build / "installation-cancellation.elf", args.nm)
    for label in ("journal", "authority"):
        seed = root / (label + "-seed")
        shutil.copytree(baseline, seed)
        interrupted = bytearray((seed / "flash").read_bytes())
        for address, size in ((JOURNAL_SLOT, 4124), (AUTHORITY_SLOT, 32)):
            interrupted[address : address + size] = b"\xff" * size
        if label == "authority":
            interrupted[JOURNAL_SLOT : JOURNAL_SLOT + 4124] = records[
                JOURNAL_SLOT : JOURNAL_SLOT + 4124
            ]
        partial = JOURNAL_SLOT if label == "journal" else AUTHORITY_SLOT
        # Program only some body bits, with status still unpublished. Generate
        # this fixture from the runtime-captured journal, never ROM snapshots.
        for index in range(1, 5):
            interrupted[partial + index] = records[partial + index] | 0xAA
        (seed / "flash").write_bytes(interrupted)
        expected = bytearray(interrupted)
        for address, size in ((JOURNAL_SLOT, 4124), (AUTHORITY_SLOT, 32)):
            expected[address : address + 4] = b"\0" + (2 * size - 2).to_bytes(3, "little")
        phase = root / label
        transfer(
            args,
            phase,
            seed,
            args.build,
            "CANCEL",
            package_name="installation-cancellation.hp",
            remote_commands=["-95 CF HOME CLEAR 'CANCEL' RCL 'CANCEL' PURGE"],
        )
        after = phase / "state"
        base = verify_probe(
            seed,
            after,
            args.build,
            args.nm,
            0,
            slot,
            image="installation-cancellation",
            expected_flash=expected,
        )
        result, operations, repeated = struct.unpack_from(
            "<3I", (after / "sram").read_bytes(), base + symbols["cancellation_result"]
        )
        require(result == repeated == 0 and operations > 0, "Cancellation or repeat failed")
        require(
            verify_busy_reads(args, phase) == operations,
            "Unexpected cancellation Flash operations",
        )
        require(
            home_string(reader(after), b"HPCLEASE")
            == reservation_value((baseline / "flash").read_bytes(), slot),
            "Cancellation modified the reservation",
        )
        print(f"Native unpublished transaction cancellation verified: {label}", flush=True)
