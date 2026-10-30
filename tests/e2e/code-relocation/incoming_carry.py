"""Compare suspended-carry root discovery with the original Saturn interpreter.

Controlled RAM instructions and saved CPU state; not a full launcher/Stock gate.
"""

import argparse
import ctypes as c
import hashlib
import importlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

if not __debug__:
    raise RuntimeError("Assertions required")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
source = root / "platform/armtoolbox/stage-stack.c"
args.output.mkdir(parents=True, exist_ok=True)
report = args.output / "incoming-carry.json"
report.unlink(missing_ok=True)
sources = [source, source.with_suffix(".h"), Path(__file__)]
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
subprocess.run(
    [
        "cc",
        "-shared",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        str(source),
        "-o",
        str(args.output / "stage.so"),
    ],
    check=True,
)
lib = c.CDLL(str(args.output / "stage.so"))
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint, c.POINTER(c.c_uint32))


class IO(c.Structure):
    _fields_ = [("context", c.c_void_p), ("read", Read), ("write", c.c_void_p)]


Pointer = c.POINTER(c.c_uint32)
lib.hpclang_stage_return_roots_with_carry.argtypes = [
    c.POINTER(IO),
    c.c_uint32,
    Pointer,
    c.c_uint32,
    c.c_int,
    c.POINTER(Pointer),
]
lib.hpclang_stage_return_roots_with_carry.restype = c.c_uint
flash = (args.state / "flash").read_bytes()
ram = (args.state / "sram").read_bytes()
assert len(flash) == 0x200000 and len(ram) == 0x80000
mapping = native_words(ram, flash)
cpu = 0x08000000 + (mapping.table - 128) * 4
rows = []
for cursor in range(8):
    for carry in (0, 1):
        for condition in (0, 1):
            for displacement in (0, 4, -4):
                uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
                uc.mem_map(0, len(flash))
                uc.mem_write(0, flash)
                uc.mem_map(0x08000000, len(ram))
                uc.mem_write(0x08000000, ram)
                uc.mem_map(0x300000, 0x10000)

                def put(at, value, width):
                    for i in range(width):
                        p = mapping.physical(at + i)
                        shift = 4 * ((at + i) & 1)
                        old = uc.mem_read(p, 1)[0]
                        uc.mem_write(
                            p,
                            bytes([(old & ~(15 << shift)) | (((value >> (4 * i)) & 15) << shift)]),
                        )

                @Read
                def read(_, at, width, value):
                    if at < 0x80000 or at + width > 0x100000:
                        return 0
                    value[0] = sum(
                        ((uc.mem_read(mapping.physical(at + i), 1)[0] >> (4 * ((at + i) & 1))) & 15)
                        << (4 * i)
                        for i in range(width)
                    )
                    return 1

                put(0x88000, 0xFFFFF, 5)
                put(0x89000, 0xFFFFF, 5)
                put(0x88000, ((displacement & 255) << 4) | (4 if condition else 5), 3)
                if displacement:
                    put(0x88001 + displacement, 0x10, 2)
                stack = (c.c_uint32 * 8)(*[0x89000 + 16 * i for i in range(8)])
                stack[(cursor - 1) & 7] = 0x89000
                before = bytes(stack)
                uc.mem_write(cpu + 0x978, before)
                uc.mem_write(cpu + 0x998, struct.pack("<I", cursor))
                uc.mem_write(cpu + 0x970, struct.pack("<I", carry))
                uc.mem_write(cpu + 0xC24, bytes(4))
                saved = int.from_bytes(uc.mem_read(cpu + 0x970, 4), "little")
                roots = (Pointer * 8)()
                io = IO(None, read, None)
                count = lib.hpclang_stage_return_roots_with_carry(
                    c.byref(io), 0x88000, stack, cursor, saved, roots
                )
                taken = carry == condition
                assert count == int(taken) and bytes(stack) == before
                if count:
                    assert c.addressof(roots[0].contents) == c.addressof(stack) + 4 * (
                        (cursor - 1) & 7
                    )
                trace = []

                def hook(_, at, size, userdata):
                    if at == 0x28898:
                        pc = uc.reg_read(UC_ARM_REG_R7)
                        trace.append(pc)
                        if pc in (0x88003, 0x89000) or len(trace) > 5:
                            uc.emu_stop()

                uc.hook_add(UC_HOOK_CODE, hook)
                uc.reg_write(UC_ARM_REG_R4, cpu)
                uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
                uc.reg_write(UC_ARM_REG_R7, 0x88000)
                uc.reg_write(UC_ARM_REG_SP, 0x308000)
                uc.emu_start(0x28828, 0x400000, count=20000)
                expected = [0x88000]
                if taken and displacement:
                    expected.append(0x88001 + displacement)
                expected.append(0x89000 if taken else 0x88003)
                assert trace == expected, (
                    cursor,
                    carry,
                    condition,
                    displacement,
                    trace,
                )
                actual_cursor = int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little")
                assert actual_cursor == ((cursor - int(taken)) & 7)
                after = bytes(uc.mem_read(cpu + 0x978, 32))
                wanted = bytearray(before)
                if taken:
                    slot = ((cursor - 1) & 7) * 4
                    wanted[slot : slot + 4] = bytes(4)
                assert after == wanted
                assert bytes(uc.mem_read(0, len(flash))) == flash
                rows.append(
                    dict(
                        cursor=cursor,
                        carry=carry,
                        condition=condition,
                        displacement=displacement,
                        roots=count,
                        trace=trace,
                    )
                )
assert hashes == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            cases=rows,
            source_sha256=hashes,
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
        ),
        indent=2,
    )
    + "\n"
)
print(args.model, len(rows), "original-interpreter incoming-carry cases PASS")
