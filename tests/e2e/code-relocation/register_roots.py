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
stack_transfer_cases = []
for cursor in range(8):
    for pop in (False, True):
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        uc.mem_map(0, len(flash))
        uc.mem_write(0, flash)
        uc.mem_map(0x08000000, len(ram))
        uc.mem_write(0x08000000, ram)
        uc.mem_map(0x300000, 0x10000)
        sequence = [
            (0x90000, 0x70 if pop else 0x60, 2),
            (0x90002, 0x3B18 if pop else 0x10, 4 if pop else 2),
        ]
        for address, value, width in sequence:
            for i in range(width):
                at = address + i
                physical = mapping.physical(at)
                shift = 4 * (at & 1)
                old = uc.mem_read(physical, 1)[0]
                uc.mem_write(
                    physical, bytes([(old & ~(15 << shift)) | (((value >> (4 * i)) & 15) << shift)])
                )
        for i in range(9):
            uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0xABCDEF1234591000 + i))
        cells = [0x93000 + i * 16 for i in range(8)]
        cells[(cursor - 1) & 7] = 0xABC92000
        uc.mem_write(cpu + 0x978, struct.pack("<8I", *cells))
        for offset, value in ((0x95C, 13), (0x970, 1), (0x998, cursor), (0xC24, 0)):
            uc.mem_write(cpu + offset, struct.pack("<I", value))
        before = bytes(uc.mem_read(cpu + 0x90C, 0x99C - 0x90C))
        expected = bytearray(before)

        @Read
        def transfer_read(_, at, width, value):
            for address, encoded, length in sequence:
                if address <= at and at + width <= address + length:
                    value[0] = (encoded >> (4 * (at - address))) & ((1 << (4 * width)) - 1)
                    return 1
            return 0

        transfer_reader = Reader(None, transfer_read, None)
        av = c.c_uint32(struct.unpack_from("<I", before, 0)[0])
        cv = c.c_uint32(struct.unpack_from("<I", before, 16)[0])
        stack = (c.c_uint32 * 8)(*cells)
        roots = (c.POINTER(c.c_uint32) * 10)()
        masks = (c.c_uint32 * 10)()
        count = inspect(
            c.byref(transfer_reader),
            0x90000,
            stack,
            cursor,
            1,
            c.byref(av),
            c.byref(cv),
            roots,
            masks,
        )
        expected_cell = c.addressof(stack) + 4 * ((cursor - 1) & 7) if pop else c.addressof(cv)
        assert (
            count == 1 and c.addressof(roots[0].contents) == expected_cell and masks[0] == 0xFFFFF
        )
        assert all(not roots[i] and not masks[i] for i in range(1, 10))
        assert list(stack) == cells
        assert (
            av.value == struct.unpack_from("<I", before, 0)[0]
            and cv.value == struct.unpack_from("<I", before, 16)[0]
        )
        if arm:

            def transfer_arm_read(at, width):
                value = c.c_uint32()
                return value.value if transfer_read(None, at, width, c.pointer(value)) else None

            actual_roots = arm.inspect(
                0x90000, bytes(stack), cursor, 1, av.value, cv.value, transfer_arm_read
            )
            assert actual_roots == [(f"stack-{(cursor - 1) & 7}" if pop else "c", 0xFFFFF)]

        if pop:
            old = struct.unpack_from("<Q", expected, 16)[0]
            struct.pack_into("<Q", expected, 16, (old & ~0xFFFFF) | cells[(cursor - 1) & 7])
            struct.pack_into("<I", expected, 0x978 - 0x90C + 4 * ((cursor - 1) & 7), 0)
            struct.pack_into("<I", expected, 0x998 - 0x90C, (cursor - 1) & 7)
            target = cells[(cursor - 1) & 7] & 0xFFFFF
        else:
            struct.pack_into("<I", expected, 0x978 - 0x90C + 4 * cursor, 0)
            target = struct.unpack_from("<I", before, 16)[0] & 0xFFFFF
        for reg, value in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, struct.unpack_from("<I", flash, 0x296F8)[0]),
            (UC_ARM_REG_R7, 0x90000),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(reg, value)
        trace = []

        def hook(uc, at, size, unused):
            trace.append(uc.reg_read(UC_ARM_REG_R7))
            if len(trace) == 3:
                uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, hook, begin=0x28898, end=0x28898)
        uc.emu_start(0x28828, 0x400000, count=10000)
        assert trace == [0x90000, 0x90002, target], (args.model, cursor, pop, trace, target)
        actual = bytes(uc.mem_read(cpu + 0x90C, len(before)))
        assert actual == expected, (
            args.model,
            cursor,
            pop,
            [(i, a, b) for i, (a, b) in enumerate(zip(actual, expected)) if a != b],
        )
        stack_transfer_cases.append(dict(cursor=cursor, pop=pop, trace=trace))

capacity_cases = []
if arm:
    for cursor in range(8):
        cells = [0x93000 + i * 16 for i in range(8)]
        instructions = {
            0x90000: (0x2B18, 4),
            0x91000: (0x3B18, 4),
            0x92000: (0x10, 2),
            **{at: (0x10, 2) for at in cells},
        }

        def capacity_read(at, width):
            if at not in instructions or width > instructions[at][1]:
                return None
            return instructions[at][0] & ((1 << (4 * width)) - 1)

        actual = arm.inspect(
            0x90000, struct.pack("<8I", *cells), cursor, -1, 0xABC91000, 0xDEF92000, capacity_read
        )
        expected_roots = [("a", 0xFFFFF), ("c", 0xFFFFF)]
        expected_roots += [(f"stack-{(cursor - 1 - i) % 8}", 0xFFFFFFFF) for i in range(8)]
        assert actual == expected_roots
        capacity_cases.append(dict(cursor=cursor, roots=actual))
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            cases=rows,
            stack_transfer_cases=stack_transfer_cases,
            linked_arm_capacity_cases=capacity_cases,
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
            scope="Register instructions and two-instruction stack transfers compared to ROM; no complete execution-root coverage or relocation authorization",
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
