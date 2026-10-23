"""A staged transaction cannot activate with a mismatched creation receipt."""

import struct

from installation import verify_owned_image
from publication import reservation_value
from reservation import reader
from reservation_failure import verify_retained_execution
from staging import staged_flash
from verify import home_string, require, symbol_offsets


IMAGE = "installation-activation-reject"


def run_activation_rejection(args, before, root, slot, *, transfer, verify_busy_reads):
    phase = root / "activation-rejected"
    transfer(
        args,
        phase,
        before,
        args.build,
        "ARMFAIL",
        package_name=IMAGE + ".hp",
        remote_commands=[
            "-95 CF HOME CLEAR \"unrelated data\" 'HPCKEEP' STO 'ARMFAIL' RCL 'ARMFAIL' PURGE"
        ],
    )
    after = phase / "state"
    require(
        (after / "flash").read_bytes() == staged_flash(before), "Rejection changed staged Flash"
    )
    ram = (after / "sram").read_bytes()
    initialized = (args.build / (IMAGE + ".bin")).read_bytes()
    symbols = symbol_offsets(args.build / (IMAGE + ".elf"), args.nm)
    matches = []
    start = 0
    while (start := ram.find(initialized, start)) >= 0:
        if start + symbols["__image_end"] <= len(ram):
            result = struct.unpack_from("<3I", ram, start + symbols["activation_result"])
            if result == (0, 0, 0xFFFFFFF6):
                matches.append(start)
        start += 1
    require(len(matches) == 1, "Missing or ambiguous rejected activation image")
    base = matches[0]
    verify_owned_image(ram, initialized, symbols, (args.build / (IMAGE + ".hp")).read_bytes(), base)
    results = struct.unpack_from("<12I", ram, base + symbols["publication_result"])
    require(
        results == (0xFFFFFFFC, 0, 0, 0xFFFFFFFC, 0, 0, 0, 0, 0, 0, 0, 2),
        "Rejected receipt entered activation or retirement",
    )
    calls = struct.unpack_from("<I", ram, base + symbols["publication_calls"])[0]
    require(calls > 0 and verify_busy_reads(args, phase) == calls, "Staging bypassed busy polling")
    require(
        home_string(reader(after), b"HPCLEASE")
        == reservation_value((before / "flash").read_bytes(), slot),
        "Rejected activation changed the reservation",
    )
    original, actual = reader(before), reader(after)
    count = original(0x8611D, 3)
    require(
        actual(0x8611D, 3) == count
        and all(
            actual(0x86120 + 16 * i, 16) == original(0x86120 + 16 * i, 16) for i in range(count)
        ),
        "Rejected activation changed the native catalog",
    )
    require(home_string(actual, b"HPCKEEP") == b"unrelated data", "Unrelated HOME data changed")
    verify_retained_execution(after, args.build, args.nm, IMAGE, base)
    print("Mismatched creation receipt retained execution with the transaction unarmed", flush=True)
