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
files = [base / name for name in ("rpl-port.c", "rpl-backup.c", "rpl-object-span.c")]
sources = {
    str(p): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [*files, base / "rpl-port.h", Path(__file__)]
}
shared = args.output / "bank-switch-maps.so"
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
        "-o",
        str(shared),
    ],
    check=True,
)
lib = c.CDLL(str(shared.resolve()))
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


class Bank(c.Structure):
    _fields_ = [("kind", c.c_int), ("index", c.c_uint32)]


class Geometry(c.Structure):
    _fields_ = [("physical", c.c_uint32), ("size", c.c_uint32)]


lib.hpclang_rpl_bank_lookup.argtypes = [c.POINTER(Reader), *([c.c_uint32] * 3), c.POINTER(Bank)]
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
bases = (c.c_uint32 * 16).from_buffer_copy(ram, cpu - 0x08000000 + 0xBE0)
count_address, directory = struct.unpack_from("<2I", flash, 0x248A0)
assert count_address < len(flash) and directory == count_address + 1
count = flash[count_address]
assert 0 < count <= 16 and directory + count <= len(flash)
sizes = (c.c_uint8 * count).from_buffer_copy(flash, directory)


@Read
def nibble(_, address, out):
    out[0] = read(address, 1)
    return 1


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
        )
    )
assert all(
    hashlib.sha256(Path(p).read_bytes()).hexdigest() == value for p, value in sources.items()
)
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            sources=sources,
            cases=rows,
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
            scope="Reviewed ROM switches and read mappings only; no complete reference graph or mutation authorization",
        ),
        indent=2,
    )
    + "\n"
)
print("PASS", args.model, len(rows), "ROM switches; populated geometry and unused-bank rejection")
