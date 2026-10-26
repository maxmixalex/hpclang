"""Original PrRUN preamble through MMU entry, skipping only the ARM payload."""

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
parser.add_argument("--model", choices=("hp50g", "hp49gplus"), required=True)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--launcher", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--relocate", action="store_true")
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
report = out / "prrun.json"
report.unlink(missing_ok=True)
flash = (args.state / "flash").read_bytes()
ram = (args.state / "sram").read_bytes()
if len(flash) != 0x200000 or len(ram) != 0x80000:
    raise ValueError("Expected complete SRAM/Flash")
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
if (
    len(launcher) < 32
    or launcher[:8] != b"HPHP49-C"
    or int.from_bytes(launcher[-16:-12], "little") != 0x1C00000
    or int.from_bytes(launcher[-4:], "little") != 0x3176B34C
):
    raise ValueError("Expected reviewed MMU launcher")
mapping = native_words(ram, flash)
cache = bytes(mapping(0x877D1 + i, 1) for i in range(0x88D6C - 0x877D1))
if (
    hashlib.sha256(cache).hexdigest()
    != "4b064bc5841a06b3c883e57ef85a3253e74899dc98d4808da8665af9ceb129fa"
):
    raise ValueError("PrRUN/cache layout differs from reviewed input")
assert 0x90000 + (len(launcher) - 8) * 2 < mapping(0x806F8) - 5
cpu = 0x08000000 + (mapping.table - 128) * 4
rows = []
for cursor in range(8):
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
            uc.mem_write(p, bytes([(old & ~(15 << shift)) | ((value >> (4 * i) & 15) << shift)]))

    def word(at):
        return sum(
            ((uc.mem_read(mapping.physical(at + i), 1)[0] >> (4 * ((at + i) & 1))) & 15) << (4 * i)
            for i in range(5)
        )

    put(0x8076B, 0x88D67)
    for off, value in [(0x95C, 0), (0x970, 0), (0xC24, 0), (0x998, cursor)]:
        uc.mem_write(cpu + off, struct.pack("<I", value))
    for i in range(9):
        uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0x123456789AB00000 + i))
    for i in range(8):
        uc.mem_write(cpu + 0x978 + 4 * i, struct.pack("<I", 0xA1000 + 0x101 * i))
    uc.reg_write(UC_ARM_REG_R4, cpu)
    uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
    uc.reg_write(UC_ARM_REG_R7, 0x2D9D)
    uc.reg_write(UC_ARM_REG_SP, 0x308000)
    for i, v in enumerate(launcher[8:]):
        uc.mem_write(mapping.physical(0x90000 + 2 * i), bytes([v]))
    stack = word(0x806F8) - 5
    put(0x806F8, stack)
    put(0x80E9B, word(0x80E9B) - 1)
    put(stack, 0x90000)
    uc.mem_write(cpu + 0x90C, struct.pack("<Q", 0x877D1))
    uc.mem_write(cpu + 0x958, struct.pack("<I", stack))
    uc.mem_write(cpu + 0x954, struct.pack("<I", 0x88D67))
    uc.mem_write(cpu + 0x924, struct.pack("<Q", word(0x80E9B)))
    uc.mem_write(cpu + 0x914, struct.pack("<Q", word(0x806F3)))
    trace = []
    accesses = []
    depth = 0
    peak = 0
    minimum = 0

    def hook(_, at, size, user):
        if at == 0x28898:
            pc = uc.reg_read(UC_ARM_REG_R7)
            trace.append(pc)
            if pc == 0x266DB or word(pc) == 0xEFB08 or len(trace) > 30000:
                uc.emu_stop()

    def access(_, kind, at, size, value, user):
        global depth, peak, minimum
        if kind != UC_MEM_READ and at == cpu + 0x998:
            step = (value - int.from_bytes(uc.mem_read(at, 4), "little")) & 7
            assert step in (1, 7), step
            depth += 1 if step == 1 else -1
            peak = max(peak, depth)
            minimum = min(minimum, depth)
        if at < cpu + 0x954 and at + size > cpu + 0x92C:
            accesses.append((kind, at - cpu, size))

    uc.hook_add(UC_HOOK_CODE, hook)
    uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, access)
    uc.emu_start(0x28828, 0x400000, count=3000000)
    assert word(trace[-1]) == 0xEFB08
    entry_trace = trace.copy()
    resume = trace[-1] + 5
    writes = c.c_uint32()
    original_resume = resume
    if args.relocate:
        current_cursor = int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little")
        caller_at = cpu + 0x978 + 4 * ((current_cursor + 7) & 7)
        original_caller = int.from_bytes(uc.mem_read(caller_at, 4), "little")
        caller = c.c_uint32(original_caller)
        pc = c.c_uint32(resume)
        original_ip = word(0x8076B)
        nibbles = bytearray(
            n
            for page in range(128, 256)
            for value in uc.mem_read(mapping.physical(page << 12), 2048)
            for n in (value & 15, value >> 4)
        )
        array = (c.c_uint8 * len(nibbles)).from_buffer(nibbles)
        status = lib.bank_catalog_grow(
            array, c.byref(caller), c.byref(pc), 0x86500, 0, current_cursor, c.byref(writes)
        )
        assert status == 0, (status, writes.value)
        assert (
            writes.value
            and caller.value == original_caller + 16
            and pc.value == original_resume + 16
        )
        for page in range(128, 256):
            first = (page - 128) * 4096
            uc.mem_write(
                mapping.physical(page << 12),
                bytes(nibbles[i] | nibbles[i + 1] << 4 for i in range(first, first + 4096, 2)),
            )
        uc.mem_write(caller_at, struct.pack("<I", caller.value))
        assert word(0x8076B) == original_ip + 16
        resume = pc.value
    saved = bytes(uc.mem_read(cpu + 0x92C, 40))
    trace.clear()
    accesses.clear()

    def stop(_, at, size, user):
        if at == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
            uc.emu_stop()

    uc.hook_add(UC_HOOK_CODE, stop)
    uc.reg_write(UC_ARM_REG_R7, resume)
    uc.emu_start(0x28828, 0x400000, count=300000)
    assert trace[-1] == 0x3130, trace[-12:]
    assert not accesses and bytes(uc.mem_read(cpu + 0x92C, 40)) == saved
    assert (depth, peak, minimum) == (0, 4, 0)
    assert word(0x806F8) <= word(0x806FD) and (word(0x806FD) - word(0x806F8)) % 5 == 0
    used = {(cursor + i) & 7 for i in range(4)}
    for i in range(8):
        assert int.from_bytes(uc.mem_read(cpu + 0x978 + 4 * i, 4), "little") == (
            0 if i in used else 0xA1000 + 0x101 * i
        )
    rows.append(
        dict(
            cursor=cursor,
            original_resume=hex(original_resume),
            resume=hex(resume),
            relocation_writes=writes.value,
            stack_peak=peak,
            stack_final=depth,
            trace=[hex(x) for x in trace],
            entry_trace=[hex(x) for x in entry_trace],
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
print(args.model, "PASS eight original PrRUN entry/return cursors; ARM payload skipped")
