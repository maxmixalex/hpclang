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
    name = layout["reservation"]["name"].encode("ascii")
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
        + f"const uint32_t template_library_id = {layout['library_id']}u;\n"
        + "const uint8_t template_reservation_name[] = {"
        + ",".join(map(str, name))
        + "};\n"
        + f"const uint32_t template_reservation_name_length = {len(name)}u;\n"
        + f"const uint32_t template_reservation_bytes = {layout['reservation']['bytes']}u;\n"
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


def run_creation_name_collision(
    args, before, root, library, layout, *, transfer, verify_busy_reads
):
    name = layout["reservation"]["name"]
    require(name.isascii() and name.isidentifier(), "Collision fixture requires an identifier")
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
            f"-95 CF HOME CLEAR \"existing user data\" '{name}' STO",
            "'CLAIMCOLLIDE' RCL 'CLAIMCOLLIDE' PURGE",
        ],
    )
    verify_creation_name_collision(
        args, before, phase, library, layout, verify_busy_reads=verify_busy_reads
    )


def verify_creation_name_collision(args, before, phase, library, layout, *, verify_busy_reads):
    verify_creation_rejection(args, before, phase, library, -6, verify_busy_reads=verify_busy_reads)
    require(
        home_string(reader(phase / "state"), layout["reservation"]["name"].encode("ascii"))
        == b"existing user data",
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
        args, before, root, library, layout, transfer=transfer, verify_busy_reads=verify_busy_reads
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

    require_absent(reader(unsafe / "state"), layout["reservation"]["name"].encode("ascii"))
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
        home_string(reader(restart / "state"), layout["reservation"]["name"].encode("ascii"))
        == home_string(reader(stored / "state"), layout["reservation"]["name"].encode("ascii")),
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
    require_absent(reader(restart / "state"), layout["reservation"]["name"].encode("ascii"))
    require(
        verify_busy_reads(args, restart) == 0,
        "Closed installation wrote Flash after reset",
    )
    print(
        "Real installation cancellation, resource release and claim closure survive restart",
        flush=True,
    )
    return {"slot": slot, "operations": operations}


def run_creation_registration(
    args, before, root, library, layout, *, transfer, emulator, completed, verify_busy_reads
):
    """Register the inert claim-bound root before creating transaction resources."""
    from reservation import require_absent
    from store import restart_storage, verify_storage

    verify_source(before, library)
    root = root / "creation-registration"
    root.mkdir()
    stored = root / "stored"
    image = "installation-template-creation-root-store"
    transfer(
        args,
        stored,
        before,
        args.build,
        "ROOTSTORE",
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
    )
    verify_source(stored / "state", library)
    require_absent(reader(stored / "state"), b"ROOTSTORE")
    require_absent(reader(stored / "state"), layout["reservation"]["name"].encode("ascii"))
    require(
        verify_busy_reads(args, stored) == operations, "Root registration bypassed Flash polling"
    )
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
    require_absent(reader(restart / "state"), layout["reservation"]["name"].encode("ascii"))
    require(verify_busy_reads(args, restart) == 0, "Inert root published a transaction after reset")
    print(
        "Claim-bound root registered before resource creation or transaction publication",
        flush=True,
    )
    return {"slot": slot, "operations": operations}


def run_registered_reuse(args, before, root, library, layout, *, transfer, verify_busy_reads):
    from installation import verify_probe
    from reservation import require_absent

    image = "installation-template-creation-registered-store"
    rejected = root / "registered-reuse-rejected"
    transfer(
        args, rejected, before, args.build, "REGRETRY", package_name=image + ".hp", consume=True
    )
    base = verify_probe(
        before, rejected / "state", args.build, args.nm, -4, 0xFFFFFFFF, image=image
    )
    symbols = symbol_offsets(args.build / (image + ".elf"), args.nm)
    ram = (rejected / "state/sram").read_bytes()
    for name, words in (
        ("storage_result", 2),
        ("storage_stage_result", 6),
        ("storage_cleanup_result", 6),
    ):
        require(
            struct.unpack_from(f"<{words}I", ram, base + symbols[name]) == (0,) * words,
            "Closed installation entered fresh transaction writes",
        )
    require(verify_busy_reads(args, rejected) == 0, "Closed installation reuse programmed Flash")
    require_absent(reader(rejected / "state"), layout["reservation"]["name"].encode("ascii"))
    verify_source(rejected / "state", library)
    print(
        "Closed registered installation rejected as a fresh transaction without writes", flush=True
    )


def run_registered_creation(
    args, before, root, library, layout, slot, *, transfer, emulator, completed, verify_busy_reads
):
    from reservation import require_absent
    from store import restart_storage, verify_storage

    verify_source(before, library)
    root = root / "registered-creation"
    root.mkdir()
    image = "installation-template-creation-registered-store"
    stored = root / "stored"
    transfer(
        args,
        stored,
        before,
        args.build,
        "REGCREATE",
        package_name=image + ".hp",
        consume=True,
        execution_ms=60000,
    )
    matched, rebound, operations = verify_storage(
        before,
        stored / "state",
        args.build,
        args.nm,
        make_fixture=partial(fixture, library, layout),
        image=image,
        staged=True,
        closed=True,
        existing_slot=slot,
    )
    require(matched == slot, "Fresh transaction replaced its registered root")
    verify_source(stored / "state", library)
    require_absent(reader(stored / "state"), b"REGCREATE")
    require(
        verify_busy_reads(args, stored) == operations, "Registered creation bypassed Flash polling"
    )
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
    require_absent(reader(restart / "state"), layout["reservation"]["name"].encode("ascii"))
    require(verify_busy_reads(args, restart) == 0, "Closed registered creation wrote after reset")
    run_registered_reuse(
        args,
        restart / "state",
        root,
        library,
        layout,
        transfer=transfer,
        verify_busy_reads=verify_busy_reads,
    )
    print(
        "Registered root admits fresh creation and preserves complete cancellation across restart",
        flush=True,
    )
    return {"slot": slot, "operations": operations}


def run_activation_cuts(
    args,
    before,
    reference,
    root,
    library,
    layout,
    journal,
    authority,
    operations,
    *,
    transfer,
    emulator,
    completed,
    verify_busy_reads,
):
    """Cut the actual activation write before/after its sole changing bit lands."""
    import re
    import shutil
    from reservation import require_absent
    from verify import verify_flash, verify_restart

    expected = (reference / "flash").read_bytes()
    name = layout["reservation"]["name"].encode("ascii")
    resource = home_string(reader(reference), name)
    address = authority & ~1
    value = int.from_bytes(expected[address : address + 2], "little")
    bit = 1 << (8 * (authority & 1))
    require(expected[authority] == 0x7E, "Reference transaction is not armed")
    results = {}
    for committed in (False, True):
        label = "activation-cut-" + ("committed" if committed else "uncommitted")
        cut = root / label
        mask = bit if committed else 0
        transfer(
            args,
            cut,
            before,
            args.build,
            "REGCUT",
            consume=True,
            package_name="installation-template-creation-active-store.hp",
            power_cut=(operations, 1, mask),
        )
        events = re.findall(
            r"^hpclang-flash-cut operation=(\d+) address=(\d+) bytes=(\d+) "
            r"value=(\d+) mask=(\d+) reads=(\d+)$",
            (cut / "emulator.log").read_text(),
            re.M,
        )
        require(
            len(events) == 1
            and tuple(map(int, events[0])) == (operations, address, 2, value, mask, 1),
            "Power cut missed the native activation write",
        )
        torn = bytearray(expected)
        if not committed:
            torn[authority] = 0x7F
        require(
            (cut / "state/flash").read_bytes() == torn,
            "Activation cut changed unrelated Flash or native-created records",
        )
        require(
            home_string(reader(cut / "state"), name) == resource,
            "Activation cut lost its native-created resource",
        )
        previous = cut / "state"
        count = 0
        for repeat in range(2):
            phase = root / f"{label}-restart-{repeat}"
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
            observed = verify_busy_reads(args, phase)
            if committed and not repeat:
                verify_flash(
                    bytes(torn),
                    (state / "flash").read_bytes(),
                    journal,
                    0x140000,
                    retire=True,
                    authority_slot=authority,
                )
                require(observed > 0, "Committed activation did not retire its transaction")
                count = observed
            else:
                require(
                    (state / "flash").read_bytes() == (previous / "flash").read_bytes(),
                    "Unarmed or already recovered activation changed Flash",
                )
                require(observed == 0, "Unarmed or recovered transaction wrote on reset")
            verify_restart(before, state, state)
            verify_source(state, library)
            if committed:
                require_absent(reader(state), name)
            else:
                require(
                    home_string(reader(state), name) == resource,
                    "Uncommitted activation released its resource",
                )
            previous = state
        results[label] = count
    print(
        "Activation power cuts distinguish committed authority without adopting unarmed resources",
        flush=True,
    )
    return results


def run_activation_ram_loss(
    args, before, fault, root, layout, journal, authority, *, emulator, completed, verify_busy_reads
):
    """Recover the native-created Flash transaction with no installation RAM."""
    import shutil
    from prepare import clear_ram_images
    from reservation import require_absent
    from verify import require_stock_pc, verify_flash, verify_restart

    previous = fault
    operations = 0
    for repeat in range(2):
        phase = root / f"activation-ram-loss-{repeat}"
        shutil.copytree(previous, phase / "state")
        sequence = "10000 capture home\n12000 finish-install\n"
        if not repeat:
            clear_ram_images(phase / "state")
            # Decline Stock's RAM recovery prompt: the Flash installation alone
            # must supply its code, identity, claim and authoritative capsule.
            sequence = "20000 tap f6\n40000 tap f6\n60000 capture home\n62000 finish-install\n"
        with emulator(
            args, phase, sequence, ["--reset"], flash_busy=True, reset_checkpoint=True
        ) as child:
            completed(child, 90)
        state = phase / "state"
        require_stock_pc(state)
        observed = verify_busy_reads(args, phase)
        if not repeat:
            verify_flash(
                (fault / "flash").read_bytes(),
                (state / "flash").read_bytes(),
                journal,
                0x140000,
                retire=True,
                authority_slot=authority,
            )
            require(observed > 0, "RAM-loss recovery did not restore its transaction")
            operations = observed
        else:
            require(
                (state / "flash").read_bytes() == (previous / "flash").read_bytes(),
                "RAM-loss recovery changed Flash on its second reset",
            )
            require(observed == 0, "RAM-loss recovery repeated Flash writes")
        verify_restart(before, state, state)
        require_absent(reader(state), layout["reservation"]["name"].encode("ascii"))
        require_absent(reader(state), b"HPCTEMPLATE")
        previous = state
    print("Native-created recovery survives complete loss of installation RAM", flush=True)
    return operations


def run_registered_activation(
    args, before, root, library, layout, slot, *, transfer, emulator, completed, verify_busy_reads
):
    """Arm a freshly created real transaction, then interrupt its target sector.

    Only target damage is injected: root, journal, authority, claim and RAM
    reservation must all come from the native installer. This isolated reset
    fixture does not prove ordering before arbitrary user startup libraries.
    """
    import shutil
    from reservation import require_absent
    from store import verify_storage
    from verify import verify_flash, verify_restart, verify_retired

    root = root / "registered-activation"
    root.mkdir()
    stored = root / "stored"
    image = "installation-template-creation-active-store"
    transfer(
        args, stored, before, args.build, "REGACTIVE", package_name=image + ".hp", consume=True
    )
    _, rebound, operations = verify_storage(
        before,
        stored / "state",
        args.build,
        args.nm,
        make_fixture=partial(fixture, library, layout),
        image=image,
        staged=True,
        existing_slot=slot,
        active=True,
    )
    require(verify_busy_reads(args, stored) == operations, "Activation bypassed Flash polling")
    require_absent(reader(stored / "state"), b"REGACTIVE")
    verify_source(stored / "state", library)
    obj = nibbles(rebound[8:])
    at = layout["recovery_binding_nibble"] - 8
    journal = int(obj[at + 16 : at + 24][::-1], 16)
    authority = int(obj[at + 24 : at + 32][::-1], 16)
    target = 0x140000
    fault = root / "fault"
    shutil.copytree(stored / "state", fault)
    flash = bytearray((fault / "flash").read_bytes())
    require(flash[target : target + 4096] != b"\xff" * 4096, "Target is already erased")
    flash[target : target + 4096] = b"\xff" * 4096
    (fault / "flash").write_bytes(flash)

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

    repaired = boot(fault, "recovered")
    verify_flash(
        bytes(flash),
        (repaired / "flash").read_bytes(),
        journal,
        target,
        retire=True,
        authority_slot=authority,
    )
    verify_restart(before, repaired, repaired)
    recovery_operations = verify_busy_reads(args, root / "recovered")
    require(recovery_operations > 0, "Activated recovery did not use the Flash driver")
    repeated = boot(repaired, "repeated")
    verify_restart(before, repaired, repeated)
    verify_retired(
        (repaired / "flash").read_bytes(),
        (repeated / "flash").read_bytes(),
        journal,
        target,
        authority_slot=authority,
    )
    require(verify_busy_reads(args, root / "repeated") == 0, "Closed recovery repeated writes")
    for state in (repaired, repeated):
        verify_source(state, library)
        require_absent(reader(state), layout["reservation"]["name"].encode("ascii"))
    print("Native-created registered transaction recovered through its installed guard", flush=True)
    cuts = run_activation_cuts(
        args,
        before,
        stored / "state",
        root,
        library,
        layout,
        journal,
        authority,
        operations,
        transfer=transfer,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )
    ram_loss = run_activation_ram_loss(
        args,
        before,
        fault,
        root,
        layout,
        journal,
        authority,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )
    return {
        "operations": operations,
        "recovery_operations": recovery_operations,
        "cuts": cuts,
        "ram_loss_operations": ram_loss,
    }


def run_registration(args, clean, root, *, transfer, emulator, completed, verify_busy_reads):
    claim_before, claim_library, claim_layout = run_creation_template(
        args, clean, root, transfer=transfer, verify_busy_reads=verify_busy_reads
    )
    registration = run_creation_registration(
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
    registered_creation = run_registered_creation(
        args,
        root / "creation-registration/restart/state",
        root,
        claim_library,
        claim_layout,
        registration["slot"],
        transfer=transfer,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )
    activation = run_registered_activation(
        args,
        root / "creation-registration/restart/state",
        root,
        claim_library,
        claim_layout,
        registration["slot"],
        transfer=transfer,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )
    return {
        "registration": registration,
        "registered_creation": registered_creation,
        "activation": activation,
    }


def run_creation(args, clean, root, *, transfer, emulator, completed, verify_busy_reads):
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
    return {
        "creation_storage": claim_storage,
        "creation_cleanup": claim_cleanup,
    }


def run_template(args, clean, root, *, transfer, emulator, completed, verify_busy_reads):
    from installation import verify_placement, verify_preparation, verify_probe
    from startup import verify_startup
    from store import restart_storage, verify_storage

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
        # This one-shot installer owns a large template/runtime copy. Release
        # its HOME binding on entry so it cannot consume recovery-time memory
        # after returning. The independent HPCTEMPLATE source remains intact.
        consume=True,
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
    from reservation import require_absent

    require_absent(reader(stored / "state"), b"REALSTORE")
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
