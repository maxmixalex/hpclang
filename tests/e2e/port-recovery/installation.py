"""Generate and verify native installation and transaction witnesses."""

import argparse
import json
from pathlib import Path
import shutil
import struct
from types import SimpleNamespace

from prepare import prepare
from reservation import packager, reader
from verify import (
    installed_recovery,
    nibbles,
    require,
    require_stock_pc,
    symbol_offsets,
)


LIBRARY_ID = 1703
IDENTITY_LIBRARY_ID = 1704
JOURNAL_SLOT = 0x160105
AUTHORITY_SLOT = JOURNAL_SLOT + 4124
TARGET = 0x140000


def fixture():
    pack = packager()
    # Empty configuration: installable through Stock without an application callback.
    # Two owned PIC stubs (BX LR), never invoked by the empty configuration.
    # Their descriptors exercise the same command-layout path as real recovery.
    image = pack.bind_image(
        bytes.fromhex("1eff2fe1") + bytes(12) + b"HPCBIND1" + bytes(8) + b"HPCROOT1" + bytes(40),
        16,
        JOURNAL_SLOT,
        AUTHORITY_SLOT,
    )
    program = pack._recovery_program(pack.package(image, len(image), model="hp50g"))
    empty = pack.immediate(0x2D9D) + pack.immediate(0x312B)
    library = pack._configuration_library(empty, LIBRARY_ID, (program,) + (empty,) * 5 + (program,))
    layout = pack.recovery_layout(
        library,
        (
            ("recovery", 0, image, 16, False),
            ("admission", 6, image, 16, False),
        ),
        identity_offset=32,
    )
    library = pack.seal_recovery_identity(library, layout["identity_object_nibble"])
    obj = nibbles(library[8:])
    span = 5 + int(obj[5:10][::-1], 16)
    allocated = (span + 15) & ~7
    body = obj[:span] + "f" * (allocated - span - 8)
    record = b"\x53" + ((allocated - 2) | 0x100000).to_bytes(3, "little")
    record += bytes(int(body[i], 16) | int(body[i + 1], 16) << 4 for i in range(0, len(body), 2))
    return library, record, layout


def fixture_identity():
    library, _, layout = fixture()
    obj = nibbles(library[8:])
    at = layout["identity_object_nibble"]
    return bytes(int(obj[i : i + 2][::-1], 16) for i in range(at, at + 96, 2))


def generate_fixture(output):
    output.mkdir(parents=True, exist_ok=True)
    library, record, layout = fixture()
    (output / "expected-library.hp").write_bytes(library)
    (output / "expected-library.h").write_text(
        "/* Generated native fixture; not a production installation root. */\n"
        "static const unsigned char expected_library[] = {\n    "
        + ", ".join(str(byte) for byte in record)
        + "\n};\n"
        + "static const unsigned char expected_identity[] = {"
        + ", ".join(str(byte) for byte in fixture_identity())
        + "};\n"
        + f"#define EXPECTED_RECOVERY_BINDING {layout['recovery_binding_nibble']}u\n"
        + f"#define EXPECTED_ADMISSION_BINDING {layout['admission_binding_nibble']}u\n"
        + f"#define EXPECTED_JOURNAL_SLOT {JOURNAL_SLOT}u\n"
        + f"#define EXPECTED_AUTHORITY_SLOT {AUTHORITY_SLOT}u\n"
        + f"#define EXPECTED_TARGET {TARGET}u\n"
        + f"#define EXPECTED_IDENTITY_LIBRARY {IDENTITY_LIBRARY_ID}u\n"
    )


def verify_execution(ram, initialized, symbols, package, result, slot, *, fresh_copy=False):
    require(len(ram) == 0x80000, "Expected complete hp4950 SRAM")
    matches = []
    position = 0
    while (position := ram.find(struct.pack("<I", 0x48504944), position)) >= 0:
        base = position - symbols["installation_result"]
        if (
            base >= 0
            and base + symbols["__image_end"] <= len(ram)
            and ram[base : base + len(initialized)] == initialized
        ):
            matches.append((base, struct.unpack_from("<8I", ram, position)))
        position += 1
    require(len(matches) == 1, "Installation execution image is missing or ambiguous")
    base, values = matches[0]
    require(
        values[1:3] == (result & 0xFFFFFFFF, slot),
        "Unexpected native installation result",
    )
    require(
        values[3:5] == values[5:7],
        "Installation witness did not restore Flash mappings",
    )
    verify_owned_image(ram, initialized, symbols, package, base, fresh_copy=fresh_copy)
    return base


def verify_owned_image(ram, initialized, symbols, package, base, *, fresh_copy=False):
    require(
        len(ram) == 0x80000 and 0 <= base <= len(ram) - symbols["__image_end"],
        "Execution image leaves the captured RAM",
    )
    require(
        ram[base : base + len(initialized)] == initialized,
        "Initialized execution image differs",
    )
    for name in ("admission_stack_low", "admission_stack_high"):
        require(
            struct.unpack_from("<2I", ram, base + symbols[name]) == (0x51A7B09D,) * 2,
            "Installation witness stack guard corrupted",
        )
    require(package[:8] == b"HPHP49-C", "Invalid installation package")
    program = nibbles(package[8:])
    require(len(program) >= 20, "Incomplete installation program")
    end = 10 + int(program[10:15][::-1], 16)
    reserve = symbols["__image_end"] * 2 + 15
    require(reserve + 15 < end <= len(program), "Invalid installation CODE extent")
    prefix = (
        program[: 15 + len(packager().entry((len(initialized) + 7) & ~7, 0, fresh_copy=True))]
        if fresh_copy
        else program[: end - reserve]
    )
    packed_ram = nibbles(ram)
    owners = [
        i
        for i in range(len(packed_ram))
        if packed_ram.startswith(prefix, i)
        and i + len(prefix) <= base * 2
        and (not fresh_copy or base * 2 <= i + len(prefix) + 15)
        and (base + symbols["__image_end"]) * 2 <= i + end
    ]
    require(len(owners) == 1, "Installation execution is not inside one owning CODE object")


def verify_probe(
    before,
    after,
    build,
    nm,
    result,
    slot,
    *,
    image="installation",
    expected_flash=None,
    fresh_copy=False,
):
    require_stock_pc(after)
    require(
        ((before / "flash").read_bytes() if expected_flash is None else expected_flash)
        == (after / "flash").read_bytes(),
        "Installation witness changed unexpected Flash bytes",
    )
    read, original = reader(after), reader(before)
    require(original(0x806FD) - original(0x806F8) == 5, "Expected an empty baseline stack")
    require(read(0x806FD) - read(0x806F8) == 10, "Expected exactly one predicate result")
    pointer = read(read(0x806F8))
    require(
        read(pointer) == 0x2933 and pointer == (0x2F94C if result >= 0 else 0x2F937),
        "Installation predicate returned the wrong real-valued result",
    )
    count = read(0x8611D, 3)
    require(
        0 < count < 128 and count == original(0x8611D, 3),
        "Installation changed catalog count",
    )
    require(
        all(read(0x86120 + i * 16, 16) == original(0x86120 + i * 16, 16) for i in range(count)),
        "Read-only installation witness changed the catalog",
    )
    ram = (after / "sram").read_bytes()
    symbols = symbol_offsets(build / (image + ".elf"), nm)
    base = verify_execution(
        ram,
        (build / (image + ".bin")).read_bytes(),
        symbols,
        (build / (image + ".hp")).read_bytes(),
        result,
        slot,
        fresh_copy=fresh_copy,
    )
    if image in ("installation-transaction", "installation-identity"):
        require(
            struct.unpack_from("<3I", ram, base + symbols["transaction_pair"])
            == ((JOURNAL_SLOT, AUTHORITY_SLOT, TARGET) if result >= 0 else (0, 0, 0)),
            "Installed transaction coordinates differ from the owned fixture",
        )
    return base


def run_installation(args, clean, *, transfer, emulator, completed, verify_busy_reads):
    from startup import verify_startup

    root = args.output / "installation"
    root.mkdir()
    build = args.build
    setup = SimpleNamespace(**(vars(args) | {"flash_busy_reads": 0}))
    missing = root / "missing"
    transfer(args, missing, clean, build, "INSTEST", package_name="installation.hp")
    base = verify_probe(clean, missing / "state", build, args.nm, -8, 0xFFFFFFFF)
    startup_missing = verify_startup(clean, missing / "state", build, args.nm, base)
    require(verify_busy_reads(args, missing) == 0, "Missing-library lookup wrote Flash")
    print("Native installation lookup rejected the absent library", flush=True)
    installed = root / "installed"
    transfer(
        setup,
        installed,
        clean,
        build,
        "TESTLIB",
        execute=False,
        package_name="expected-library.hp",
        remote_commands=["-95 CF HOME CLEAR", "'TESTLIB' RCL 2 STO 'TESTLIB' PURGE"],
    )
    baseline = root / "baseline"
    baseline.mkdir()
    shutil.copytree(installed / "state", baseline / "state")
    with emulator(
        setup, baseline, "10000 capture home\n12000 finish-install\n", ["--reset"]
    ) as child:
        completed(child, 90)
    before = baseline / "state"
    require_stock_pc(before)
    slot, size = installed_recovery(
        (before / "flash").read_bytes(),
        (build / "expected-library.hp").read_bytes(),
        LIBRARY_ID,
        TARGET,
        read=reader(before),
    )
    matched = root / "matched"
    transfer(args, matched, before, build, "INSTEST", package_name="installation.hp")
    base = verify_probe(before, matched / "state", build, args.nm, 0, slot)
    startup_matched = verify_startup(before, matched / "state", build, args.nm, base)
    require(verify_busy_reads(args, matched) == 0, "Installed-library lookup wrote Flash")
    transactions = []
    for label, status in (
        ("armed", 1),
        ("damaged", 1),
        ("retiring", 2),
        ("closed", 0),
        ("unpublished", -4),
    ):
        source = root / (label + "-input")
        prepare(before, source, JOURNAL_SLOT, TARGET, bound=True)
        flash = bytearray((source / "flash").read_bytes())
        if label != "damaged":
            flash[TARGET : TARGET + 4096] = (before / "flash").read_bytes()[TARGET : TARGET + 4096]
        if label == "retiring":
            flash[JOURNAL_SLOT] = 0x7C
        elif label == "closed":
            flash[JOURNAL_SLOT] = flash[AUTHORITY_SLOT] = 0
        elif label == "unpublished":
            flash[AUTHORITY_SLOT] = 0xFF
        (source / "flash").write_bytes(flash)
        phase = root / label
        transfer(
            args,
            phase,
            source,
            build,
            "INSTEST",
            package_name="installation-transaction.hp",
            consume=True,
        )
        verify_probe(
            source,
            phase / "state",
            build,
            args.nm,
            status,
            slot if status >= 0 else 0xFFFFFFFF,
            image="installation-transaction",
        )
        require(
            verify_busy_reads(args, phase) == 0,
            "Installed transaction preflight wrote Flash",
        )
        transactions.append(label)
        print(f"Installed transaction preflight verified: {label}", flush=True)
    from publication import run_publication

    run_publication(
        args,
        before,
        slot,
        transfer=transfer,
        emulator=emulator,
        completed=completed,
        verify_busy_reads=verify_busy_reads,
    )
    (args.output / "result.json").write_text(
        json.dumps(
            dict(
                status="pass",
                model=args.model,
                suite="installation",
                slot=slot,
                bytes=size,
                transactions=transactions,
                startup_inventory={"missing": startup_missing, "matched": startup_matched},
                reservation_creation=["existing-different", "existing-identical", "lost-write-ack"],
                publication=True,
                cancellation=True,
            ),
            indent=2,
        )
        + "\n"
    )
    print(
        "PASS: native installation, transaction preflight, publication and cancellation",
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate the native installation fixture")
    parser.add_argument("output", type=Path)
    generate_fixture(parser.parse_args().output)
