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
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

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
args = parser.parse_args()
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
        for mode in ("baseline", "move") + (("stale-r0",) if flags == 1 else ()):
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

            def word(at):
                return sum(
                    ((uc.mem_read(mapping.physical(at + i), 1)[0] >> (4 * ((at + i) & 1))) & 15)
                    << (4 * i)
                    for i in range(5)
                )

            allocation_trace = []
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

                def allocation_hook(_, at, size, user):
                    if at == 0x28898:
                        pc = uc.reg_read(UC_ARM_REG_R7)
                        allocation_trace.append(pc)
                        if pc == 0x88C6E or len(allocation_trace) > 30000:
                            uc.emu_stop()

                handle = uc.hook_add(UC_HOOK_CODE, allocation_hook)
                uc.emu_start(0x28828, 0x400000, count=3000000)
                uc.hook_del(handle)
                assert allocation_trace[-1] == 0x88C6E and 0x5B80 in allocation_trace
                initial_r0 = int.from_bytes(uc.mem_read(cpu + 0x92C, 4), "little")
                target = initial_r0 & 0xFFFFF
                target_word = 0x2A2C
                assert word(target) == target_word and word(target + 5) >= 69
                assert int.from_bytes(uc.mem_read(cpu + 0x93C, 4), "little") & 0xFFF == 1
            put(0x8076B, 0x88D67)
            entry = c.c_uint32(0x88C73)
            r0 = c.c_uint32(initial_r0)
            writes = c.c_uint32()
            delta = 0 if mode == "baseline" else 16
            if delta:
                nibbles = bytearray(
                    n
                    for page in range(128, 256)
                    for value in uc.mem_read(mapping.physical(page << 12), 2048)
                    for n in (value & 15, value >> 4)
                )
                data = (c.c_uint8 * len(nibbles)).from_buffer(nibbles)
                assert (
                    lib.bank_catalog_grow_r0(
                        data,
                        c.byref(entry),
                        c.byref(r0),
                        int(flags == 1 and mode == "move"),
                        c.byref(writes),
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
            assert r0.value == initial_r0 + (16 if mode == "move" and flags == 1 else 0)
            for off, value in ((0x95C, 0), (0x970, 0), (0xC24, 0), (0x998, cursor)):
                uc.mem_write(cpu + off, struct.pack("<I", value))
            for i in range(9):
                uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0x123456789AB00000 + i))
            uc.mem_write(cpu + 0x92C, struct.pack("<Q", 0xABCDEF1200000000 | r0.value))
            uc.mem_write(cpu + 0x93C, struct.pack("<Q", 0x76543210ABCDE000 | flags))
            saved = bytes(uc.mem_read(cpu + 0x92C, 40))
            for i in range(8):
                uc.mem_write(cpu + 0x978 + 4 * i, struct.pack("<I", 0xA1000 + 0x101 * i))
            caller_slot = (cursor + 7) & 7
            uc.mem_write(cpu + 0x978 + 4 * caller_slot, struct.pack("<I", 0x3130))
            stack = word(0x806F8)
            old_top = word(stack)
            uc.reg_write(UC_ARM_REG_R4, cpu)
            uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
            uc.reg_write(UC_ARM_REG_R7, entry.value)
            uc.reg_write(UC_ARM_REG_SP, 0x308000)
            trace = []
            accesses = []
            cursor_steps = []

            memory_checks = []
            pending_memory = []

            def live_nibble(at):
                return uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1)) & 15

            @Read
            def read_live(_, at, value):
                value[0] = live_nibble(at)
                return 1

            live_reader = Reader(read_live, None)

            def hook(_, at, size, user):
                if at == 0x28898:
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
            uc.emu_start(0x28828, 0x400000, count=30000)
            assert trace[-1] == 0x3130 and cursor_steps == [1, 7, 1, 7, 7]
            assert memory_checks and not pending_memory
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
            for i in range(8):
                assert int.from_bytes(uc.mem_read(cpu + 0x978 + 4 * i, 4), "little") == (
                    0 if i in (cursor, caller_slot) else 0xA1000 + 0x101 * i
                )
            rows.append(
                dict(
                    allocation_trace=[hex(x) for x in allocation_trace],
                    target=hex(target),
                    cursor=cursor,
                    flags=flags,
                    mode=mode,
                    r0_read=used,
                    memory_transfers=memory_checks,
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
