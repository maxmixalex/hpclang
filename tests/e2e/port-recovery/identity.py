"""Execute sealed admission from its installed library, including low memory."""

import json
import shutil
from types import SimpleNamespace

from installation import (
    AUTHORITY_SLOT,
    IDENTITY_LIBRARY_ID,
    JOURNAL_SLOT,
    TARGET,
    verify_probe,
)
from prepare import prepare
from reservation import home_string, packager, reader, verify_zero_real_array
from verify import installed_recovery, nibbles, require, symbol_offsets


IMAGE = "installation-identity"
USER_ELEMENTS = 24000
RESERVE_BYTES = 32768


def package_identity(build, output, nm):
    """Use owned ELF coordinates, without searching for an identity signature."""
    output.mkdir()
    pack = packager()
    symbols = symbol_offsets(build / (IMAGE + ".elf"), nm)
    initialized = (build / (IMAGE + ".bin")).read_bytes()
    program = pack.package(initialized, symbols["__image_end"], model="hp50g", fresh_copy=True)
    empty = pack.immediate(0x2D9D) + pack.immediate(0x312B)
    library = pack._configuration_library(
        empty, IDENTITY_LIBRARY_ID, (empty,) * 6 + (pack._recovery_program(program),)
    )
    offset = symbols["hpclang_recovery_identity"]
    layout = pack.recovery_layout(
        library,
        (("admission", 6, initialized, symbols["hpclang_recovery_binding"], True),),
        identity_offset=offset,
    )
    position = layout["identity_object_nibble"]
    library = pack.seal_recovery_identity(library, position)
    data = nibbles(library[8:])[position : position + 96]
    descriptor = bytes(int(data[i : i + 2][::-1], 16) for i in range(0, len(data), 2))
    initialized = initialized[:offset] + descriptor + initialized[offset + 48 :]
    (output / (IMAGE + ".bin")).write_bytes(initialized)
    (output / (IMAGE + ".hp")).write_bytes(
        pack.package(initialized, symbols["__image_end"], model="hp50g", fresh_copy=True)
    )
    shutil.copyfile(build / (IMAGE + ".elf"), output / (IMAGE + ".elf"))
    (output / "identity-library.hp").write_bytes(library)
    invoke = (
        empty[:5]
        + pack.immediate(0x2E92)
        + pack.immediate(IDENTITY_LIBRARY_ID, 3)
        + pack.immediate(6, 3)
        + empty[5:]
    )
    invoke += "0" * (len(invoke) % 2)
    (output / "invoke.hp").write_bytes(
        b"HPHP49-C" + bytes(int(invoke[i : i + 2][::-1], 16) for i in range(0, len(invoke), 2))
    )
    return library


def run_identity(args, clean, *, transfer, emulator, completed, verify_busy_reads):
    root = args.output / "identity"
    root.mkdir()
    build = root / "package"
    library = package_identity(args.build, build, args.nm)
    setup = SimpleNamespace(**(vars(args) | {"flash_busy_reads": 0}))
    installed = root / "installed"
    transfer(
        setup,
        installed,
        clean,
        build,
        "IDLIB",
        execute=False,
        package_name="identity-library.hp",
        remote_commands=["-95 CF HOME CLEAR", "'IDLIB' RCL 2 STO 'IDLIB' PURGE"],
    )
    baseline = root / "baseline"
    baseline.mkdir()
    shutil.copytree(installed / "state", baseline / "state")
    with emulator(
        setup, baseline, "10000 capture home\n12000 finish-install\n", ["--reset"]
    ) as child:
        completed(child, 90)
    before = baseline / "state"
    slot, _ = installed_recovery(
        (before / "flash").read_bytes(),
        library,
        IDENTITY_LIBRARY_ID,
        TARGET,
        read=reader(before),
    )

    def probe(source, label, result, *, pressure=False):
        phase = root / label
        transfer(
            args,
            phase,
            source,
            build,
            "IDCHECK",
            package_name="invoke.hp",
            remote_commands=["-95 CF HOME CLEAR 'IDCHECK' RCL 'IDCHECK' PURGE"],
        )
        verify_probe(
            source,
            phase / "state",
            build,
            args.nm,
            result,
            slot if result >= 0 else 0xFFFFFFFF,
            image=IMAGE,
            fresh_copy=True,
        )
        require(verify_busy_reads(args, phase) == 0, "Sealed admission wrote Flash")
        if pressure:
            read = reader(phase / "state")
            verify_zero_real_array(read, b"HPCFILL", USER_ELEMENTS)
            require(
                home_string(read, b"HPCRES") == b"x" * RESERVE_BYTES,
                "Sealed admission changed the reserved RAM",
            )
        print(f"Sealed library admission verified: {label}", flush=True)

    def transaction(source, label):
        seed = root / (label + "-input")
        prepare(source, seed, JOURNAL_SLOT, TARGET, bound=True)
        flash = bytearray((seed / "flash").read_bytes())
        if "damaged" not in label:
            flash[TARGET : TARGET + 4096] = (source / "flash").read_bytes()[TARGET : TARGET + 4096]
        if label == "retiring":
            flash[JOURNAL_SLOT] = 0x7C
        elif "closed" in label:
            flash[JOURNAL_SLOT] = flash[AUTHORITY_SLOT] = 0
        elif label == "unpublished":
            flash[AUTHORITY_SLOT] = 0xFF
        (seed / "flash").write_bytes(flash)
        return seed

    for label, result in (
        ("armed", 1),
        ("damaged", 1),
        ("retiring", 2),
        ("closed", 0),
        ("unpublished", -4),
    ):
        probe(transaction(before, label), label, result)

    changed = root / "changed-library"
    shutil.copytree(before, changed)
    values = [int(nibble, 16) for nibble in nibbles(library[8:])]
    span = 5 + sum(values[5 + i] << (4 * i) for i in range(5))
    values[12] ^= 1  # Title only: keep ID, command code and allocation geometry.
    crc = 0
    for value in values[5 : span - 4]:
        for bit in range(4):
            carry = (crc ^ (value >> bit)) & 1
            crc >>= 1
            if carry:
                crc ^= 0x8408
    values[span - 4 : span] = [(crc >> (4 * i)) & 15 for i in range(4)]
    altered = bytes(values[i] | values[i + 1] << 4 for i in range(0, len(values), 2))
    flash = bytearray((changed / "flash").read_bytes())
    flash[slot + 4 : slot + 4 + len(altered)] = altered
    (changed / "flash").write_bytes(flash)
    installed_recovery(
        flash, b"HPHP49-C" + altered, IDENTITY_LIBRARY_ID, TARGET, read=reader(changed)
    )
    probe(transaction(changed, "changed"), "changed", -4)

    filled = root / "filled"
    transfer(
        setup,
        filled,
        before,
        build,
        "UNUSED",
        installed=True,
        execute=True,
        remote_commands=[
            "-95 CF HOME CLEAR",
            "\"x\" 1 15 START DUP + NEXT 'HPCRES' STO",
            f"{{ {USER_ELEMENTS} }} 0. CON 'HPCFILL' STO",
        ],
    )
    pressure = filled / "state"
    read = reader(pressure)
    verify_zero_real_array(read, b"HPCFILL", USER_ELEMENTS)
    require(
        home_string(read, b"HPCRES") == b"x" * RESERVE_BYTES,
        "Low-memory fixture lacks its reservation",
    )
    gap = (read(0x806F8) - read(0x806F3)) // 2
    # Keep the fixture below the combined admission plus full journal buffers.
    copied = len((build / (IMAGE + ".hp")).read_bytes()) - 8
    require(0 < gap < copied + 2 * 4124, "Sealed admission fixture lacks memory pressure")
    for label, result in (("pressure-damaged", 1), ("pressure-closed", 0)):
        probe(transaction(pressure, label), label, result, pressure=True)
    (args.output / "result.json").write_text(
        json.dumps(
            dict(
                status="pass",
                model=args.model,
                suite="identity",
                slot=slot,
                free_bytes=gap,
                program_bytes=copied,
                sealed_identity=True,
                modified_content_rejected=True,
                low_memory=True,
            ),
            indent=2,
        )
        + "\n"
    )
