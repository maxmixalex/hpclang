"""Verify a failed RAM creation retains execution instead of resuming Stock."""

import configparser
import struct
import subprocess

from installation import verify_owned_image
from verify import require, symbol_offsets


IMAGE = "installation-reservation-failure"


def verify_failure(before, after, build, nm):
    require(
        (before / "flash").read_bytes() == (after / "flash").read_bytes(),
        "Failed reservation creation modified Flash",
    )
    ram = (after / "sram").read_bytes()
    initialized = (build / (IMAGE + ".bin")).read_bytes()
    symbols = symbol_offsets(build / (IMAGE + ".elf"), nm)
    matches = []
    start = 0
    while (start := ram.find(initialized, start)) >= 0:
        if start + symbols["__image_end"] <= len(ram):
            result = struct.unpack_from("<2I", ram, start + symbols["reservation_result"])
            if result == (0xFFFFFFFB, 1):
                matches.append(start)
        start += 1
    require(len(matches) == 1, "Missing or ambiguous first-write failure image")
    base = matches[0]
    verify_owned_image(ram, initialized, symbols, (build / (IMAGE + ".hp")).read_bytes(), base)
    require(
        struct.unpack_from("<12I", ram, base + symbols["publication_result"]) == (0,) * 12,
        "Failed reservation creation entered transaction publication",
    )
    require(
        struct.unpack_from("<I", ram, base + symbols["installation_result"])[0] == 0,
        "Failed reservation creation returned a normal predicate result",
    )
    state = configparser.ConfigParser()
    state.read(after / "state")
    cpu = state["s3c2410-arm"]
    pc, cpsr = int(cpu["reg-15"], 0), int(cpu["cpsr"], 0)
    require(
        cpsr & 0xC0 == 0xC0 and cpsr & 31 not in (0x17, 0x1B),
        "Failed reservation creation lost IRQ ownership or entered an exception",
    )
    listing = subprocess.check_output([nm, "-S", build / (IMAGE + ".elf")], text=True)
    spans = [line.split() for line in listing.splitlines() if line.endswith(" reservation_failed")]
    require(len(spans) == 1 and len(spans[0]) == 4, "Missing owned failure loop")
    address, size = int(spans[0][0], 16), int(spans[0][1], 16)
    require(
        0 <= pc - 0x08000000 - base - address < size + 4,
        "Reservation failure did not remain in the owned failure loop",
    )


def run_failure(args, baseline, root, *, transfer, verify_busy_reads):
    phase = root / "reservation-write-failure"
    transfer(
        args,
        phase,
        baseline,
        args.build,
        "PUBFAIL",
        package_name=IMAGE + ".hp",
        remote_commands=["-95 CF HOME CLEAR 'PUBFAIL' RCL 'PUBFAIL' PURGE"],
    )
    verify_failure(baseline, phase / "state", args.build, args.nm)
    require(verify_busy_reads(args, phase) == 0, "RAM failure issued a Flash operation")
    print(
        "Failed RAM creation retained code/owner/guards/IRQs without Flash publication", flush=True
    )
