"""Native retirement and claim closure against a real installed transaction."""

import shutil
import struct

from installation import verify_probe
from reservation import bound_header, packager, reader, require_absent
from verify import home_string, nibbles, require, require_stock_pc, symbol_offsets, verify_restart


def run_retirement(
    args,
    before,
    active,
    recovered,
    root,
    library,
    layout,
    record,
    slot,
    *,
    transfer,
    verify_busy_reads,
    close=False,
    prepared_states=None,
):
    """Bind the probe from the independently reconstructed installer output.

    Never populate expected ownership from an arbitrary surviving claim. The
    Neither classification nor claim closure may release RAM or reclaim Flash.
    """
    image = "installation-retirement-close" if close else "installation-retirement"
    data = nibbles(record[4:])

    def word(at):
        return int(data[at : at + 8][::-1], 16)

    journal = word(layout["recovery_binding_nibble"] - 8 + 16)
    authority = word(layout["recovery_binding_nibble"] - 8 + 24)
    claim = word(layout["admission_creation_nibble"] - 8)
    from template import fixture

    bound_library, expected, _ = fixture(
        library, layout, journal_slot=journal, authority_slot=authority, claim_slot=claim
    )
    require(record == expected, "Retirement expected root is not independently reconstructed")
    binding = bound_header(
        bound_library,
        layout["identity_object_nibble"],
        slot,
        journal,
        0x140000,
        (before / "flash").read_bytes(),
    )
    require(struct.unpack_from("<I", binding, 64)[0] == authority, "Retirement pair differs")
    binding = binding[:76] + struct.pack("<I", layout["reservation"]["bytes"])
    name = layout["reservation"]["name"].encode("ascii")
    require(0 < len(name) < 128, "Invalid retirement resource name")
    package = root / "retirement-package"
    package.mkdir()
    symbols = symbol_offsets(args.build / (image + ".elf"), args.nm)
    initialized = bytearray((args.build / (image + ".bin")).read_bytes())
    fields = {
        "retirement_binding": binding,
        "retirement_name": name.ljust(128, b"\0"),
        "retirement_name_length": struct.pack("<I", len(name)),
        "retirement_library_id": struct.pack("<I", layout["library_id"]),
        "retirement_claim_slot": struct.pack("<I", claim),
    }
    for field, value in fields.items():
        at = symbols[field]
        require(at >= 0 and at + len(value) <= len(initialized), "Retirement field outside ELF")
        initialized[at : at + len(value)] = value
    (package / (image + ".bin")).write_bytes(initialized)
    (package / (image + ".hp")).write_bytes(
        packager().package(bytes(initialized), symbols["__image_end"], model=args.model)
    )
    shutil.copyfile(args.build / (image + ".elf"), package / (image + ".elf"))
    ready_states = {}
    for label, source, result, expected_phase in (
        ("active", active, -4, 99),
        ("recovered", recovered, 0, 3 if close else 1),
    ):
        # The completed installer leaves its large original source fixture in
        # HOME. Consume that duplicate before receiving another CODE program;
        # keep the actual installed root and active reservation unchanged.
        if prepared_states is None:
            prepared = root / ("retirement-" + label + "-prepared")
            transfer(
                args,
                prepared,
                source,
                package,
                "RETIRE",
                installed=True,
                execute=False,
                remote_commands=["-95 CF HOME CLEAR 'HPCTEMPLATE' PURGE"],
            )
            require(verify_busy_reads(args, prepared) == 0, "Fixture cleanup programmed Flash")
            ready = prepared / "state"
        else:
            ready = prepared_states[label]
        ready_states[label] = ready
        require(
            (ready / "flash").read_bytes() == (source / "flash").read_bytes(),
            "Retirement fixture cleanup changed Flash",
        )
        require_absent(reader(ready), b"HPCTEMPLATE")
        if result < 0:
            require(
                home_string(reader(ready), name) == home_string(reader(source), name),
                "Retirement fixture cleanup changed the active reservation",
            )
        else:
            require_absent(reader(ready), name)
        phase = root / ("retirement-" + label)
        transfer(args, phase, ready, package, "RETIRE", package_name=image + ".hp", consume=True)
        expected_flash = bytearray((ready / "flash").read_bytes())
        writes = int(close and result == 0)
        if writes:
            require(expected_flash[claim + 16] == 0x7E, "Expected a real CREATED claim")
            expected_flash[claim + 16] = 0
        base = verify_probe(
            ready,
            phase / "state",
            package,
            args.nm,
            result,
            slot if result == 0 else 0xFFFFFFFF,
            image=image,
            expected_flash=bytes(expected_flash),
        )
        ram = (phase / "state/sram").read_bytes()
        require(
            struct.unpack_from("<I", ram, base + symbols["retirement_claim_phase"])[0]
            == expected_phase,
            "Retirement claim phase differs",
        )
        require(verify_busy_reads(args, phase) == writes, "Unexpected retirement Flash writes")
        if close:
            require(
                struct.unpack_from("<4I", ram, base + symbols["retirement_close_result"])
                == (result & 0xFFFFFFFF, 0, writes, 0),
                "Claim closure or its idempotent repeat differs",
            )
        if result < 0:
            require(
                home_string(reader(phase / "state"), name) == home_string(reader(ready), name),
                "Retirement changed the active reservation",
            )
        else:
            require_absent(reader(phase / "state"), name)
    print(
        "Native retired claim closure and unchanged repeat pass"
        if close
        else "Native retirement rejects an active transaction and accepts its recovered state",
        flush=True,
    )
    return ready_states


def run_closed_restart(args, clean, closed, root, name, *, emulator, completed, verify_busy_reads):
    """Retain the closed claim and original root across two real Stock resets."""
    previous = closed
    for repeat in range(2):
        phase = root / f"closed-claim-restart-{repeat}"
        shutil.copytree(previous, phase / "state")
        with emulator(
            args,
            phase,
            "10000 capture home\n12000 finish-install\n",
            ["--reset"],
            flash_busy=True,
            reset_checkpoint=True,
        ) as child:
            completed(child, 90)
        state = phase / "state"
        require_stock_pc(state)
        verify_restart(clean, closed, state)
        require_absent(reader(state), name)
        require_absent(reader(state), b"HPCTEMPLATE")
        require(verify_busy_reads(args, phase) == 0, "Closed claim replayed writes after reset")
        previous = state
    print("Closed native claim survives repeated reset without Flash writes", flush=True)
