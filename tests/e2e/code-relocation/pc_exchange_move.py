"""Controlled catalog movement around original APCEX/CPCEX execution.

The pinned fixture supplies code and register provenance. This is not automatic
root discovery, public registration, or proof that arbitrary opaque code moves.
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
out = args.output
out.mkdir(parents=True, exist_ok=True)
report = out / "pc-exchange-move.json"
report.unlink(missing_ok=True)
base = root / "runtime/core"
sources = [
    base / n
    for n in (
        "rpl-mutation.c",
        "rpl-object-store.c",
        "rpl-stack-view.c",
        "object-snapshot.c",
        "rpl-object-span.c",
    )
]
sources += [
    root / "platform/armtoolbox/stage-stack.c",
    Path(__file__).with_name("threaded-return.c"),
]
sources.append(base / "rpl-code.c")
tracked = [
    *sources,
    Path(__file__),
    base / "rpl-mutation.h",
    base / "rpl-code.h",
    root / "platform/armtoolbox/stage-stack.h",
]
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
subprocess.run(
    [
        "cc",
        "-shared",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I" + str(base),
        "-I" + str(root / "include"),
        "-I" + str(root / "platform/armtoolbox"),
        *map(str, sources),
        "-o",
        str(out / "geometry.so"),
    ],
    check=True,
)
lib = c.CDLL(str(out / "geometry.so"))
lib.bank_catalog_grow_dispatch.argtypes = [
    c.POINTER(c.c_uint8),
    c.POINTER(c.c_uint32),
    c.POINTER(c.c_uint32),
    c.c_uint,
    c.POINTER(c.c_uint32),
]
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


ReadWord = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint, c.POINTER(c.c_uint32))


class StackIO(c.Structure):
    _fields_ = [("context", c.c_void_p), ("read", ReadWord), ("write", c.c_void_p)]


lib.hpclang_stage_execution_roots.argtypes = [
    c.POINTER(StackIO),
    c.c_uint32,
    c.POINTER(c.c_uint32),
    c.c_uint32,
    c.c_int,
    c.POINTER(c.c_uint32),
    c.POINTER(c.c_uint32),
    c.POINTER(c.POINTER(c.c_uint32)),
    c.POINTER(c.c_uint32),
]
lib.hpclang_stage_execution_roots.restype = c.c_uint


class Guard(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("mapping", "field", "expected", "width")]


lib.bank_catalog_grow_dispatch_guarded.argtypes = [
    *lib.bank_catalog_grow_dispatch.argtypes,
    c.c_void_p,
    c.POINTER(Guard),
    c.c_uint32,
]


class Assignment(c.Structure):
    _fields_ = [
        (name, c.c_uint32)
        for name in (
            "instruction",
            "length",
            "source",
            "destination",
            "source_first",
            "destination_first",
            "nibbles",
            "constant",
        )
    ]


class Exchange(c.Structure):
    _fields_ = [("capture", Assignment), ("target", c.c_uint32)]


lib.hpclang_rpl_code_pc_exchange_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint32,
    c.c_uint32,
    c.POINTER(Exchange),
]
flash = (args.state / "flash").read_bytes()
ram = (args.state / "sram").read_bytes()
assert len(flash) == 0x200000 and len(ram) == 0x80000
mapping = native_words(ram, flash)
cpu = 0x08000000 + (mapping.table - 128) * 4
entry, target = 0x88B48, 0x88985
assert mapping(entry, 2) == 0x30 and mapping(target, 2) == 0x95
rows = []
rejections = []
for register in (0, 2):
    for phase in ("before-exchange", "after-exchange"):
        for mask in (3, 1, 2):
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
                        p, bytes([(old & ~(15 << shift)) | (((value >> (4 * i)) & 15) << shift)])
                    )

            put(entry, 0x6B18 if register == 0 else 0x7B18, 4)
            put(target, 0x2B18 if register == 0 else 0x3B18, 4)
            for i in range(9):
                uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0xABCDEF1234567890 + i))
            selected = cpu + 0x90C + 8 * register
            initial = 0xABCDEF1234500000 | target
            uc.mem_write(selected, struct.pack("<Q", initial))

            @Read
            def read(_, at, value):
                if not entry <= at < entry + 4:
                    return 0
                value[0] = (uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1))) & 15
                return 1

            reader, exchange = Reader(read, None), Exchange()
            a_low = int.from_bytes(uc.mem_read(cpu + 0x90C, 4), "little")
            c_low = int.from_bytes(uc.mem_read(cpu + 0x91C, 4), "little")
            assert (
                lib.hpclang_rpl_code_pc_exchange_inspect(
                    c.byref(reader), entry, 4, a_low, c_low, c.byref(exchange)
                )
                == 0
            )
            assert exchange.target == target and exchange.capture.destination == register
            assert exchange.capture.constant == entry + 4 and exchange.capture.source == 13
            for offset, value in ((0x95C, 7), (0x970, 1), (0x998, 3), (0xC24, 0)):
                uc.mem_write(cpu + offset, struct.pack("<I", value))
            for reg, value in (
                (UC_ARM_REG_R4, cpu),
                (UC_ARM_REG_R5, 0xFFFFF),
                (UC_ARM_REG_R7, entry),
                (UC_ARM_REG_SP, 0x308000),
            ):
                uc.reg_write(reg, value)
            expected_trace = [entry, target]
            trace = []

            def hook(_, address, size, unused):
                if address != 0x28898:
                    return
                pc = uc.reg_read(UC_ARM_REG_R7)
                trace.append(pc)
                if pc != expected_trace[len(trace) - 1] or len(trace) == len(expected_trace):
                    uc.emu_stop()

            uc.hook_add(UC_HOOK_CODE, hook)
            pc = entry
            if phase == "after-exchange":
                uc.emu_start(0x28828, 0x400000, count=20000)
                assert trace == expected_trace
                assert int.from_bytes(uc.mem_read(selected, 8), "little") == (
                    (initial & ~0xFFFFF) | (entry + 4)
                )
                pc = target
            before = bytes(uc.mem_read(cpu + 0x90C, 0x99C - 0x90C))
            data = bytearray()
            for page in range(128, 256):
                data.extend(
                    n
                    for byte in uc.mem_read(mapping.physical(page << 12), 2048)
                    for n in (byte & 15, byte >> 4)
                )
            array = (c.c_uint8 * len(data)).from_buffer_copy(data)
            value = c.c_uint32(int.from_bytes(uc.mem_read(selected, 4), "little"))
            saved_pc = c.c_uint32(pc)
            writes = c.c_uint32()
            guards = (Guard * 2)(
                Guard(1, entry, 0x6B18 if register == 0 else 0x7B18, 4),
                Guard(1, target, 0x2B18 if register == 0 else 0x3B18, 4),
            )

            @ReadWord
            def read_word(_, at, width, result):
                # Only these caller-owned instruction extents are qualified.
                if not any(g.field <= at and at + width <= g.field + g.width for g in guards):
                    return 0
                result[0] = sum(int(array[at - 0x80000 + i]) << (4 * i) for i in range(width))
                return 1

            stack_io = StackIO(None, read_word, None)
            a = c.c_uint32(struct.unpack_from("<I", before, 0)[0])
            cv = c.c_uint32(struct.unpack_from("<I", before, 16)[0])
            stack = (c.c_uint32 * 8).from_buffer_copy(before[0x978 - 0x90C : 0x998 - 0x90C])
            original_stack = bytes(stack)
            discovered = (c.POINTER(c.c_uint32) * 10)()
            masks = (c.c_uint32 * 10)()
            assert (
                lib.hpclang_stage_execution_roots(
                    c.byref(stack_io), pc, stack, 3, 1, c.byref(a), c.byref(cv), discovered, masks
                )
                == 1
            )
            selected_root = a if register == 0 else cv
            assert c.addressof(discovered[0].contents) == c.addressof(selected_root)
            assert masks[0] == 0xFFFFF and selected_root.value == value.value
            assert bytes(stack) == original_stack
            if mask == 3:
                # Both instruction identities must survive until commit. Inject
                # each changed/invalid nibble, including the final dependency.
                for guard in guards:
                    for nibble in range(guard.width):
                        index = guard.field + nibble - 0x80000
                        for fault in ("changed", "invalid"):
                            faulty = (c.c_uint8 * len(data)).from_buffer_copy(data)
                            faulty[index] = (faulty[index] ^ 1) if fault == "changed" else 16
                            unchanged = bytes(faulty)
                            register_copy, pc_copy = c.c_uint32(value.value), c.c_uint32(pc)
                            rejected_writes = c.c_uint32(123)
                            result = lib.bank_catalog_grow_dispatch_guarded(
                                faulty,
                                c.byref(register_copy),
                                c.byref(pc_copy),
                                3,
                                c.byref(rejected_writes),
                                None,
                                guards,
                                len(guards),
                            )
                            assert result != 0 and rejected_writes.value == 0
                            assert bytes(faulty) == unchanged
                            assert register_copy.value == value.value and pc_copy.value == pc
                            rejections.append(
                                dict(
                                    register=register,
                                    phase=phase,
                                    address=guard.field + nibble,
                                    fault=fault,
                                    result=result,
                                )
                            )
            status = lib.bank_catalog_grow_dispatch_guarded(
                array,
                c.byref(value),
                c.byref(saved_pc),
                mask,
                c.byref(writes),
                None,
                guards,
                len(guards),
            )
            assert status == 0 and writes.value > 0, status
            assert value.value == int.from_bytes(
                before[8 * register : 8 * register + 4], "little"
            ) + (16 if mask & 1 else 0)
            assert saved_pc.value == pc + (16 if mask & 2 else 0)
            for page in range(128, 256):
                lo = (page - 128) * 4096
                uc.mem_write(
                    mapping.physical(page << 12),
                    bytes(array[i] | array[i + 1] << 4 for i in range(lo, lo + 4096, 2)),
                )
            uc.mem_write(selected, struct.pack("<I", value.value))
            uc.reg_write(UC_ARM_REG_R7, saved_pc.value)
            expected_trace = (
                [entry + 16, target + 16, entry + 20]
                if phase == "before-exchange"
                else [target + 16, entry + 20]
            )
            trace = []
            uc.emu_start(0x28828, 0x400000, count=20000)
            assert trace
            if mask == 3:
                assert trace == expected_trace
                after = bytearray(before)
                struct.pack_into("<Q", after, 8 * register, (initial & ~0xFFFFF) | (entry + 20))
                assert bytes(uc.mem_read(cpu + 0x90C, len(after))) == after
            else:
                assert trace != expected_trace
                if mask == 1:
                    assert trace == [pc]
                else:
                    assert trace == [pc + 16, target if phase == "before-exchange" else entry + 4]
            assert bytes(uc.mem_read(0, len(flash))) == flash
            rows.append(
                dict(
                    register=register,
                    phase=phase,
                    mask=mask,
                    trace=trace,
                    expected_trace=expected_trace,
                    writes=writes.value,
                )
            )
assert hashes == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            cases=rows,
            rejections=rejections,
            source_sha256=hashes,
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
        ),
        indent=2,
    )
    + "\n"
)
print(args.model, "PASS 4 relocated exchange paths and 8 observed stale-address controls")
