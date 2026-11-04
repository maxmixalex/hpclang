"""Controlled MMU entry and return; does not execute the intervening ARM program."""

import argparse
import ctypes as c
import subprocess
import hashlib
import importlib
import json
from pathlib import Path
import struct
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
    raise RuntimeError("Assertions required")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--launcher", type=Path, required=True)
parser.add_argument("--model", choices=("hp50g", "hp49gplus"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument(
    "--descriptor",
    action="store_true",
    help="Run original command-4 descriptor loads before MMU setup",
)
parser.add_argument(
    "--linked-descriptor",
    action="store_true",
    help="Read link fields and allocate descriptor using original helpers",
)
parser.add_argument(
    "--full-link",
    action="store_true",
    help="Execute continuous original PrLNK with its XLIB launcher reference",
)
parser.add_argument(
    "--relocate", action="store_true", help="Grow the real C catalog at the controlled ARM boundary"
)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
if args.full_link:
    args.linked_descriptor = True
if args.linked_descriptor:
    args.descriptor = True
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
report = out / "mmu-entry.json"
report.unlink(missing_ok=True)
sources = {}
if args.relocate:
    base = root / "runtime/core"
    stage = root / "platform/armtoolbox/stage-stack.c"
    shim = Path(__file__).with_name("threaded-return.c")
    files = [
        stage,
        shim,
        *[
            base / name
            for name in (
                "rpl-mutation.c",
                "rpl-object-store.c",
                "rpl-stack-view.c",
                "object-snapshot.c",
                "rpl-object-span.c",
            )
        ],
    ]
    sources = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
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
            *map(str, files),
            "-o",
            str(out / "geometry.so"),
        ],
        check=True,
    )
    lib = c.CDLL(str(out / "geometry.so"))
    lib.bank_catalog_grow.argtypes = [
        c.POINTER(c.c_uint8),
        c.POINTER(c.c_uint32),
        c.POINTER(c.c_uint32),
        c.c_uint32,
        c.c_uint,
        c.c_uint,
        c.POINTER(c.c_uint32),
    ]
launcher = args.launcher.read_bytes()
if launcher[:8] != b"HPHP49-C" or int.from_bytes(launcher[-4:], "little") != 0x3176B34C:
    raise ValueError("Expected Launcher V3 object")
request = int.from_bytes(launcher[-16:-12], "little")
if request != 0x1C00000:
    raise ValueError("Launcher memory request differs from reviewed MMU policy")
flash, ram = (args.state / "flash").read_bytes(), (args.state / "sram").read_bytes()
if len(flash) != 0x200000 or len(ram) != 0x80000:
    raise ValueError("Expected complete saved state")
mapping = native_words(ram, flash)
code = bytes(mapping(0x889A5 + i, 1) for i in range(0x88D67 - 0x889A5))
if (
    hashlib.sha256(code).hexdigest()
    != "d1300eb62a91f7f3d9659b7870805de2ec0fe113912ceeaa9815812cb9d936eb"
):
    raise ValueError("MMU launcher code differs from reviewed code")
if (
    args.descriptor
    and hashlib.sha256(bytes(mapping(0x8890D + i, 1) for i in range(0x98))).hexdigest()
    != "b3b580f89f6025bcc963445cac1ee038fb193daab97712c5c6d5a3937de76993"
):
    raise ValueError("Descriptor loader differs from reviewed code")
if (
    args.linked_descriptor
    and hashlib.sha256(bytes(mapping(0x88311 + i, 1) for i in range(0x581))).hexdigest()
    != "d2cdeac8cb3c85b27e21ee5097337648b8de384ea949d127d855de8ddb36cd47"
):
    raise ValueError("Descriptor producer differs from reviewed code")
cpu = 0x08000000 + (mapping.table - 128) * 4
rows = []
for cursor in range(8):
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(0, len(flash))
    uc.mem_write(0, flash)
    uc.mem_map(0x08000000, len(ram))
    uc.mem_write(0x08000000, ram)
    uc.mem_map(0x300000, 0x10000)

    def put(at, value, width=5):
        for i in range(width):
            p = mapping.physical(at + i)
            shift = 4 * ((at + i) & 1)
            old = uc.mem_read(p, 1)[0]
            uc.mem_write(p, bytes([(old & ~(15 << shift)) | ((value >> (4 * i) & 15) << shift)]))

    def word(at, width=5):
        return sum(
            ((uc.mem_read(mapping.physical(at + i), 1)[0] >> (4 * ((at + i) & 1))) & 15) << (4 * i)
            for i in range(width)
        )

    launch_pair = (0x08050000 << 32) | request
    code_pair = (0x08048000 << 32) | 0x1000
    descriptor = 0x90000
    arm_entry = 0x08050000
    producer_traces = []
    resolved_entries = []
    if args.linked_descriptor:
        assert 0x90000 + (len(launcher) - 8) * 2 < word(0x806F8)
        for i, value in enumerate(launcher[8:]):
            uc.mem_write(mapping.physical(0x90000 + 2 * i), bytes([value]))
        for off, value in ((0x95C, 0), (0x970, 0), (0xC24, 0), (0x998, cursor)):
            uc.mem_write(cpu + off, struct.pack("<I", value))
        uc.mem_write(cpu + 0x958, struct.pack("<I", 0x90000))
        uc.mem_write(cpu + 0x94C, struct.pack("<Q", 0x26C30))
        uc.reg_write(UC_ARM_REG_R4, cpu)
        uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
        uc.reg_write(UC_ARM_REG_SP, 0x308000)

        def segment(entry, end):
            path = []

            def watch(_, at, size, user):
                if at == 0x28898:
                    pc = uc.reg_read(UC_ARM_REG_R7)
                    path.append(pc)
                    if args.full_link and pc == 0x8840D:
                        resolved_entries.append(
                            int.from_bytes(uc.mem_read(cpu + 0x910, 4), "little")
                        )
                    if pc == end or len(path) > 30000:
                        uc.emu_stop()

            uc.mem_write(cpu + 0x998, struct.pack("<I", cursor))
            uc.mem_write(cpu + 0x978 + 4 * ((cursor + 7) & 7), struct.pack("<I", 0x70000))
            uc.reg_write(UC_ARM_REG_R7, entry)
            handle = uc.hook_add(UC_HOOK_CODE, watch)
            uc.emu_start(0x28828, 0x400000, count=3000000)
            uc.hook_del(handle)
            assert path[-1] == end
            producer_traces.append([hex(pc) for pc in path])

        def read_reg(offset):
            return int.from_bytes(uc.mem_read(cpu + offset, 8), "little")

        if args.full_link:
            if (mapping(0x882F7), mapping(0x882FC, 3), mapping(0x882FF, 3)) != (0x2E92, 275, 11):
                raise ValueError("PrLNK launcher reference differs from reviewed XLIB")
            put(0x8F040, 0x2911)
            put(0x8F045, 0xFFFFF)
            stack = word(0x806F8) - 15
            put(0x806F8, stack)
            put(0x80E9B, word(0x80E9B) - 3)
            put(stack, 0x8F040)
            put(stack + 5, 0x90000)
            put(stack + 10, 0x882F7)
            uc.mem_write(cpu + 0x958, struct.pack("<I", stack))
            uc.mem_write(cpu + 0x954, struct.pack("<I", 0x88D67))
            uc.mem_write(cpu + 0x914, struct.pack("<Q", word(0x806F3)))
            uc.mem_write(cpu + 0x924, struct.pack("<Q", word(0x80E9B)))
            segment(0x88311, 0x5143)
            descriptor = word(word(0x806F8))
            assert word(descriptor) == 0x2A4E and word(descriptor + 5) == 45
            launch_pair, code_pair = word(descriptor + 10, 16), word(descriptor + 26, 16)
            arm_entry = word(descriptor + 42, 8)
            assert launch_pair == (0x0804800C << 32) | request
            assert code_pair == (0x0804800C << 32) | ((len(launcher) - 20) * 2)
            assert resolved_entries == [arm_entry] and arm_entry != 0x08050000
            assert all(
                hex(at) in producer_traces[-1]
                for at in (0x8838F, 0x883DC, 0x88512, 0x88406, 0x88431)
            )
        else:
            segment(0x885F1, 0x70000)

            assert read_reg(0x970) & 0xFFFFFFFF == 0
            assert read_reg(0x90C) & 0xFFFFF == 0x90018
            assert read_reg(0x914) & 0xFFFFF == (len(launcher) - 20) * 2
            uc.mem_write(cpu + 0x91C, struct.pack("<Q", 0xFFFFF))
            segment(0x8879A, 0x70000)
            assert read_reg(0x970) & 0xFFFFFFFF == 0
            launch_pair, code_pair = read_reg(0x90C), read_reg(0x924)
            assert launch_pair == (0x0804800C << 32) | request
            assert code_pair == (0x0804800C << 32) | ((len(launcher) - 20) * 2)
            segment(0x88378, 0x883BA)
            descriptor = read_reg(0x92C) & 0xFFFFF
            assert word(descriptor) == 0x2A4E and word(descriptor + 5) == 45
            for offset, value in ((10, launch_pair), (26, code_pair)):
                assert all(
                    (
                        uc.mem_read(mapping.physical(descriptor + offset + i), 1)[0]
                        >> (4 * ((descriptor + offset + i) & 1))
                    )
                    & 15
                    == (value >> (4 * i)) & 15
                    for i in range(16)
                )
    if not args.full_link:
        stack = word(0x806F8) - 5
        put(0x806F8, stack)
        put(0x80E9B, word(0x80E9B) - 1)
        put(stack, 0x88903)
    assert word(0x806F8) <= word(0x806FD)
    assert (word(0x806FD) - word(0x806F8)) % 5 == 0
    rpl_return_stack = word(0x806F3)
    put(0x8076B, 0x88D67)
    for off, value in [(0x95C, 0), (0x970, 0), (0xC24, 0), (0x998, cursor)]:
        uc.mem_write(cpu + off, struct.pack("<I", value))
    for i in range(9):
        uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0x123456789AB00000 + i))
    uc.mem_write(cpu + 0x90C, struct.pack("<Q", (0x08050000 << 32) | request))
    uc.mem_write(cpu + 0x924, struct.pack("<Q", (0x08048000 << 32) | 0x1000))
    uc.mem_write(cpu + 0x914, struct.pack("<Q", (0x08050000 << 32) | (1 << 20) | 32))
    uc.mem_write(cpu + 0x92C, struct.pack("<Q", 0xABCDEF1208050000))
    uc.mem_write(cpu + 0x93C, struct.pack("<Q", 0x76543210ABCDE001))
    for i in range(8):
        uc.mem_write(cpu + 0x978 + 4 * i, struct.pack("<I", 0xA1000 + 0x101 * i))
    if args.descriptor:
        for off in (0x90C, 0x91C, 0x92C):
            uc.mem_write(cpu + off, struct.pack("<Q", 0xDEADBEEF01234567))
        if not args.linked_descriptor:
            put(descriptor, 0x2A4E)
            put(descriptor + 5, 45)
            put(descriptor + 10, launch_pair, 16)
            put(descriptor + 26, code_pair, 16)
        if not args.full_link:
            put(descriptor + 42, arm_entry, 8)
        uc.mem_write(cpu + 0x914, struct.pack("<Q", rpl_return_stack))
        stack = word(0x806F8)
        put(stack, descriptor)
        uc.mem_write(cpu + 0x954, struct.pack("<I", 0x88D67))
        uc.mem_write(cpu + 0x958, struct.pack("<I", stack))
        uc.mem_write(cpu + 0x924, struct.pack("<Q", word(0x80E9B)))
    uc.reg_write(UC_ARM_REG_R4, cpu)
    uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
    uc.reg_write(UC_ARM_REG_R7, 0x8890D if args.descriptor else 0x889A5)
    uc.reg_write(UC_ARM_REG_SP, 0x308000)
    if not args.descriptor:
        uc.mem_write(cpu + 0x978 + 4 * ((cursor + 7) & 7), struct.pack("<I", 0x88985))
    trace = []
    accesses = []
    depth = [0]
    peak = [0]

    def hook(_, at, size, user):
        if at == 0x28898:
            pc = uc.reg_read(UC_ARM_REG_R7)
            trace.append(pc)
            if args.descriptor and pc == 0x8891C:
                assert word(0x806F3) == rpl_return_stack, (
                    "SAVPTR corrupted RPL return-stack position"
                )
            if pc in (0x88B43, 0x88C6E, 0x88AC4) or len(trace) > 30000:
                uc.emu_stop()

    def access(_, kind, at, size, value, user):
        if kind != UC_MEM_READ and at == cpu + 0x998:
            step = (value - int.from_bytes(uc.mem_read(at, 4), "little")) & 7
            assert step in (1, 7)
            depth[0] += 1 if step == 1 else -1
            peak[0] = max(peak[0], depth[0])
        if at < cpu + 0x954 and at + size > cpu + 0x92C:
            accesses.append((kind, at - cpu, size))

    uc.hook_add(UC_HOOK_CODE, hook)
    uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, access)
    uc.emu_start(0x28828, 0x400000, count=3000000)
    assert trace[-1] == 0x88B43
    assert depth[0] == int(args.descriptor) and peak[0] == 3 + int(args.descriptor)
    regs = [int.from_bytes(uc.mem_read(cpu + 0x90C + 8 * i, 8), "little") for i in range(9)]
    assert regs[4] == (
        ((code_pair >> 32) << 32) | arm_entry if args.descriptor else 0xABCDEF1208050000
    )
    assert regs[5] == code_pair
    assert regs[7] == launch_pair
    assert regs[8] == 0x123456789AB26C30
    assert regs[6] & ~0xFFF == 0x76543210ABCDE000
    assert regs[1] == (regs[6] & 0xFFF) // 2 and regs[1] > 0
    assert regs[0] == regs[7] and regs[2] & 0xFFFFFFFF == arm_entry
    assert regs[2] >> 32 & 0xFFF == 0
    if args.descriptor:
        assert all(pc in trace for pc in (0x8890D, 0x88922, 0x88929, 0x88933, 0x88937, 0x88981))
        assert word(descriptor) == 0x2A4E and word(descriptor + 5) == 45
    entry_trace = trace.copy()
    resume = c.c_uint32(0x88B48)
    writes = c.c_uint32()
    delta = 16 if args.relocate else 0
    if args.relocate:
        current_cursor = int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little")
        caller_slot = (current_cursor + 7) & 7
        caller_at = cpu + 0x978 + 4 * caller_slot
        caller = c.c_uint32(int.from_bytes(uc.mem_read(caller_at, 4), "little"))
        assert caller.value == 0x88985
        nibbles = bytearray(
            n
            for page in range(128, 256)
            for value in uc.mem_read(mapping.physical(page << 12), 2048)
            for n in (value & 15, value >> 4)
        )
        array = (c.c_uint8 * len(nibbles)).from_buffer(nibbles)
        status = lib.bank_catalog_grow(
            array, c.byref(caller), c.byref(resume), 0x86500, 0, current_cursor, c.byref(writes)
        )
        assert status == 0, (status, writes.value, hex(word(0x806F3)), hex(word(0x806F8)))
        assert writes.value and caller.value == 0x88995 and resume.value == 0x88B58
        for page in range(128, 256):
            first = (page - 128) * 4096
            uc.mem_write(
                mapping.physical(page << 12),
                bytes(nibbles[i] | nibbles[i + 1] << 4 for i in range(first, first + 4096, 2)),
            )
        uc.mem_write(caller_at, struct.pack("<I", caller.value))
        assert word(0x8076B) == 0x88D67 + delta
    saved = bytes(uc.mem_read(cpu + 0x92C, 40))
    accesses.clear()
    trace.clear()

    # Existing code hook stops at pre-ARMSYS only; stop at ROM dispatch additionally.
    def stop(_, at, size, user):
        if at == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
            uc.emu_stop()

    uc.hook_add(UC_HOOK_CODE, stop)
    uc.reg_write(UC_ARM_REG_R7, resume.value)
    uc.emu_start(0x28828, 0x400000, count=300000)
    assert trace[-1] == 0x3130, trace[-10:]
    assert not accesses and bytes(uc.mem_read(cpu + 0x92C, 40)) == saved
    assert word(0x806F3) == rpl_return_stack + delta
    assert word(0x806F8) <= word(0x806FD)
    assert (word(0x806FD) - word(0x806F8)) % 5 == 0
    assert depth[0] == (-1 + int(args.descriptor)) and peak[0] == 3 + int(args.descriptor)
    assert (
        int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little")
        == (cursor + (0 if args.descriptor else 7)) & 7
    )
    used = {
        (cursor + offset) & 7 for offset in ((0, 1, 2, 3) if args.descriptor else (-1, 0, 1, 2))
    }
    for i in range(8):
        assert int.from_bytes(uc.mem_read(cpu + 0x978 + 4 * i, 4), "little") == (
            0 if i in used else 0xA1000 + 0x101 * i
        )
    rows.append(
        dict(
            rpl_return_stack=hex(rpl_return_stack),
            descriptor=args.descriptor,
            producer_traces=producer_traces,
            full_link=args.full_link,
            relocation_writes=writes.value,
            arm_entry=hex(arm_entry),
            cursor=cursor,
            depth=depth[0],
            peak=peak[0],
            registers=regs,
            entry_trace=[hex(x) for x in entry_trace],
            return_trace=[hex(x) for x in trace],
        )
    )
assert all(
    hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
    for path, digest in sources.items()
)
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            sources=sources,
            inputs={
                "sram": hashlib.sha256(ram).hexdigest(),
                "flash": hashlib.sha256(flash).hexdigest(),
                "launcher": hashlib.sha256(launcher).hexdigest(),
                "driver": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            },
            rows=rows,
        ),
        indent=2,
    )
    + "\n"
)
print(args.model, "PASS eight MMU entry/return cursors; descriptor=", args.descriptor)
