"""Compare native bank lookup/geometry with original ROM switch execution."""

import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

from flashptr_guards import verify_flashptr
from bank_overlay import verify_attachment_switches, verify_overlay
from alias_guards import verify_alias_selectors
from bank_map_model import verify_bank_map
from rom_profile import verify_rom_profile

if not __debug__:
    raise RuntimeError("Assertions required")
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
from verify import native_words  # noqa: E402

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
report = args.output / "bank-switch-maps.json"
report.unlink(missing_ok=True)
base = root / "runtime/core"
files = [
    base / name
    for name in (
        "rpl-port.c",
        "rpl-backup.c",
        "rpl-object-span.c",
        "rpl-reference.c",
        "rpl-object-store.c",
        "rpl-code.c",
        "rpl-stock-memory.c",
        "rpl-bank-effects.c",
        "rpl-bank-plan.c",
        "rpl-rom-profile.c",
    )
]
sources = {
    str(p): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [
        *files,
        base / "rpl-port.h",
        base / "rpl-reference.h",
        base / "rpl-stock-memory.h",
        base / "rpl-bank-effects.h",
        base / "rpl-bank-plan.h",
        base / "rpl-rom-profile.h",
        Path(__file__),
        Path(__file__).with_name("flashptr_guards.py"),
        Path(__file__).with_name("bank_overlay.py"),
        Path(__file__).with_name("alias_guards.py"),
        Path(__file__).with_name("bank_map_model.py"),
        Path(__file__).with_name("rom_profile.py"),
    ]
}
shared = args.output / "bank-switch-maps.so"
stub = args.output / "stock-host-stub.c"
stub.write_text(
    "#include <hpclang/bootstrap.h>\nconst struct hpclang_boot_info *hpclang_boot_info(void) { return 0; }\n"
)
subprocess.run(
    [
        args.cc,
        "-shared",
        "-fPIC",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I" + str(base),
        "-I" + str(root / "include"),
        *map(str, files),
        str(stub),
        "-o",
        str(shared),
    ],
    check=True,
)
lib = c.CDLL(str(shared.resolve()))
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))
Dependency = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint8)
MappedRead = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint32, c.POINTER(c.c_uint8))


class Guard(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("mapping", "field", "expected", "width")]


class GuardBuilder(c.Structure):
    _fields_ = [
        ("guards", c.POINTER(Guard)),
        ("capacity", c.c_uint32),
        ("count", c.c_uint32),
        ("error", c.c_int),
    ]


class GuardReader(c.Structure):
    _fields_ = [("read", MappedRead), ("context", c.c_void_p)]


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


class Bank(c.Structure):
    _fields_ = [("kind", c.c_int), ("index", c.c_uint32)]


class Geometry(c.Structure):
    _fields_ = [("physical", c.c_uint32), ("size", c.c_uint32)]


class BankedTarget(c.Structure):
    _fields_ = [("bank", Bank), ("offset", c.c_uint32)]


lib.hpclang_rpl_acptr_resolve_guarded.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.POINTER(BankedTarget),
    Dependency,
    c.c_void_p,
    Dependency,
    c.c_void_p,
]
lib.hpclang_rpl_bank_lookup.argtypes = [c.POINTER(Reader), *([c.c_uint32] * 3), c.POINTER(Bank)]
lib.hpclang_rpl_bank_lookup_guarded.argtypes = [
    *lib.hpclang_rpl_bank_lookup.argtypes,
    Dependency,
    c.c_void_p,
]
lib.hpclang_rpl_reference_guards_begin.argtypes = [
    c.POINTER(GuardBuilder),
    c.POINTER(Guard),
    c.c_uint32,
]
lib.hpclang_rpl_reference_guards_observe.argtypes = [
    c.POINTER(GuardBuilder),
    c.c_uint32,
    c.c_uint32,
    c.c_uint8,
]
lib.hpclang_rpl_reference_guards_validate.argtypes = [
    c.POINTER(GuardReader),
    c.POINTER(Guard),
    c.c_uint32,
]
lib.hpclang_rpl_rom_bank_layout.argtypes = [
    c.POINTER(c.c_uint8),
    *([c.c_uint32] * 3),
    c.POINTER(c.c_uint32),
    c.c_uint32,
    c.POINTER(Geometry),
]
ram, flash = (args.state / "sram").read_bytes(), (args.state / "flash").read_bytes()
assert len(ram) == 0x80000 and len(flash) == 0x200000
read = native_words(ram, flash)
cpu = 0x08000000 + (read.table - 128) * 4
# Entry identity only selects the expected bank under the reviewed P=0
# calling convention. Reject a different saved context instead of silently
# treating its switch result as evidence for that convention.
incoming_p = struct.unpack_from("<I", ram, cpu - 0x08000000 + 0x95C)[0]
assert incoming_p == 0, "ROM bank-switch fixture requires incoming P=0"
bases = (c.c_uint32 * 16).from_buffer_copy(ram, cpu - 0x08000000 + 0xBE0)
count_address, directory = struct.unpack_from("<2I", flash, 0x248A0)
assert count_address < len(flash) and directory == count_address + 1
count = flash[count_address]
assert 0 < count <= 16 and directory + count <= len(flash)
sizes = (c.c_uint8 * count).from_buffer_copy(flash, directory)
corrupt = None
source_corrupt = None
source_nibbles = []


@Read
def source_nibble(_, address, out):
    # Deliberately overlap the table's numeric address in a different mapping.
    offset = address - 0x26BBB
    if not 0 <= offset < len(source_nibbles):
        return 0
    out[0] = source_nibbles[offset] ^ (1 if address == source_corrupt else 0)
    return 1


@Read
def nibble(_, address, out):
    out[0] = read(address, 1) ^ (1 if address == corrupt else 0)
    return 1


@MappedRead
def mapped_nibble(_, mapping, address, out):
    if mapping == 2:
        return source_nibble(None, address, out)
    if mapping != 1:
        return 0
    return nibble(None, address, out)


guard_reader = GuardReader(mapped_nibble, None)
reader = Reader(nibble, None)
rows = []
for index in range(17):
    switch = read(0x26BBB + index * 7 if index < 16 else 0x26C39)
    binding = Bank(-1, 99)
    assert (
        lib.hpclang_rpl_bank_lookup(c.byref(reader), 0x26BBB, 0x26C39, switch, c.byref(binding))
        == 1
    )
    assert (binding.kind, binding.index) == ((0, index) if index < 16 else (1, 0))
    guards = (Guard * 17)()
    builder = GuardBuilder()
    assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 17) == 0
    observed = []

    @Dependency
    def observe(_, address, value):
        observed.append(address)
        return lib.hpclang_rpl_reference_guards_observe(c.byref(builder), 1, address, value)

    guarded = Bank(-1, 99)
    assert (
        lib.hpclang_rpl_bank_lookup_guarded(
            c.byref(reader),
            0x26BBB,
            0x26C39,
            switch,
            c.byref(guarded),
            observe,
            None,
        )
        == 1
    )
    assert (guarded.kind, guarded.index) == (binding.kind, binding.index)
    expected = {0x26BBB + i * 7 + j for i in range(16) for j in range(5)}
    expected.update(range(0x26C39, 0x26C39 + 5))
    assert len(observed) == 85 and set(observed) == expected and builder.count == 17
    assert (
        lib.hpclang_rpl_reference_guards_validate(c.byref(guard_reader), guards, builder.count) == 0
    )
    for corrupt in observed:
        assert (
            lib.hpclang_rpl_reference_guards_validate(
                c.byref(guard_reader),
                guards,
                builder.count,
            )
            < 0
        )
    corrupt = None
    short_builder = GuardBuilder()
    assert lib.hpclang_rpl_reference_guards_begin(c.byref(short_builder), guards, 16) == 0

    @Dependency
    def short_observe(_, address, value):
        return lib.hpclang_rpl_reference_guards_observe(c.byref(short_builder), 1, address, value)

    unavailable = Bank(-1, 99)
    assert (
        lib.hpclang_rpl_bank_lookup_guarded(
            c.byref(reader), 0x26BBB, 0x26C39, switch, c.byref(unavailable), short_observe, None
        )
        < 0
    )
    assert short_builder.error < 0 and short_builder.count == 0
    assert (unavailable.kind, unavailable.index) == (-1, 99)
    source_nibbles = [
        (word >> (4 * j)) & 15 for word in (0x02BAA, 0x7327E, switch) for j in range(5)
    ]
    source_reader = Reader(source_nibble, None)
    combined = (Guard * 20)()
    combined_builder = GuardBuilder()
    assert lib.hpclang_rpl_reference_guards_begin(c.byref(combined_builder), combined, 20) == 0
    source_observed, table_observed = [], []

    @Dependency
    def observe_source(_, address, value):
        source_observed.append(address)
        return lib.hpclang_rpl_reference_guards_observe(
            c.byref(combined_builder), 2, address, value
        )

    @Dependency
    def observe_table(_, address, value):
        table_observed.append(address)
        return lib.hpclang_rpl_reference_guards_observe(
            c.byref(combined_builder), 1, address, value
        )

    resolved = BankedTarget(Bank(-1, 99), 0xDEAD)
    assert (
        lib.hpclang_rpl_acptr_resolve_guarded(
            c.byref(source_reader),
            0x26BBB,
            15,
            c.byref(reader),
            0x26BBB,
            0x26C39,
            c.byref(resolved),
            observe_source,
            None,
            observe_table,
            None,
        )
        == 1
    )
    assert (resolved.bank.kind, resolved.bank.index) == (binding.kind, binding.index)
    assert resolved.offset == 0x3327E and combined_builder.count == 20
    assert source_observed == list(range(0x26BBB, 0x26BBB + 15))
    assert table_observed == observed
    assert (
        lib.hpclang_rpl_reference_guards_validate(
            c.byref(guard_reader), combined, combined_builder.count
        )
        == 0
    )
    for source_corrupt in source_observed:
        assert (
            lib.hpclang_rpl_reference_guards_validate(
                c.byref(guard_reader), combined, combined_builder.count
            )
            < 0
        )
    source_corrupt = None
    for corrupt in table_observed:
        assert (
            lib.hpclang_rpl_reference_guards_validate(
                c.byref(guard_reader), combined, combined_builder.count
            )
            < 0
        )
    corrupt = None
    geometry = Geometry(0xDEAD, 0xBEEF)
    if index < 16:
        populated = lib.hpclang_rpl_rom_bank_layout(
            sizes, count, directory, len(flash), bases, index, c.byref(geometry)
        )
        assert populated == (1 if index < count else 0)
        if not populated:
            assert (geometry.physical, geometry.size) == (0xDEAD, 0xBEEF)
    else:
        populated = 1
        geometry = Geometry(0x08020000, 0x20000)
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    for at, data in ((0, flash), (0x08000000, ram)):
        uc.mem_map(at, len(data))
        uc.mem_write(at, data)
    uc.mem_map(0x300000, 0x10000)
    uc.mem_write(cpu + 0x998, struct.pack("<I", 1))
    uc.mem_write(cpu + 0x978, struct.pack("<I", 0x3130))
    for reg, value in (
        (UC_ARM_REG_R4, cpu),
        (UC_ARM_REG_R5, 0xFFFFF),
        (UC_ARM_REG_R7, switch),
        (UC_ARM_REG_SP, 0x308000),
    ):
        uc.reg_write(reg, value)
    returned = []

    def stop(_, pc, size, user):
        if pc == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
            returned.append(True)
            uc.emu_stop()

    uc.hook_add(UC_HOOK_CODE, stop)
    uc.emu_start(0x28828, 0x400000, count=100000)
    assert returned
    pages = struct.unpack("<64I", uc.mem_read(cpu + 0x40 * 4, 64 * 4))
    if populated:
        assert pages == tuple(geometry.physical + i * 0x800 for i in range(64))
        assert 0 < geometry.size <= 0x20000
    else:
        # An unused table entry still has a callable switch. It installs a
        # repeated dummy page, not a usable ROM bank or a physical zero base.
        assert len(set(pages)) == 1 and pages[0] != 0
    assert bytes(uc.mem_read(cpu, 0x40 * 4)) == ram[cpu - 0x08000000 : cpu - 0x08000000 + 0x100]
    assert (
        bytes(uc.mem_read(cpu + 0x80 * 4, 0x80 * 4))
        == ram[cpu - 0x08000000 + 0x200 : cpu - 0x08000000 + 0x400]
    )
    assert bytes(uc.mem_read(0, len(flash))) == flash
    rows.append(
        dict(
            index=index,
            switch=switch,
            populated=bool(populated),
            physical=geometry.physical if populated else None,
            valid_bytes=geometry.size if populated else 0,
            pages=list(pages),
            switch_guards=builder.count,
            rejected_stale_nibbles=len(observed),
            insufficient_guard_capacity_rejected=True,
            controlled_acptr_guards=combined_builder.count,
            controlled_acptr_stale_rejections=len(source_observed) + len(table_observed),
        )
    )
flashptr_rows = verify_flashptr(lib, read, flash, rows)
overlay_rows = verify_overlay(lib, ram, flash, cpu, rows)
attachment_rows = verify_attachment_switches(lib, read, ram, flash, cpu, rows)
profile_row = verify_rom_profile(lib, flash, args.model)
alias_selector_rows = verify_alias_selectors(
    lib, read, ram, flash, cpu, rows, profile_row["profile"]
)
full_map_rows = verify_bank_map(lib, ram, flash, cpu, rows)
assert all(
    hashlib.sha256(Path(p).read_bytes()).hexdigest() == value for p, value in sources.items()
)
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            sources=sources,
            cases=rows,
            flashptr_tables=flashptr_rows,
            overlay_sequences=overlay_rows,
            attachment_switch_sequences=attachment_rows,
            alias_selector_guards=alias_selector_rows,
            full_bank_read_maps=full_map_rows,
            rom_profile=profile_row,
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
            incoming_p=incoming_p,
            scope="Reviewed P=0 ROM switches/read mappings; no complete reference graph or mutation authorization",
        ),
        indent=2,
    )
    + "\n"
)
print(
    "PASS",
    args.model,
    len(rows),
    "ROM switches and",
    len(overlay_rows),
    "ERAM overlay sequences and",
    len(attachment_rows),
    "attachment-switch sequences; populated geometry and unused-bank rejection",
)
