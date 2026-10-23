"""Preserve a replacement HOME object while closing a retired native claim."""

import struct

from installation import verify_probe
from reservation import reader
from verify import home_string, require, symbol_offsets


def run_conflict(args, before, package, root, name, *, transfer, verify_busy_reads):
    """Use Stock STO to create user data after recovery, then require rejection."""
    root.mkdir(exist_ok=True)
    identifier = name.decode("ascii")
    require(identifier.isidentifier(), "Conflict fixture requires an identifier")
    image = "installation-retirement-close"
    symbols = symbol_offsets(package / (image + ".elf"), args.nm)
    original = (before / "flash").read_bytes()
    payload = b"replacement user data"
    prepared = root / "replacement"
    transfer(
        args,
        prepared,
        before,
        package,
        "RETIRECONFLICT",
        installed=True,
        execute=False,
        remote_commands=[f"-95 CF HOME CLEAR \"{payload.decode()}\" '{identifier}' STO"],
    )
    ready = prepared / "state"
    require((ready / "flash").read_bytes() == original, "Replacement changed Flash")
    require(verify_busy_reads(args, prepared) == 0, "Replacement programmed Flash")
    require(home_string(reader(ready), name) == payload, "Replacement was not stored")
    rejected = root / "rejected"
    transfer(
        args,
        rejected,
        ready,
        package,
        "RETIRECONFLICT",
        package_name=image + ".hp",
        consume=True,
    )
    state = rejected / "state"
    base = verify_probe(ready, state, package, args.nm, -4, 0xFFFFFFFF, image=image)
    ram = (state / "sram").read_bytes()
    require(
        struct.unpack_from("<4I", ram, base + symbols["retirement_close_result"])
        == (0xFFFFFFFC, 0, 0, 0),
        "Conflicting resource was not rejected before programming",
    )
    require((state / "flash").read_bytes() == original, "Conflict changed Flash")
    require(verify_busy_reads(args, rejected) == 0, "Conflict programmed Flash")
    require(home_string(reader(state), name) == payload, "Closure changed user replacement")
    print(
        "Retired claim closure preserves a Stock-created same-name replacement",
        flush=True,
    )
