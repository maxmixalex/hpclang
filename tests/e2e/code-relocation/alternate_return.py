"""Controlled alternate ARM return: conditional R0 use and real catalog repairs.

This does not establish entry reachability, incoming R0 provenance, initial
return-stack depth, later caller execution safety or complete relocation coverage.
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

from unicorn import (
    Uc,
    UC_ARCH_ARM,
    UC_MODE_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_WRITE,
    UC_MEM_READ,
)
from unicorn.arm_const import (
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R4,
    UC_ARM_REG_R5,
    UC_ARM_REG_R7,
    UC_ARM_REG_SP,
)

if not __debug__:
    raise RuntimeError("Verification requires assertions")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp50g", "hp49gplus"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
parser.add_argument(
    "--allocated-root",
    action="store_true",
    help="Use original legacy allocation to establish R0 (flags=1)",
)
parser.add_argument(
    "--arm-return",
    choices=("preserve", "clobber"),
    help="Execute a controlled ARM leaf; clobber demonstrates the r0 continuation contract",
)
parser.add_argument(
    "--continuous-return",
    action="store_true",
    help="Retain actual allocation/ARM return state through catalog movement and Saturn continuation",
)
args = parser.parse_args()
if args.continuous_return:
    if args.arm_return == "clobber":
        parser.error("continuous return requires the preserved ARMSYS continuation")
    args.arm_return = "preserve"
if args.arm_return:
    args.allocated_root = True
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words

out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
report = out / "alternate-return.json"
report.unlink(missing_ok=True)
base = root / "runtime/core"
stage = root / "platform/armtoolbox/stage-stack.c"
shim = Path(__file__).with_name("threaded-return.c")
names = [
    "rpl-mutation.c",
    "rpl-object-store.c",
    "rpl-stack-view.c",
    "object-snapshot.c",
    "rpl-object-span.c",
    "rpl-code.c",
]
sources = [stage, shim, *[base / n for n in names]]
tracked = [
    *sources,
    Path(__file__),
    base / "rpl-mutation.h",
    base / "rpl-code.h",
    base / "rpl-reference.h",
    stage.with_suffix(".h"),
]
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
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
        "-I" + str(stage.parent),
        "-I" + str(root / "include"),
        *map(str, sources),
        "-o",
        str(out / "geometry.so"),
    ],
    check=True,
)
lib = c.CDLL(str(out / "geometry.so"))
lib.bank_catalog_grow_r0.argtypes = [
    c.POINTER(c.c_uint8),
    c.POINTER(c.c_uint32),
    c.POINTER(c.c_uint32),
    c.c_uint,
    c.POINTER(c.c_uint32),
]
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


MappedRead = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint32, c.POINTER(c.c_uint8))


class MappedReader(c.Structure):
    _fields_ = [("read", MappedRead), ("context", c.c_void_p)]


class Guard(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("mapping", "field", "expected", "width")]


lib.bank_catalog_grow_r0_guarded.argtypes = [
    *lib.bank_catalog_grow_r0.argtypes,
    c.POINTER(MappedReader),
    c.POINTER(Guard),
    c.c_uint32,
    c.c_uint32,
]


class Assignment(c.Structure):
    _fields_ = [
        (n, c.c_uint32)
        for n in (
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


class Memory(c.Structure):
    _fields_ = [
        (name, c.c_uint32)
        for name in ("instruction", "length", "data", "address", "first", "nibbles", "load")
    ]


class Origins(c.Structure):
    _fields_ = [("registers", (c.c_uint32 * 16) * 13)]


class Exchange(c.Structure):
    _fields_ = [
        (name, c.c_uint32)
        for name in ("instruction", "length", "left", "right", "first", "nibbles")
    ]


lib.bank_temp_allocation.argtypes = [c.POINTER(Reader), c.c_uint32, c.POINTER(c.c_uint32)]
lib.hpclang_rpl_object_span.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.POINTER(c.c_uint32),
]
lib.hpclang_rpl_code_exchange_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint,
    c.POINTER(Exchange),
]
lib.hpclang_rpl_code_origins_exchange.argtypes = [c.POINTER(Origins), c.POINTER(Exchange)]


class Adjustment(c.Structure):
    _fields_ = [
        ("instruction", c.c_uint32),
        ("length", c.c_uint32),
        ("address", c.c_uint32),
        ("delta", c.c_int32),
    ]


lib.hpclang_rpl_code_address_adjust_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.POINTER(Adjustment),
]


lib.hpclang_rpl_code_origins_assign.argtypes = [
    c.POINTER(Origins),
    c.POINTER(Assignment),
    c.POINTER(c.c_uint32),
]
lib.hpclang_rpl_code_origins_memory.argtypes = [
    c.POINTER(Origins),
    c.POINTER(Memory),
    c.POINTER(c.c_uint32),
    c.c_uint,
]


lib.hpclang_rpl_code_memory_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint,
    c.POINTER(Memory),
]


lib.hpclang_rpl_code_assignment_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint,
    c.POINTER(Assignment),
]
flash, ram = (args.state / "flash").read_bytes(), (args.state / "sram").read_bytes()
if len(flash) != 0x200000 or len(ram) != 0x80000:
    raise ValueError("Expected full SRAM/Flash images")
mapping = native_words(ram, flash)
cpu = 0x08000000 + (mapping.table - 128) * 4


def encoding(at, size):
    return "".join(format(mapping(at + i, 1), "x") for i in range(size))


if encoding(0x88C73, 0x2F) != "8f2d76081a31a808b1d1808a06181af101471411c4145cf":
    raise ValueError("Alternate return differs from reviewed code")
if encoding(0x67D2, 52) != "1bb9e08146d71b3f608146d51b8f6081461351bb670814613403":
    raise ValueError("GETPTR differs from reviewed code")
if encoding(0x679B, 0x37) != "1361bb67081441b8f6081371351441b3f608d91441bb9e08db14403":
    raise ValueError("PUTPTR differs from reviewed code")
if encoding(0x88CA2, 9) != "8fb976003":
    raise ValueError("Alternate return tail differs from reviewed code")
if mapping(0x88D67) != 0x312B:
    raise ValueError("Controlled target differs from reviewed thread")


@Read
def read_code(_, at, value):
    if not 0x88C8E <= at < 0x88C94:
        return 0
    value[0] = mapping(at, 1)
    return 1


reader = Reader(read_code, None)
assignment = Assignment()
assert (
    lib.hpclang_rpl_code_assignment_inspect(c.byref(reader), 0x88C8E, 6, 0, c.byref(assignment))
    == 0
)
assert (
    assignment.source,
    assignment.destination,
    assignment.source_first,
    assignment.destination_first,
    assignment.nibbles,
) == (4, 0, 0, 0, 5)
rows = []
baseline_traces = {}
for cursor in range(8):
    for flags in (1,) if args.allocated_root else range(4):
        modes = (
            ("baseline",)
            if args.arm_return == "clobber"
            else ("baseline", "move") + (("stale-r0",) if flags == 1 else ())
        )
        for mode in modes:
            uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
            uc.mem_map(0, len(flash))
            uc.mem_write(0, flash)
            uc.mem_map(0x08000000, len(ram))
            uc.mem_write(0x08000000, ram)
            uc.mem_map(0x300000, 0x10000)

            def put(at, value):
                for i in range(5):
                    p = mapping.physical(at + i)
                    shift = 4 * ((at + i) & 1)
                    old = uc.mem_read(p, 1)[0]
                    uc.mem_write(
                        p, bytes([(old & ~(15 << shift)) | ((value >> (4 * i) & 15) << shift)])
                    )

            def nibble(at):
                return uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1)) & 15

            def word(at):
                return sum(nibble(at + i) << (4 * i) for i in range(5))

            caller_slot = (cursor + 7) & 7
            if args.continuous_return:
                put(0x8076B, 0x88D67)
                for i in range(8):
                    uc.mem_write(
                        cpu + 0x978 + 4 * i,
                        struct.pack("<I", 0x3130 if i == caller_slot else 0xA1000 + 0x101 * i),
                    )

            allocation_trace = []
            allocation_origins = Origins()
            allocation_origin_path = []
            allocation_bounds = []
            allocation_start_top = word(0x806EE)
            arm_trace = []
            arm_arguments = []
            allocation_reads = {}
            guard_rejections = 0
            register_guard_rejections = 0
            post_move_allocation = None

            @Read
            def allocation_read(_, at, value):
                value[0] = uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1)) & 15
                allocation_reads[at] = value[0]
                return 1

            allocation_reader = Reader(allocation_read, None)
            target = 0x88D67
            initial_r0 = 0x34588D67
            target_word = 0x312B
            if args.allocated_root:
                assert (
                    hashlib.sha256(bytes(mapping(0x88BC1 + i, 1) for i in range(0xAD))).hexdigest()
                    == "7db56c9eeb8b94b85ba4d3e04804fd97e43c2c0b352d5796bee3f330bf8a49a0"
                )
                for off, value in ((0x95C, 0), (0x970, 0), (0xC24, 0), (0x998, cursor)):
                    uc.mem_write(cpu + off, struct.pack("<I", value))
                uc.mem_write(cpu + 0x90C, struct.pack("<Q", 1))
                uc.mem_write(cpu + 0x914, struct.pack("<Q", (0x08050000 << 32) | (1 << 20) | 32))
                uc.mem_write(cpu + 0x92C, struct.pack("<Q", 0xABCDEF1234588D67))
                uc.reg_write(UC_ARM_REG_R4, cpu)
                uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
                uc.reg_write(UC_ARM_REG_R7, 0x88BC1)
                uc.reg_write(UC_ARM_REG_SP, 0x308000)
                if args.arm_return:
                    # ARMSYS returns Saturn PC in ARM r0. The other volatile
                    # registers and ARM condition flags are not that contract.
                    leaf = [0xE3A01022, 0xE3A02033, 0xE3A03044, 0xE3A0C055, 0xE3510022]
                    if args.arm_return == "clobber":
                        leaf.append(0xE3A00011)
                    leaf.append(0xE12FFF1E)  # BX LR
                    uc.mem_write(0x08050000, struct.pack("<" + "I" * len(leaf), *leaf))

                def allocation_hook(_, at, size, user):
                    if args.arm_return and 0x08050000 <= at < 0x08050000 + 4 * len(leaf):
                        arm_trace.append(at)
                        if at == 0x08050000:
                            arm_arguments.extend(
                                (uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1))
                            )
                            assert arm_arguments == [0x88C73, cpu]
                    if at == 0x28898:
                        pc = uc.reg_read(UC_ARM_REG_R7)
                        allocation_trace.append(pc)
                        if args.arm_return and arm_trace:
                            assert pc == (0x11 if args.arm_return == "clobber" else 0x88C73)
                            uc.emu_stop()
                            return
                        steps = (0x5BC2, 0x5BC5, 0x5BC8)
                        if allocation_origin_path and len(allocation_origin_path) < 3:
                            assert pc == steps[len(allocation_origin_path)]
                        if pc in steps:
                            if pc == steps[0]:
                                first = int.from_bytes(uc.mem_read(cpu + 0x954, 4), "little")
                                extent = c.c_uint32()
                                assert first == allocation_start_top + 1
                                assert (
                                    lib.bank_temp_allocation(
                                        c.byref(allocation_reader), first, c.byref(extent)
                                    )
                                    == 0
                                )
                                assert word(0x806EE) == allocation_start_top + extent.value + 6
                                allocation_bounds.extend((first, extent.value))
                                for i in range(5):
                                    allocation_origins.registers[11][i] = 0x100 + i
                            if pc == steps[1]:
                                copy = Assignment()
                                assert (
                                    lib.hpclang_rpl_code_assignment_inspect(
                                        c.byref(allocation_reader), pc, 3, 0, c.byref(copy)
                                    )
                                    == 0
                                )
                                assert (copy.source, copy.destination, copy.nibbles) == (0, 11, 5)
                                assert (
                                    lib.hpclang_rpl_code_origins_assign(
                                        c.byref(allocation_origins), c.byref(copy), None
                                    )
                                    == 0
                                )
                            else:
                                exchange = Exchange()
                                assert (
                                    lib.hpclang_rpl_code_exchange_inspect(
                                        c.byref(allocation_reader), pc, 3, 0, c.byref(exchange)
                                    )
                                    == 0
                                )
                                assert (
                                    exchange.left,
                                    exchange.right,
                                    exchange.first,
                                    exchange.nibbles,
                                ) == ((0, 11, 0, 5) if pc == steps[0] else (0, 4, 0, 16))
                                assert (
                                    lib.hpclang_rpl_code_origins_exchange(
                                        c.byref(allocation_origins), c.byref(exchange)
                                    )
                                    == 0
                                )
                            allocation_origin_path.append(pc)
                        elif len(allocation_origin_path) == 3:
                            assert (
                                int.from_bytes(uc.mem_read(cpu + 0x92C, 4), "little") & 0xFFFFF
                                == allocation_bounds[0]
                            )
                        if (pc == 0x88C6E and not args.arm_return) or len(allocation_trace) > 30000:
                            uc.emu_stop()

                def allocation_write(_, kind, at, size, value, user):
                    if len(allocation_origin_path) == 3 and allocation_trace[-1] != 0x5BC8:
                        assert not (at < cpu + 0x934 and at + size > cpu + 0x92C), (
                            "unmodeled R0 write after qualification"
                        )

                write_handle = uc.hook_add(UC_HOOK_MEM_WRITE, allocation_write)
                handle = uc.hook_add(UC_HOOK_CODE, allocation_hook)
                uc.emu_start(0x28828, 0x400000, count=3000000)
                uc.hook_del(handle)
                uc.hook_del(write_handle)
                assert 0x5B80 in allocation_trace
                if args.arm_return:
                    assert arm_trace == [0x08050000 + 4 * i for i in range(len(leaf))]
                    if args.arm_return == "clobber":
                        assert allocation_trace[-1] == 0x11
                        rows.append(
                            dict(
                                cursor=cursor,
                                arm_return=args.arm_return,
                                arm_arguments=arm_arguments,
                                arm_trace=arm_trace,
                                observed_saturn_pc=allocation_trace[-1],
                                expected_saturn_pc=0x88C73,
                            )
                        )
                        continue
                    assert allocation_trace[-1] == 0x88C73
                    assert (
                        int.from_bytes(uc.mem_read(cpu + 0x92C, 4), "little") & 0xFFFFF
                        == allocation_bounds[0]
                    )
                else:
                    assert allocation_trace[-1] == 0x88C6E
                initial_r0 = int.from_bytes(uc.mem_read(cpu + 0x92C, 4), "little")
                target = initial_r0 & 0xFFFFF
                target_word = 0x2A2C
                assert word(target) == target_word and word(target + 5) >= 69
                assert len(allocation_origin_path) == 3 and target == allocation_bounds[0]
                assert list(allocation_origins.registers[4])[:5] == [0x100 + i for i in range(5)]
                span = c.c_uint32()
                assert (
                    lib.hpclang_rpl_object_span(
                        c.byref(allocation_reader), target, allocation_bounds[1], c.byref(span)
                    )
                    == 0
                )
                assert span.value == allocation_bounds[1]
                # The TEMP query qualifies exact starts, never coincidental
                # interior values. Unreadable chain metadata cannot leave a
                # partially qualified output behind.
                for candidate in (target - 1, target + 1, target + allocation_bounds[1] - 1):
                    extent = c.c_uint32(0x12345678)
                    assert (
                        lib.bank_temp_allocation(
                            c.byref(allocation_reader), candidate, c.byref(extent)
                        )
                        == -7
                    )
                    assert extent.value == 0x12345678
                for unreadable in (0x806EE, word(0x806E9), word(0x806EE) - 5):

                    @Read
                    def failing_read(context, at, value):
                        return 0 if at == unreadable else allocation_read(context, at, value)

                    failing_reader = Reader(failing_read, None)
                    extent = c.c_uint32(0x12345678)
                    assert (
                        lib.bank_temp_allocation(c.byref(failing_reader), target, c.byref(extent))
                        < 0
                    )
                    assert extent.value == 0x12345678
                assert int.from_bytes(uc.mem_read(cpu + 0x93C, 4), "little") & 0xFFF == 1
            if not args.continuous_return:
                put(0x8076B, 0x88D67)
            entry = c.c_uint32(uc.reg_read(UC_ARM_REG_R7) if args.continuous_return else 0x88C73)
            assert entry.value == 0x88C73
            boundary_state = bytes(uc.mem_read(cpu + 0x90C, 0x90))
            boundary_sp = uc.reg_read(UC_ARM_REG_SP)
            r0 = c.c_uint32(initial_r0)
            writes = c.c_uint32()
            delta = 0 if mode == "baseline" else 16
            allocation_bytes = (
                bytes(nibble(target + i) for i in range(allocation_bounds[1]))
                if args.allocated_root
                else b""
            )
            if delta:
                nibbles = bytearray(
                    n
                    for page in range(128, 256)
                    for value in uc.mem_read(mapping.physical(page << 12), 2048)
                    for n in (value & 15, value >> 4)
                )
                data = (c.c_uint8 * len(nibbles)).from_buffer(nibbles)
                guards = (Guard * len(allocation_reads))(
                    *[
                        Guard(1 if at >= 0x80000 else 2, at, value, 1)
                        for at, value in sorted(allocation_reads.items())
                    ]
                )

                @MappedRead
                def read_guard(_, identity, at, value):
                    # Main-RAM dependencies must use the mutation IO, not this
                    # alternate reader. ROM has a separately owned mapping.
                    if identity != 2 or at >= 0x80000:
                        return 0
                    value[0] = uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1)) & 15
                    return 1

                guard_reader = MappedReader(read_guard, None)
                if args.allocated_root:
                    before_move = bytes(data)
                    for field in (0x806EE, 0x5BC8, target):
                        guard = next(g for g in guards if g.field == field)
                        guard.expected ^= 1
                        probe_entry, probe_r0, probe_writes = (
                            c.c_uint32(entry.value),
                            c.c_uint32(r0.value),
                            c.c_uint32(),
                        )
                        assert (
                            lib.bank_catalog_grow_r0_guarded(
                                data,
                                c.byref(probe_entry),
                                c.byref(probe_r0),
                                1,
                                c.byref(probe_writes),
                                c.byref(guard_reader),
                                guards,
                                len(guards),
                                initial_r0,
                            )
                            < 0
                        )
                        assert not probe_writes.value and bytes(data) == before_move
                        assert (probe_entry.value, probe_r0.value) == (entry.value, r0.value)
                        guard.expected ^= 1
                        guard_rejections += 1
                    for change in (1, 0x100000):
                        probe_entry, probe_r0, probe_writes = (
                            c.c_uint32(entry.value),
                            c.c_uint32(initial_r0 ^ change),
                            c.c_uint32(),
                        )
                        assert (
                            lib.bank_catalog_grow_r0_guarded(
                                data,
                                c.byref(probe_entry),
                                c.byref(probe_r0),
                                1,
                                c.byref(probe_writes),
                                c.byref(guard_reader),
                                guards,
                                len(guards),
                                initial_r0,
                            )
                            < 0
                        )
                        assert not probe_writes.value and bytes(data) == before_move
                        assert (probe_entry.value, probe_r0.value) == (
                            entry.value,
                            initial_r0 ^ change,
                        )
                        register_guard_rejections += 1
                assert (
                    lib.bank_catalog_grow_r0_guarded(
                        data,
                        c.byref(entry),
                        c.byref(r0),
                        int(flags == 1 and mode == "move"),
                        c.byref(writes),
                        c.byref(guard_reader),
                        guards,
                        len(guards),
                        initial_r0,
                    )
                    == 0
                )
                assert entry.value == 0x88C73 + 16 and writes.value
                for page in range(128, 256):
                    lo = (page - 128) * 4096
                    uc.mem_write(
                        mapping.physical(page << 12),
                        bytes(nibbles[i] | nibbles[i + 1] << 4 for i in range(lo, lo + 4096, 2)),
                    )
                assert word(target + 16) == target_word and word(target) != target_word
            if args.allocated_root:
                moved = target + delta
                extent = c.c_uint32()
                assert (
                    lib.bank_temp_allocation(c.byref(allocation_reader), moved, c.byref(extent))
                    == 0
                )
                assert extent.value == allocation_bounds[1]
                assert bytes(nibble(moved + i) for i in range(extent.value)) == allocation_bytes
                span = c.c_uint32()
                assert (
                    lib.hpclang_rpl_object_span(
                        c.byref(allocation_reader), moved, extent.value, c.byref(span)
                    )
                    == 0
                )
                assert span.value == extent.value
                candidate = c.c_uint32(0x12345678)
                qualified = lib.bank_temp_allocation(
                    c.byref(allocation_reader), r0.value & 0xFFFFF, c.byref(candidate)
                )
                assert qualified == (-7 if mode == "stale-r0" else 0)
                if mode == "stale-r0":
                    assert candidate.value == 0x12345678
                else:
                    assert candidate.value == extent.value
                post_move_allocation = dict(
                    first=moved, extent=extent.value, r0_qualified=qualified == 0
                )
            assert r0.value == initial_r0 + (16 if mode == "move" and flags == 1 else 0)
            if args.continuous_return:
                assert bytes(uc.mem_read(cpu + 0x90C, 0x90)) == boundary_state
                assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == cursor
                assert uc.reg_read(UC_ARM_REG_SP) == boundary_sp
                # Publish only the proven low-word repair into the suspended
                # interpreter. Keep real high words, flags and stack cells.
                uc.mem_write(cpu + 0x92C, struct.pack("<I", r0.value))
            else:
                for off, value in ((0x95C, 0), (0x970, 0), (0xC24, 0), (0x998, cursor)):
                    uc.mem_write(cpu + off, struct.pack("<I", value))
                for i in range(9):
                    uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0x123456789AB00000 + i))
                uc.mem_write(cpu + 0x92C, struct.pack("<Q", 0xABCDEF1200000000 | r0.value))
                uc.mem_write(cpu + 0x93C, struct.pack("<Q", 0x76543210ABCDE000 | flags))
            saved = bytes(uc.mem_read(cpu + 0x92C, 40))
            if not args.continuous_return:
                for i in range(8):
                    uc.mem_write(cpu + 0x978 + 4 * i, struct.pack("<I", 0xA1000 + 0x101 * i))
                uc.mem_write(cpu + 0x978 + 4 * caller_slot, struct.pack("<I", 0x3130))
            return_cells = struct.unpack("<8I", uc.mem_read(cpu + 0x978, 32))
            stack = word(0x806F8)
            old_top = word(stack)
            if not args.continuous_return:
                uc.reg_write(UC_ARM_REG_R4, cpu)
                uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
            uc.reg_write(UC_ARM_REG_R7, entry.value)
            if not args.continuous_return:
                uc.reg_write(UC_ARM_REG_SP, 0x308000)
            trace = []
            accesses = []
            cursor_steps = []

            memory_checks = []
            pending_memory = []
            origins = Origins()
            origin_path = []
            origin_memory = (c.c_uint32 * 5)(*[0x200 + i for i in range(5)])
            preserved_top = (c.c_uint32 * 5)()
            origin_address = []
            pending_address_step = []
            origin_steps = tuple(pc + delta for pc in (0x88C8E, 0x88C94, 0x88C97, 0x88C9A, 0x88C9D))

            def live_nibble(at):
                return uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1)) & 15

            @Read
            def read_live(_, at, value):
                value[0] = live_nibble(at)
                return 1

            live_reader = Reader(read_live, None)

            def hook(_, at, size, user):
                if at == 0x28898:
                    if pending_address_step:
                        assert bytes(uc.mem_read(cpu + 0x90C, 0x90)) == pending_address_step.pop()
                    if pending_memory:
                        reg_at, expected_register, address, expected_memory, address_state = (
                            pending_memory.pop()
                        )
                        assert int.from_bytes(uc.mem_read(reg_at, 8), "little") == expected_register
                        assert [
                            live_nibble(address + i) for i in range(len(expected_memory))
                        ] == expected_memory
                        assert bytes(uc.mem_read(cpu + 0x954, 8)) == address_state
                    pc = uc.reg_read(UC_ARM_REG_R7)
                    trace.append(pc)
                    if origin_path and len(origin_path) < len(origin_steps):
                        assert pc == origin_steps[len(origin_path)], "unmodeled origin-path step"
                    if pc == origin_steps[0]:
                        # Caller-owned symbolic labels for R0.A; numeric values
                        # never establish these labels or qualify a target map.
                        for i in range(5):
                            origins.registers[4][i] = (
                                allocation_origins.registers[4][i]
                                if args.allocated_root
                                else 0x100 + i
                            )
                        copy = Assignment()
                        assert (
                            lib.hpclang_rpl_code_assignment_inspect(
                                c.byref(live_reader), pc, 6, 0, c.byref(copy)
                            )
                            == 0
                        )
                        assert (copy.source, copy.destination, copy.nibbles) == (4, 0, 5)
                        assert (
                            lib.hpclang_rpl_code_origins_assign(
                                c.byref(origins), c.byref(copy), None
                            )
                            == 0
                        )
                        origin_path.append(pc)
                    if pc == origin_steps[3]:
                        # Decode arithmetic separately from nibble copies. Its
                        # borrow invalidates D1 origins and replaces carry.
                        assert len(origin_path) == 3
                        adjustment = Adjustment()
                        assert (
                            lib.hpclang_rpl_code_address_adjust_inspect(
                                c.byref(live_reader), pc, 3, c.byref(adjustment)
                            )
                            == 0
                        )
                        assert (adjustment.address, adjustment.delta, adjustment.length) == (
                            12,
                            -5,
                            3,
                        )
                        before = bytearray(uc.mem_read(cpu + 0x90C, 0x90))
                        d1 = struct.unpack_from("<I", before, 0x4C)[0]
                        assert origin_address == [d1] and d1 == stack and d1 >= 5
                        result = d1 + adjustment.delta
                        assert 0 <= result <= 0xFFFFF
                        struct.pack_into("<I", before, 0x4C, result)
                        struct.pack_into("<I", before, 0x64, 0)
                        pending_address_step.append(bytes(before))
                        for i in range(5):
                            origins.registers[12][i] = 0
                        origin_path.append(pc)
                    if pc == 0x3130 or len(trace) > 120:
                        uc.emu_stop()
                        return
                    if live_nibble(pc) == 1 and live_nibble(pc + 1) in (4, 5):
                        transfer = Memory()
                        assert (
                            lib.hpclang_rpl_code_memory_inspect(
                                c.byref(live_reader),
                                pc,
                                4,
                                int.from_bytes(uc.mem_read(cpu + 0x95C, 4), "little"),
                                c.byref(transfer),
                            )
                            == 0
                        )
                        address_state = bytes(uc.mem_read(cpu + 0x954, 8))
                        address = struct.unpack_from(
                            "<I", address_state, 4 * (transfer.address - 11)
                        )[0]
                        assert 0 <= address < address + transfer.nibbles <= 0x100000
                        if pc in (origin_steps[1], origin_steps[2], origin_steps[4]):
                            assert len(origin_path) == origin_steps.index(pc)
                            assert transfer.nibbles == 5 and transfer.first == 0
                            if pc == origin_steps[1]:
                                assert transfer.load == 1 and transfer.data == 2
                                assert address == stack
                                origin_address.append(address)
                            elif pc == origin_steps[2]:
                                assert transfer.load == 0 and transfer.data == 0
                                assert origin_address == [address]
                            else:
                                assert transfer.load == 0 and transfer.data == 2
                                assert origin_address == [address + 5]
                                assert address == stack - 5
                            tokens = preserved_top if pc == origin_steps[4] else origin_memory
                            assert (
                                lib.hpclang_rpl_code_origins_memory(
                                    c.byref(origins), c.byref(transfer), tokens, 5
                                )
                                == 0
                            )
                            origin_path.append(pc)
                            if len(origin_path) == 3:
                                assert list(origin_memory) == [0x100 + i for i in range(5)]
                            if len(origin_path) == 5:
                                assert list(origin_memory) == [0x100 + i for i in range(5)]
                                assert list(preserved_top) == [0x200 + i for i in range(5)]
                        reg_at = cpu + 0x90C + 8 * transfer.data
                        register = int.from_bytes(uc.mem_read(reg_at, 8), "little")
                        values = [live_nibble(address + i) for i in range(transfer.nibbles)]
                        mask = ((1 << (4 * transfer.nibbles)) - 1) << (4 * transfer.first)
                        if transfer.load:
                            value = sum(n << (4 * i) for i, n in enumerate(values))
                            register = (register & ~mask) | (value << (4 * transfer.first))
                        else:
                            values = [
                                (register >> (4 * (transfer.first + i))) & 15
                                for i in range(transfer.nibbles)
                            ]
                        pending_memory.append((reg_at, register, address, values, address_state))
                        memory_checks.append(
                            dict(
                                instruction=pc,
                                load=transfer.load,
                                register=transfer.data,
                                address=address,
                                first=transfer.first,
                                nibbles=transfer.nibbles,
                            )
                        )

            def access(_, kind, at, size, value, user):
                if at < cpu + 0x954 and at + size > cpu + 0x92C:
                    accesses.append((kind, at - cpu, size))
                if kind != UC_MEM_READ and at == cpu + 0x998:
                    assert size == 4
                    cursor_steps.append((value - int.from_bytes(uc.mem_read(at, 4), "little")) & 7)

            uc.hook_add(UC_HOOK_CODE, hook)
            uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, access)
            uc.emu_start(0x28898 if args.continuous_return else 0x28828, 0x400000, count=30000)
            assert trace[-1] == 0x3130 and cursor_steps == [1, 7, 1, 7, 7]
            assert memory_checks and not pending_memory
            assert not pending_address_step
            assert len(origin_path) == (5 if flags == 1 else 0)
            assert 0x88CA2 + delta in trace and 0x88CA9 + delta in trace
            assert word(0x806F8) == stack - (5 if flags == 1 else 0)
            assert word(0x8076B) == 0x88D67 + delta
            normalized = [pc - delta if 0x88000 <= pc < 0x89000 else pc for pc in trace]
            if mode == "baseline":
                baseline_traces[cursor, flags] = normalized
            else:
                assert normalized == baseline_traces[cursor, flags]
            assert int.from_bytes(uc.mem_read(cpu + 0x970, 4), "little") == 0
            used = any(
                k == UC_MEM_READ and at < 0x934 and at + size > 0x92C for k, at, size in accesses
            )
            assert used == (flags == 1)
            assert all(
                k == UC_MEM_READ and (0x92C <= at < 0x934 or 0x93C <= at < 0x944)
                for k, at, size in accesses
            )
            assert bytes(uc.mem_read(cpu + 0x92C, 40)) == saved
            assert word(stack) == ((r0.value & 0xFFFFF) if flags == 1 else old_top)
            if flags == 1:
                assert word(stack - 5) == old_top
                assert (word(stack) == target + delta) == (mode != "stale-r0")
            assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == caller_slot
            if args.continuous_return:
                assert uc.reg_read(UC_ARM_REG_SP) == boundary_sp
                final_extent = c.c_uint32(0x12345678)
                final_query = lib.bank_temp_allocation(
                    c.byref(allocation_reader), word(stack), c.byref(final_extent)
                )
                assert final_query == (-7 if mode == "stale-r0" else 0)
                assert final_extent.value == (
                    0x12345678 if mode == "stale-r0" else allocation_bounds[1]
                )
            for i in range(8):
                assert int.from_bytes(uc.mem_read(cpu + 0x978 + 4 * i, 4), "little") == (
                    0 if i in (cursor, caller_slot) else return_cells[i]
                )
            rows.append(
                dict(
                    allocation_trace=[hex(x) for x in allocation_trace],
                    arm_return=args.arm_return,
                    continuous_return=args.continuous_return,
                    boundary_sp=boundary_sp if args.continuous_return else None,
                    arm_arguments=arm_arguments,
                    arm_trace=arm_trace,
                    allocation_origin_path=allocation_origin_path,
                    allocation_bounds=allocation_bounds,
                    allocation_guard_rejections=guard_rejections,
                    register_guard_rejections=register_guard_rejections,
                    post_move_allocation=post_move_allocation,
                    target=hex(target),
                    cursor=cursor,
                    flags=flags,
                    mode=mode,
                    r0_read=used,
                    memory_transfers=memory_checks,
                    r0_origin_path=origin_path,
                    stack_origin_slots=(
                        {str(stack): list(origin_memory), str(stack - 5): list(preserved_top)}
                        if flags == 1
                        else {}
                    ),
                    writes=writes.value,
                    stack_peak=1,
                    stack_final=-1,
                    trace=[hex(x) for x in trace],
                )
            )
assert hashes == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            inputs={
                "flash": hashlib.sha256(flash).hexdigest(),
                "sram": hashlib.sha256(ram).hexdigest(),
            },
            sources=hashes,
            rows=rows,
        ),
        indent=2,
    )
    + "\n"
)
print(args.model, "PASS", len(rows), "cases; allocated_root=", args.allocated_root)
