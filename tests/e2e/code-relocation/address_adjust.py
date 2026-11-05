"""Compare D0/D1 immediate arithmetic with the original Saturn interpreter.

Synthetic instructions prove semantics, not launcher reachability or pointer
provenance. Wrapped results do not establish mapping continuity.
"""

import argparse
import ctypes as c
import hashlib
import importlib
import itertools
import json
from pathlib import Path
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

if not __debug__:
    raise RuntimeError("Verification requires assertions")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output
out.mkdir(parents=True, exist_ok=True)
report = out / "address-adjust.json"
report.unlink(missing_ok=True)
base = root / "runtime/core"
files = [
    base / name
    for name in (
        "rpl-code.c",
        "rpl-reference.c",
        "rpl-backup.c",
        "rpl-port.c",
        "rpl-object-store.c",
        "rpl-object-span.c",
    )
]
tracked = files + [base / "rpl-code.h", Path(__file__)]
hashes = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
subprocess.run(
    [
        args.cc,
        "-shared",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        *map(str, files),
        "-o",
        str(out / "code.so"),
    ],
    check=True,
)
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


class Adjustment(c.Structure):
    _fields_ = [
        ("instruction", c.c_uint32),
        ("length", c.c_uint32),
        ("address", c.c_uint32),
        ("delta", c.c_int32),
    ]


lib = c.CDLL(str(out / "code.so"))
inspect = lib.hpclang_rpl_code_address_adjust_inspect
inspect.argtypes = [c.POINTER(Reader), c.c_uint32, c.c_uint32, c.POINTER(Adjustment)]
flash = (args.state / "flash").read_bytes()
ram = (args.state / "sram").read_bytes()
mapping = native_words(ram, flash)
cpu = 0x08000000 + (mapping.table - 128) * 4
rows = []
starts = (0, 1, 15, 16, 0xFFFF0, 0xFFFFE, 0xFFFFF, 0x81234)
address = 0x90001
for opcode, immediate, initial, carry in itertools.product(
    (6, 7, 8, 12), range(16), starts, (0, 1)
):
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(0, len(flash))
    uc.mem_write(0, flash)
    uc.mem_map(0x08000000, len(ram))
    uc.mem_write(0x08000000, ram)
    uc.mem_map(0x300000, 0x10000)
    for i, nibble in enumerate((1, opcode, immediate)):
        at = address + i
        physical, shift = mapping.physical(at), 4 * (at & 1)
        old = uc.mem_read(physical, 1)[0]
        uc.mem_write(physical, bytes([(old & ~(15 << shift)) | (nibble << shift)]))
    for i in range(9):
        uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0xABCDEF1234567890 + i))
    target = 0x954 if opcode in (6, 8) else 0x958
    for offset, value in (
        (0x954, 0xA1234),
        (0x958, 0xB5678),
        (target, initial),
        (0x95C, immediate),
        (0x970, carry),
        (0x998, immediate % 8),
        (0xC24, 0),
    ):
        uc.mem_write(cpu + offset, struct.pack("<I", value))
    before = bytes(uc.mem_read(cpu + 0x90C, 0x99C - 0x90C))

    @Read
    def read(_, at, value):
        if not address <= at < address + 3:
            return 0
        value[0] = uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1)) & 15
        return 1

    reader, adjustment = Reader(read, None), Adjustment()
    assert inspect(c.byref(reader), address, 3, c.byref(adjustment)) == 0
    assert adjustment.instruction == address and adjustment.length == 3
    assert adjustment.address == (11 if opcode in (6, 8) else 12)
    delta = (immediate + 1) * (1 if opcode in (6, 7) else -1)
    assert adjustment.delta == delta
    value = (initial + adjustment.delta) & 0xFFFFF
    expected = bytearray(before)
    struct.pack_into("<I", expected, target - 0x90C, value)
    struct.pack_into("<I", expected, 0x970 - 0x90C, int(not 0 <= initial + delta <= 0xFFFFF))
    for register, value in (
        (UC_ARM_REG_R4, cpu),
        (UC_ARM_REG_R5, struct.unpack_from("<I", flash, 0x296F8)[0]),
        (UC_ARM_REG_R7, address),
        (UC_ARM_REG_SP, 0x308000),
    ):
        uc.reg_write(register, value)
    trace = []

    def hook(uc, at, size, unused):
        trace.append(uc.reg_read(UC_ARM_REG_R7))
        if len(trace) == 2:
            uc.emu_stop()

    uc.hook_add(UC_HOOK_CODE, hook, begin=0x28898, end=0x28898)
    uc.emu_start(0x28828, 0x400000, count=10000)
    assert trace == [address, address + 3]
    assert bytes(uc.mem_read(cpu + 0x90C, len(before))) == expected, (
        opcode,
        immediate,
        initial,
        carry,
    )
    rows.append(
        dict(
            opcode=opcode,
            immediate=immediate,
            initial=initial,
            carry=carry,
            result=(initial + delta) & 0xFFFFF,
            wrapped=not 0 <= initial + delta <= 0xFFFFF,
        )
    )
assert hashes == {
    str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked
}
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            cases=rows,
            sources=hashes,
            inputs={
                "flash": hashlib.sha256(flash).hexdigest(),
                "sram": hashlib.sha256(ram).hexdigest(),
            },
        ),
        indent=2,
    )
    + "\n"
)
print(args.model, "PASS", len(rows), "address adjustments", flush=True)
