"""Compare launcher register-use classification with original interpreter execution.

Synthetic instructions in private Stock snapshots prove instruction semantics,
not launcher reachability or permission to relocate a captured address.
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
    raise RuntimeError("Verification requires assertions")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
parser.add_argument("--stage1-elf", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output
out.mkdir(parents=True, exist_ok=True)
report = out / "register-roots.json"
report.unlink(missing_ok=True)
base = root / "runtime/core"
files = [root / "platform/armtoolbox/stage-stack.c"]
subprocess.run(
    [
        args.cc,
        "-shared",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I" + str(root / "platform/armtoolbox"),
        *map(str, files),
        "-o",
        str(out / "code.so"),
    ],
    check=True,
)
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint, c.POINTER(c.c_uint32))


class Reader(c.Structure):
    _fields_ = [("context", c.c_void_p), ("read", Read), ("write", c.c_void_p)]


lib = c.CDLL(str(out / "code.so"))
inspect = lib.hpclang_stage_execution_roots
inspect.restype = c.c_uint
inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.POINTER(c.c_uint32),
    c.c_uint32,
    c.c_int,
    c.POINTER(c.c_uint32),
    c.POINTER(c.c_uint32),
    c.POINTER(c.POINTER(c.c_uint32)),
    c.POINTER(c.c_uint32),
]
flash = (args.state / "flash").read_bytes()
ram = (args.state / "sram").read_bytes()
mapping = native_words(ram, flash)
cpu = 0x08000000 + (mapping.table - 128) * 4
rows = []
arm = None
if args.stage1_elf:
    from arm_stage_roots import ArmStageRoots

    arm = ArmStageRoots(args.stage1_elf)
for address in (0x90000, 0x90001, 0xFFFE0):
    for opcode in range(2, 8):
        for p in range(16):
            uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
            uc.mem_map(0, len(flash))
            uc.mem_write(0, flash)
            uc.mem_map(0x08000000, len(ram))
            uc.mem_write(0x08000000, ram)
            uc.mem_map(0x300000, 0x10000)
            for i, nibble in enumerate((8, 1, 11, opcode)):
                at = address + i
                physical, shift = mapping.physical(at), 4 * (at & 1)
                old = uc.mem_read(physical, 1)[0]
                uc.mem_write(physical, bytes([(old & ~(15 << shift)) | (nibble << shift)]))
            for i in range(9):
                uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0xABCDEF1234567890 + i))
            for offset, value in ((0x95C, p), (0x970, p & 1), (0x998, p % 8), (0xC24, 0)):
                uc.mem_write(cpu + offset, struct.pack("<I", value))
            before = bytes(uc.mem_read(cpu + 0x90C, 0x99C - 0x90C))

            @Read
            def read(_, at, width, value):
                if at < address or at + width > address + 4:
                    return 0
                value[0] = sum(
                    ((uc.mem_read(mapping.physical(at + i), 1)[0] >> (4 * ((at + i) & 1))) & 15)
                    << (4 * i)
                    for i in range(width)
                )
                return 1

            reader = Reader(None, read, None)
            a = c.c_uint32(struct.unpack_from("<I", before, 0)[0])
            cv = c.c_uint32(struct.unpack_from("<I", before, 16)[0])
            stack = (c.c_uint32 * 8).from_buffer_copy(before[0x978 - 0x90C : 0x998 - 0x90C])
            saved_stack = bytes(stack)
            old_a, old_c = a.value, cv.value
            roots = (c.POINTER(c.c_uint32) * 10)()
            masks = (c.c_uint32 * 10)()
            count = inspect(
                c.byref(reader), address, stack, p % 8, p & 1, c.byref(a), c.byref(cv), roots, masks
            )
            selected = cv if opcode & 1 else a
            jumps = opcode < 4 or opcode >= 6
            target = (selected.value & 0xFFFFF) if jumps else address + 4
            assert count == int(jumps)
            if jumps:
                assert c.addressof(roots[0].contents) == c.addressof(selected)
                assert masks[0] == 0xFFFFF
            assert all(not roots[i] and not masks[i] for i in range(count, 10))
            assert (a.value, cv.value, bytes(stack)) == (old_a, old_c, saved_stack)
            if arm:

                def arm_read(at, width):
                    result = c.c_uint32()
                    return result.value if read(None, at, width, c.pointer(result)) else None

                actual_roots = arm.inspect(
                    address, saved_stack, p % 8, p & 1, old_a, old_c, arm_read
                )
                assert actual_roots == ([("c" if opcode & 1 else "a", 0xFFFFF)] if jumps else [])
            expected = bytearray(before)
            if opcode >= 4:
                offset = 16 if opcode & 1 else 0
                old = struct.unpack_from("<Q", expected, offset)[0]
                struct.pack_into("<Q", expected, offset, (old & ~0xFFFFF) | (address + 4))
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
            assert trace == [address, target]
            assert bytes(uc.mem_read(cpu + 0x90C, len(before))) == expected
            rows.append(dict(address=address, opcode=opcode, p=p, carry=p & 1, cursor=p % 8))
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            cases=rows,
            sources={
                str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in files + [root / "platform/armtoolbox/stage-stack.h"]
            },
            driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            arm_stage_sha256=hashlib.sha256(args.stage1_elf.read_bytes()).hexdigest()
            if arm
            else None,
            arm_driver_sha256=hashlib.sha256(
                Path(__file__).with_name("arm_stage_roots.py").read_bytes()
            ).hexdigest()
            if arm
            else None,
            scope="One instruction compared to ROM; no complete execution-root coverage or relocation authorization",
        ),
        indent=2,
    )
    + "\n"
)
print(
    "PASS",
    args.model,
    len(rows),
    "register-consumption cases; unrelated registers/flags/stack preserved",
)
