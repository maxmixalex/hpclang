"""Generate and verify the independent, read-only native installation witness."""

import argparse
import json
from pathlib import Path
import shutil
import struct
from types import SimpleNamespace

from reservation import packager, reader
from verify import installed_recovery, nibbles, require, require_stock_pc, symbol_offsets


LIBRARY_ID = 1703


def fixture():
    pack = packager()
    # Empty configuration: installable through Stock without an application callback.
    library = pack._configuration_library(
        pack.immediate(0x2D9D) + pack.immediate(0x312B), LIBRARY_ID
    )
    obj = nibbles(library[8:])
    span = 5 + int(obj[5:10][::-1], 16)
    allocated = (span + 15) & ~7
    body = obj[:span] + "f" * (allocated - span - 8)
    record = b"\x53" + ((allocated - 2) | 0x100000).to_bytes(3, "little")
    record += bytes(int(body[i], 16) | int(body[i + 1], 16) << 4 for i in range(0, len(body), 2))
    return library, record


def generate_fixture(output):
    output.mkdir(parents=True, exist_ok=True)
    library, record = fixture()
    (output / "expected-library.hp").write_bytes(library)
    (output / "expected-library.h").write_text(
        "/* Generated native fixture; not a production installation root. */\n"
        "static const unsigned char expected_library[] = {\n    "
        + ", ".join(str(byte) for byte in record)
        + "\n};\n"
    )


def verify_execution(ram, initialized, symbols, package, result, slot):
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
    require(values[1:3] == (result & 0xFFFFFFFF, slot), "Unexpected native installation result")
    require(values[3:5] == values[5:7], "Installation witness did not restore Flash mappings")
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
    prefix = program[: end - reserve]
    packed_ram = nibbles(ram)
    owners = [
        i
        for i in range(len(packed_ram))
        if packed_ram.startswith(prefix, i)
        and i + len(prefix) <= base * 2
        and (base + symbols["__image_end"]) * 2 <= i + end
    ]
    require(len(owners) == 1, "Installation execution is not inside one owning CODE object")


def verify_probe(before, after, build, nm, result, slot):
    require_stock_pc(after)
    require(
        (before / "flash").read_bytes() == (after / "flash").read_bytes(),
        "Read-only installation witness changed Flash",
    )
    read, original = reader(after), reader(before)
    require(original(0x806FD) - original(0x806F8) == 5, "Expected an empty baseline stack")
    require(read(0x806FD) - read(0x806F8) == 10, "Expected exactly one predicate result")
    pointer = read(read(0x806F8))
    require(
        read(pointer) == 0x2933 and pointer == (0x2F94C if result == 0 else 0x2F937),
        "Installation predicate returned the wrong real-valued result",
    )
    count = read(0x8611D, 3)
    require(0 < count < 128 and count == original(0x8611D, 3), "Installation changed catalog count")
    require(
        all(read(0x86120 + i * 16, 16) == original(0x86120 + i * 16, 16) for i in range(count)),
        "Read-only installation witness changed the catalog",
    )
    verify_execution(
        (after / "sram").read_bytes(),
        (build / "installation.bin").read_bytes(),
        symbol_offsets(build / "installation.elf", nm),
        (build / "installation.hp").read_bytes(),
        result,
        slot,
    )


def run_installation(args, clean, *, transfer, emulator, completed, verify_busy_reads):
    root = args.output / "installation"
    root.mkdir()
    build = args.build
    setup = SimpleNamespace(**(vars(args) | {"flash_busy_reads": 0}))
    missing = root / "missing"
    transfer(args, missing, clean, build, "INSTEST", package_name="installation.hp")
    verify_probe(clean, missing / "state", build, args.nm, -8, 0xFFFFFFFF)
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
        None,
        read=reader(before),
    )
    matched = root / "matched"
    transfer(args, matched, before, build, "INSTEST", package_name="installation.hp")
    verify_probe(before, matched / "state", build, args.nm, 0, slot)
    require(verify_busy_reads(args, matched) == 0, "Installed-library lookup wrote Flash")
    (args.output / "result.json").write_text(
        json.dumps(
            dict(status="pass", model=args.model, suite="installation", slot=slot, bytes=size),
            indent=2,
        )
        + "\n"
    )
    print(
        "PASS: native installation content/catalog/bank matching without Flash writes", flush=True
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate the native installation fixture")
    parser.add_argument("output", type=Path)
    generate_fixture(parser.parse_args().output)
