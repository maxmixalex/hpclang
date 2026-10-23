"""Native partial-programming retry and closed-transaction restart checks."""

import shutil
import struct
import zlib

from installation import AUTHORITY_SLOT, JOURNAL_SLOT, TARGET, fixture_identity, verify_probe
from prepare import prepare
from reservation import packager, reader
from verify import home_string, require, require_stock_pc, symbol_offsets


def reservation_value(flash, slot):
    capsule = bytes.fromhex("362030") + b"HPCSECT1"
    capsule += struct.pack("<3I", JOURNAL_SLOT, TARGET, 4096)
    capsule += flash[TARGET : TARGET + 4096]
    header = b"HPCMEM2\0" + fixture_identity()
    header += struct.pack(
        "<6I", slot, JOURNAL_SLOT, AUTHORITY_SLOT, TARGET, zlib.crc32(capsule), 32768
    )
    value = bytearray((i * 37 + 11) & 255 for i in range(32768))
    value[:80] = header
    return bytes(value)


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
    activation = struct.unpack_from(
        "<3I", (after / "sram").read_bytes(), base + symbols["activation_result"]
    )
    require(activation == (0, 0, 0), "Staged transaction or fresh receipt failed before activation")
    require(
        results[:9] == ((0,) * 9 if repeated else (0xFFFFFFFC, 0, 0, 0xFFFFFFFC, 0, 1, 0, 0, 0)),
        "Unexpected publication/retry/closure results",
    )
    require(results[10] == int(repeated), "Wrong publication or closed-repeat path")
    require(results[11] == (0 if repeated else 2), "Partial programming was not observed")
    require(
        home_string(reader(after), b"HPCLEASE")
        == reservation_value((before / "flash").read_bytes(), slot),
        "Reservation differs from its installation/transaction binding",
    )
    require(
        results[9] == 0 if repeated else results[9] > 0,
        "Unexpected publication or closed-repeat program count",
    )
    return results[9]


def run_reservation_collisions(args, baseline, root, slot, *, transfer, verify_busy_reads):
    pack = packager()
    expected = reservation_value((baseline / "flash").read_bytes(), slot)
    for label, value in (("different", b"user-owned reservation name"), ("identical", expected)):
        package = root / ("collision-" + label + "-package")
        package.mkdir()
        data = pack.immediate(0x2A2C) + pack.immediate(5 + 2 * len(value))
        data += "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in value)
        (package / "reservation.hp").write_bytes(
            b"HPHP49-C" + bytes(int(data[i : i + 2][::-1], 16) for i in range(0, len(data), 2))
        )
        occupied = root / ("collision-" + label + "-occupied")
        transfer(
            args,
            occupied,
            baseline,
            package,
            "LEASEIN",
            execute=False,
            package_name="reservation.hp",
            remote_commands=[
                "-95 CF HOME CLEAR",
                "'LEASEIN' RCL 'HPCLEASE' STO 'LEASEIN' PURGE",
            ],
        )
        before = occupied / "state"
        require(home_string(reader(before), b"HPCLEASE") == value, "Collision fixture differs")
        phase = root / ("collision-" + label + "-rejected")
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
        base = verify_probe(
            before,
            after,
            args.build,
            args.nm,
            -10,
            0xFFFFFFFF,
            image="installation-publication",
        )
        symbols = symbol_offsets(args.build / "installation-publication.elf", args.nm)
        results = struct.unpack_from(
            "<12I", (after / "sram").read_bytes(), base + symbols["publication_result"]
        )
        reservation = struct.unpack_from(
            "<2I", (after / "sram").read_bytes(), base + symbols["reservation_result"]
        )
        require(reservation == (0xFFFFFFF6, 0), "Collision attempted native RAM mutation")
        require(results == (0,) * 12, "Collision entered transaction publication")
        require(verify_busy_reads(args, phase) == 0, "Collision wrote Flash")
        require(home_string(reader(after), b"HPCLEASE") == value, "Collision adopted user data")
        print(f"Reservation creation rejected {label} existing content without writes", flush=True)


def run_publication(args, baseline, slot, *, transfer, emulator, completed, verify_busy_reads):
    from activation import run_activation_rejection
    from cancellation import run_cancellation
    from reservation_failure import run_failure

    root = args.output / "installation" / "publication"
    root.mkdir()
    run_reservation_collisions(
        args, baseline, root, slot, transfer=transfer, verify_busy_reads=verify_busy_reads
    )
    run_failure(args, baseline, root, transfer=transfer, verify_busy_reads=verify_busy_reads)
    run_activation_rejection(
        args, baseline, root, slot, transfer=transfer, verify_busy_reads=verify_busy_reads
    )
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
                home_string(reader(before), b"HPCLEASE")
                == reservation_value((baseline / "flash").read_bytes(), slot),
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
