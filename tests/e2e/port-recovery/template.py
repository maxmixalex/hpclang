"""Build witness constants and an independent oracle for the real recovery library."""

import argparse
from functools import partial
import json
from pathlib import Path
import struct

from reservation import packager, reader
from verify import home_object, home_string, nibbles, require, symbol_offsets


def fixture(library, layout, *, journal_slot=None, authority_slot=None, claim_slot=None):
    """Rebind through the host packager, independently of the ARM implementation."""
    data = list(nibbles(library[8:]))
    span = 5 + int("".join(data[5:10])[::-1], 16)

    def put(at, value):
        data[at : at + len(value) * 2] = nibbles(value)

    require((journal_slot is None) == (authority_slot is None), "Incomplete binding")
    require(claim_slot is None or journal_slot is not None, "Claim requires transaction binding")
    if journal_slot is not None:
        for name in ("recovery_binding_nibble", "admission_binding_nibble"):
            at = layout[name] - 8
            put(at + 16, journal_slot.to_bytes(4, "little"))
            put(at + 24, authority_slot.to_bytes(4, "little"))
        if claim_slot is not None:
            put(layout["admission_creation_nibble"] - 8, claim_slot.to_bytes(4, "little"))
        put(layout["identity_object_nibble"], b"HPCROOT1" + bytes(40))
        crc = 0
        for nibble in data[5 : span - 4]:
            crc = (crc >> 4) ^ (((crc ^ int(nibble, 16)) & 15) * 0x1081)
        put(span - 4, crc.to_bytes(2, "little"))
        packed = bytes(int(data[i], 16) | int(data[i + 1], 16) << 4 for i in range(0, len(data), 2))
        library = packager().seal_recovery_identity(
            library[:8] + packed, layout["identity_object_nibble"]
        )
    obj = nibbles(library[8:])
    allocated = (span + 15) & ~7
    body = obj[:span] + "f" * (allocated - span - 8)
    record = b"\x53" + ((allocated - 2) | 0x100000).to_bytes(3, "little")
    record += bytes(int(body[i], 16) | int(body[i + 1], 16) << 4 for i in range(0, len(body), 2))
    require(len(record) == layout["record_bytes"], "Template allocation size differs")
    return library, record, layout


def generate(library_path, layout_path, output):
    library = library_path.read_bytes()
    layout = json.loads(layout_path.read_text())
    _, record, _ = fixture(library, layout)
    data = nibbles(library[8:])
    span = 5 + int(data[5:10][::-1], 16)
    at = layout["identity_object_nibble"]
    identity = bytes(int(data[i : i + 2][::-1], 16) for i in range(at, at + 96, 2))
    output.write_text(
        "/* Generated from the actual recovery/admission build. */\n#include <stdint.h>\n"
        + f"_Alignas(4) uint8_t expected_library[{len(record)}];\n"
        + "#if defined(HPCLANG_INSTALLATION_TEMPLATE_CREATION) && defined(HPCLANG_INSTALLATION_STORE)\n"
        + f'extern uint8_t prepared_library[{len(record)}] __attribute__((alias("expected_library")));\n'
        + "#else\n"
        + f"_Alignas(4) uint8_t prepared_library[{len(record)}];\n#endif\n"
        + "const uint8_t expected_identity[48] = {"
        + ",".join(map(str, identity))
        + "};\n"
        + f"const uint32_t preparation_bytes = {len(record)}u, template_span = {span}u;\n"
        + f"const uint32_t template_recovery_binding = {layout['recovery_binding_nibble']}u;\n"
        + f"const uint32_t template_admission_binding = {layout['admission_binding_nibble']}u;\n"
        + f"const uint32_t template_identity = {at}u;\n"
        + f"const uint32_t template_creation = {layout.get('admission_creation_nibble', 0xFFFFFFFF)}u;\n"
    )


def verify_source(state, library):
    read = reader(state)
    obj = home_object(read, b"HPCTEMPLATE")
    data = nibbles(library[8:])
    span = 5 + int(data[5:10][::-1], 16)
    require(
        all(read(obj + i, 1) == int(data[i], 16) for i in range(span)),
        "Source recovery template changed",
    )


def run_installed_recovery(
    args, before, root, library, layout, *, emulator, completed, verify_busy_reads
):
    """Exercise the actually installed/rebound root with an injected transaction.

    The fixture constructs authority; this does not establish production
    installation provenance, resource ownership or activation permission.
    """
    import shutil
    from prepare import prepare
    from verify import verify_flash, verify_restart, verify_retired

    obj = nibbles(library[8:])
    at = layout["recovery_binding_nibble"] - 8
    journal = int(obj[at + 16 : at + 24][::-1], 16)
    authority = int(obj[at + 24 : at + 32][::-1], 16)
    require(authority == journal + 4124, "Expected contiguous prepared transaction")
    target = 0x140000
    fault = root / "transaction-fault"
    prepare(before, fault, journal, target, bound=True)

    def boot(source, label):
        phase = root / label
        phase.mkdir()
        shutil.copytree(source, phase / "state")
        with emulator(
            args,
            phase,
            "10000 capture home\n12000 finish-install\n",
            ["--reset"],
            flash_busy=True,
            reset_checkpoint=True,
        ) as child:
            completed(child, 90)
        return phase / "state"

    repaired = boot(fault, "transaction-recovered")
    verify_flash(
        (fault / "flash").read_bytes(),
        (repaired / "flash").read_bytes(),
        journal,
        target,
        retire=True,
        authority_slot=authority,
    )
    verify_restart(before, repaired, repaired)
    operations = verify_busy_reads(args, root / "transaction-recovered")
    require(not args.flash_busy_reads or operations > 0, "Recovery bypassed Flash polling")
    repeated = boot(repaired, "transaction-rechecked")
    verify_restart(before, repaired, repeated)
    verify_retired(
        (repaired / "flash").read_bytes(),
        (repeated / "flash").read_bytes(),
        journal,
        target,
        authority_slot=authority,
    )
    require(
        verify_busy_reads(args, root / "transaction-rechecked") == 0,
        "Closed installed transaction programmed Flash again",
    )
    for state in (repaired, repeated):
        verify_source(state, (args.build / "template.hp").read_bytes())
    return {"journal": journal, "authority": authority, "operations": operations}


def run_creation_template(args, clean, root, *, transfer, verify_busy_reads):
    root = root / "creation-template"
    root.mkdir()
    build = args.build
    library = (build / "template-creation.hp").read_bytes()
    layout = json.loads((build / "template-creation-layout.json").read_text())
    deposited = root / "source"
    transfer(
        args,
        deposited,
        clean,
        build,
        "HPCTEMPLATE",
        execute=False,
        package_name="template-creation.hp",
        remote_commands=["-95 CF HOME CLEAR"],
    )
    before = deposited / "state"
    verify_source(before, library)
    prepared = root / "prepared"
    image = "installation-template-creation"
    transfer(args, prepared, before, build, "CLAIMPREP", package_name=image + ".hp")
    verify_creation_template(
        args, before, prepared, library, layout, verify_busy_reads=verify_busy_reads
    )
    return before, library, layout


def verify_creation_template(args, before, prepared, library, layout, *, verify_busy_reads):
    from installation import verify_placement, verify_preparation, verify_probe
    from startup import verify_startup

    build = args.build
    image = "installation-template-creation"
    make_fixture = partial(fixture, library, layout)
    after = prepared / "state"
    base = verify_probe(before, after, build, args.nm, -8, 0xFFFFFFFF, image=image)
    verify_startup(before, after, build, args.nm, base, image=image)
    verify_placement(
        before, after, build, args.nm, base, existing=False, make_fixture=make_fixture, image=image
    )
    verify_preparation(
        before,
        after,
        build,
        args.nm,
        base,
        existing=False,
        make_fixture=make_fixture,
        image=image,
        creation=True,
    )
    verify_source(after, library)
    require(verify_busy_reads(args, prepared) == 0, "Claim-aware draft wrote Flash")
    print("Real claim-bound template matches independent four-extent draft", flush=True)


def run_creation_name_collision(args, before, root, library, *, transfer, verify_busy_reads):
    phase = root / "creation-name-collision"
    image = "installation-template-creation-store"
    transfer(
        args,
        phase,
        before,
        args.build,
        "CLAIMCOLLIDE",
        package_name=image + ".hp",
        remote_commands=[
            "-95 CF HOME CLEAR \"existing user data\" 'HPCLEASE' STO",
            "'CLAIMCOLLIDE' RCL 'CLAIMCOLLIDE' PURGE",
        ],
    )
    verify_creation_name_collision(
        args, before, phase, library, verify_busy_reads=verify_busy_reads
    )


def verify_creation_name_collision(args, before, phase, library, *, verify_busy_reads):
    verify_creation_rejection(args, before, phase, library, -6, verify_busy_reads=verify_busy_reads)
    require(
        home_string(reader(phase / "state"), b"HPCLEASE") == b"existing user data",
        "Name collision changed the user's variable",
    )
    print("Real installation rejects a pre-existing resource name before Flash writes", flush=True)


def verify_creation_rejection(args, before, phase, library, result, *, verify_busy_reads):
    from installation import verify_probe
    from prepare import free_tail

    image = "installation-template-creation-store"
    state = phase / "state"
    # Preparation precedes the name check, so diagnostics retain its planned
    # root location even though the rejection must leave all Flash unchanged.
    span = 5 + int(nibbles(library[8:])[5:10][::-1], 16)
    size = ((span + 15) & ~7) // 2
    flash = (before / "flash").read_bytes()
    candidates = [free_tail(flash, bank) for bank in range(0x140000, 0x200000, 0x20000)]
    candidates = [
        at for at in candidates if at >= 0x141000 and at + size < (at & ~0x1FFFF) + 0x20000
    ]
    require(candidates, "Rejection fixture has no root capacity")
    base = verify_probe(before, state, args.build, args.nm, result, candidates[0], image=image)
    symbols = symbol_offsets(args.build / (image + ".elf"), args.nm)
    require(
        struct.unpack_from("<2I", (state / "sram").read_bytes(), base + symbols["storage_result"])
        == (0, 0),
        "Rejected installation attempted root storage",
    )
    require(verify_busy_reads(args, phase) == 0, "Rejected installation wrote Flash")
    verify_source(state, library)


def run_creation_storage(
    args, before, root, library, layout, *, transfer, emulator, completed, verify_busy_reads
):
    from store import restart_storage, verify_storage

    verify_source(before, library)
    run_creation_name_collision(
        args, before, root, library, transfer=transfer, verify_busy_reads=verify_busy_reads
    )
    root = root / "creation-storage"
    root.mkdir()
    image = "installation-template-creation-store"
    unsafe = root / "unsafe-execution"
    transfer(args, unsafe, before, args.build, "CLAIMSTORE", package_name=image + ".hp")
    verify_creation_rejection(
        args, before, unsafe, library, -5, verify_busy_reads=verify_busy_reads
    )
    from reservation import require_absent

    require_absent(reader(unsafe / "state"), b"HPCLEASE")
    print("Movable USEROB execution rejected before Flash publication", flush=True)
    stored = root / "stored"
    transfer(
        args,
        stored,
        before,
        args.build,
        "CLAIMSTORE",
        package_name=image + ".hp",
        consume=True,
        execution_ms=60000,
    )
    slot, rebound, operations = verify_storage(
        before,
        stored / "state",
        args.build,
        args.nm,
        make_fixture=partial(fixture, library, layout),
        image=image,
        staged=True,
    )
    verify_source(stored / "state", library)
    require(
        verify_busy_reads(args, stored) == operations,
        "Claim-bound storage bypassed expected Flash polling",
    )
    restart = root / "restart"
    body = nibbles(rebound[8:])
    binding = layout["recovery_binding_nibble"] - 8
    pair = tuple(int(body[at : at + 8][::-1], 16) for at in (binding + 16, binding + 24))
    restart_storage(
        args,
        stored,
        restart,
        rebound,
        slot,
        emulator=emulator,
        completed=completed,
        library_id=layout["library_id"],
        staged_pair=pair,
    )
    verify_source(restart / "state", library)
    require(
        home_string(reader(restart / "state"), b"HPCLEASE")
        == home_string(reader(stored / "state"), b"HPCLEASE"),
        "Restart changed the created recovery resource",
    )
    print("Claim-bound root and staged transaction survive Stock restart unchanged", flush=True)
    return {"slot": slot, "operations": operations}


def run_creation_cleanup(
    args,
    before,
    root,
    library,
    layout,
    *,
    transfer,
    emulator,
    completed,
    verify_busy_reads,
):
    from reservation import require_absent
    from store import restart_storage, verify_storage

    verify_source(before, library)
    root = root / "creation-cleanup"
    root.mkdir()
    image = "installation-template-creation-cancel-store"
    stored = root / "stored"
    transfer(
        args,
        stored,
        before,
        args.build,
        "CLAIMCLOSE",
        package_name=image + ".hp",
        consume=True,
        execution_ms=60000,
    )
    slot, rebound, operations = verify_storage(
        before,
        stored / "state",
        args.build,
        args.nm,
        make_fixture=partial(fixture, library, layout),
        image=image,
        staged=True,
        closed=True,
    )
    verify_source(stored / "state", library)
    require(verify_busy_reads(args, stored) == operations, "Cleanup bypassed Flash polling")
    restart = root / "restart"
    restart_storage(
        args,
        stored,
        restart,
        rebound,
        slot,
        emulator=emulator,
        completed=completed,
        library_id=layout["library_id"],
    )
    verify_source(restart / "state", library)
    require_absent(reader(restart / "state"), b"HPCLEASE")
    require(
        verify_busy_reads(args, restart) == 0,
        "Closed installation wrote Flash after reset",
    )
    print(
        "Real installation cancellation, resource release and claim closure survive restart",
        flush=True,
    )
    return {"slot": slot, "operations": operations}


def run_template(args, clean, root, *, transfer, emulator, completed, verify_busy_reads):
    from installation import verify_placement, verify_preparation, verify_probe
    from startup import verify_startup
    from store import restart_storage, verify_storage

    claim_before, claim_library, claim_layout = run_creation_template(
        args, clean, root, transfer=transfer, verify_busy_reads=verify_busy_reads
    )
    claim_storage = run_creation_storage(
        args,
        claim_before,
        root,
        claim_library,
        claim_layout,
        transfer=transfer,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )

    claim_cleanup = run_creation_cleanup(
        args,
        claim_before,
        root,
        claim_library,
        claim_layout,
        transfer=transfer,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )
    root = root / "real-template"
    root.mkdir()
    build = args.build
    library = (build / "template.hp").read_bytes()
    layout = json.loads((build / "template-layout.json").read_text())
    make_fixture = partial(fixture, library, layout)
    deposited = root / "template"
    transfer(
        args,
        deposited,
        clean,
        build,
        "HPCTEMPLATE",
        execute=False,
        package_name="template.hp",
        remote_commands=["-95 CF HOME CLEAR"],
    )
    before = deposited / "state"
    verify_source(before, library)
    prepared = root / "prepared"
    image = "installation-template"
    transfer(args, prepared, before, build, "REALPREP", package_name=image + ".hp")
    after = prepared / "state"
    base = verify_probe(before, after, build, args.nm, -8, 0xFFFFFFFF, image=image)
    verify_startup(before, after, build, args.nm, base, image=image)
    verify_placement(
        before,
        after,
        build,
        args.nm,
        base,
        existing=False,
        make_fixture=make_fixture,
        image=image,
    )
    verify_preparation(
        before,
        after,
        build,
        args.nm,
        base,
        existing=False,
        make_fixture=make_fixture,
        image=image,
    )
    symbols = symbol_offsets(build / (image + ".elf"), args.nm)
    values = struct.unpack_from(
        "<3I", (after / "sram").read_bytes(), base + symbols["template_result"]
    )
    require(values[1:] == (1, layout["record_bytes"]), "Template identity/copy differs")
    verify_source(after, library)
    require(verify_busy_reads(args, prepared) == 0, "Template preparation wrote Flash")

    stored = root / "stored"
    transfer(
        args,
        stored,
        before,
        build,
        "REALSTORE",
        package_name="installation-template-store.hp",
        # A real library programs tens of thousands of words with simulated
        # busy polling; the small-witness budget can terminate it mid-write.
        execution_ms=60000,
    )
    slot, rebound, operations = verify_storage(
        before,
        stored / "state",
        build,
        args.nm,
        make_fixture=make_fixture,
        image="installation-template-store",
    )
    verify_source(stored / "state", library)
    require(
        verify_busy_reads(args, stored) == operations,
        "Real storage bypassed Flash polling",
    )
    restart_storage(
        args,
        stored,
        root / "restart",
        rebound,
        slot,
        emulator=emulator,
        completed=completed,
        library_id=layout["library_id"],
    )

    wrong = root / "wrong-template"
    transfer(
        args,
        wrong,
        clean,
        build,
        "HPCTEMPLATE",
        execute=False,
        package_name="expected-library.hp",
        remote_commands=["-95 CF HOME CLEAR"],
    )
    rejected = root / "rejected"
    transfer(args, rejected, wrong / "state", build, "REALPREP", package_name=image + ".hp")
    base = verify_probe(
        wrong / "state",
        rejected / "state",
        build,
        args.nm,
        -100,
        0xFFFFFFFF,
        image=image,
    )
    values = struct.unpack_from(
        "<3I", (rejected / "state/sram").read_bytes(), base + symbols["template_result"]
    )
    require(
        values[0] != 0 and values[1] != 1 and values[2] == 0,
        "Wrong template was not rejected before copying",
    )
    verify_source(rejected / "state", (build / "expected-library.hp").read_bytes())
    require(verify_busy_reads(args, rejected) == 0, "Rejected template wrote Flash")
    recovery = run_installed_recovery(
        args,
        root / "restart/state",
        root,
        rebound,
        layout,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )
    print(
        "Real recovery template preparation, storage, restart, rejection and recovery passed",
        flush=True,
    )
    return {
        "bytes": layout["record_bytes"],
        "creation_storage": claim_storage,
        "creation_cleanup": claim_cleanup,
        "slot": slot,
        "operations": operations,
        "recovery": recovery,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("layout", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.library, args.layout, args.output)
