"""Native partial-programming retry and closed-transaction restart checks."""

import shutil
import struct

from installation import AUTHORITY_SLOT, JOURNAL_SLOT, TARGET, verify_probe
from prepare import prepare
from reservation import reader
from verify import home_string, require, require_stock_pc, symbol_offsets


RESERVATION = bytes((i * 37 + 11) & 255 for i in range(32768))


def verify_publication(before, after, build, nm, expected, slot, repeated):
    base = verify_probe(
        before,
        after,
        build,
        nm,
        0,
        slot,
        image="installation-publication",
        expected_flash=expected,
    )
    symbols = symbol_offsets(build / "installation-publication.elf", nm)
    results = struct.unpack_from(
        "<12I", (after / "sram").read_bytes(), base + symbols["publication_result"]
    )
    require(
        results[:9] == ((0,) * 9 if repeated else (0xFFFFFFFC, 0, 0, 0xFFFFFFFC, 0, 1, 0, 0, 0)),
        "Unexpected publication/retry/closure results",
    )
    require(results[10] == int(repeated), "Wrong publication or closed-repeat path")
    require(results[11] == (0 if repeated else 2), "Partial programming was not observed")
    require(home_string(reader(after), b"HPCLEASE") == RESERVATION, "Reservation was modified")
    require(
        results[9] == 0 if repeated else results[9] > 0,
        "Unexpected publication or closed-repeat program count",
    )
    return results[9]


def run_publication(args, baseline, slot, *, transfer, emulator, completed, verify_busy_reads):
    from cancellation import run_cancellation

    root = args.output / "installation" / "publication"
    root.mkdir()
    prepared = root / "expected"
    prepare(baseline, prepared, JOURNAL_SLOT, TARGET, bound=True)
    records = (prepared / "flash").read_bytes()
    expected = bytearray((baseline / "flash").read_bytes())
    for address, size in ((JOURNAL_SLOT, 4124), (AUTHORITY_SLOT, 32)):
        expected[address : address + size] = records[address : address + size]
        expected[address] = 0
    before = baseline
    for label, repeated in (("publish", False), ("closed-repeat", True)):
        phase = root / label
        transfer(
            args,
            phase,
            before,
            args.build,
            "PUBTEST",
            package_name="installation-publication.hp",
            remote_commands=["-95 CF HOME CLEAR 'PUBTEST' RCL 'PUBTEST' PURGE"],
        )
        after = phase / "state"
        operations = verify_publication(
            before, after, args.build, args.nm, expected, slot, repeated
        )
        require(
            verify_busy_reads(args, phase) == operations,
            "Unexpected Flash operations during publication or closed repeat",
        )
        print(f"Native transaction publication verified: {label}", flush=True)
        if not repeated:
            restart = root / "restart"
            restart.mkdir()
            shutil.copytree(after, restart / "state")
            with emulator(
                args, restart, "10000 capture home\n12000 finish-install\n", ["--reset"]
            ) as child:
                completed(child, 90)
            before = restart / "state"
            require_stock_pc(before)
            require(
                (before / "flash").read_bytes() == expected, "Restart changed transaction Flash"
            )
            require(
                home_string(reader(before), b"HPCLEASE") == RESERVATION,
                "Restart lost or changed the reservation",
            )
    run_cancellation(
        args,
        root / "restart" / "state",
        records,
        slot,
        transfer=transfer,
        verify_busy_reads=verify_busy_reads,
    )
