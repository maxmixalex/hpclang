"""Trace-derived saved-IP state, actual catalog grow, original GETPTR/dispatch."""

import argparse
import ctypes as c
import hashlib
import importlib
import json
import struct
import subprocess
import sys
from pathlib import Path
from unicorn import (
    Uc,
    UcError,
    UC_ARCH_ARM,
    UC_MODE_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_WRITE,
)
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

if not __debug__:
    raise RuntimeError("Verification requires Python assertions enabled")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True, help="directory containing sram and flash")
parser.add_argument("--trace", type=Path, required=True, help="normal-return register trace")
parser.add_argument("--model", choices=("hp50g", "hp49gplus"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words

OUT = args.output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
RAM = 0x08000000
(OUT / "getptr-move.json").unlink(missing_ok=True)
base = root / "runtime/core"
names = [
    "rpl-mutation.c",
    "rpl-object-store.c",
    "rpl-stack-view.c",
    "object-snapshot.c",
    "rpl-object-span.c",
]
stage = root / "platform/armtoolbox/stage-stack.c"
stage_hash = hashlib.sha256(stage.read_bytes()).hexdigest()
shim = Path(__file__).with_name("threaded-return.c")
source_hashes = {name: hashlib.sha256((base / name).read_bytes()).hexdigest() for name in names}
shim_hash = hashlib.sha256(shim.read_bytes()).hexdigest()
driver_hash = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
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
        str(stage),
        "-I" + str(root / "include"),
        str(shim),
        *[str(base / n) for n in names],
        "-o",
        str(OUT / "geometry.so"),
    ],
    check=True,
)
lib = c.CDLL(str(OUT / "geometry.so"))
lib.bank_catalog_grow.argtypes = [
    c.POINTER(c.c_uint8),
    c.POINTER(c.c_uint32),
    c.POINTER(c.c_uint32),
    c.c_uint32,
    c.c_uint,
    c.c_uint,
    c.POINTER(c.c_uint32),
]


def grow(data, root, entry, position, mode, cursor):
    array = (c.c_uint8 * len(data)).from_buffer_copy(data)
    native = c.c_uint32(root)
    pc = c.c_uint32(entry)
    writes = c.c_uint32()
    status = lib.bank_catalog_grow(
        array, c.byref(native), c.byref(pc), position, mode, cursor, c.byref(writes)
    )
    return status, bytes(array), native.value, pc.value, writes.value


rows = []
negatives = []
# This gate deliberately pins the previously reviewed command-4 layout.
# A different ROM/cache requires review, not silently adapting guessed offsets.
thread = 0x88D67
model = args.model
state = args.state
flash, ram = (state / "flash").read_bytes(), (state / "sram").read_bytes()
if len(flash) != 0x200000 or len(ram) != 0x80000:
    raise ValueError("Expected full pinned SRAM/Flash images")
mapping = native_words(ram, flash)
cpu = RAM + (mapping.table - 128) * 4


def encoding(at, n):
    return "".join(format(mapping(at + i, 1), "x") for i in range(n))


if encoding(0x67F7, 16) != "1bb6708146134031" or encoding(0x8899B, 10) != "142164808c":
    raise ValueError("GETPTR/dispatch code differs from reviewed layout")
if mapping(thread) != 0x312B or mapping(0x312B) != 0x3130:
    raise ValueError("Thread/ROM entry differs from reviewed normal return")
if encoding(0x88B48, 2) != "03" or encoding(0x88985, 22) != "5908dbd6628f2d760174e7":
    raise ValueError("ARM return/caller code differs from reviewed layout")
if encoding(0x67D2, 52) != "1bb9e08146d71b3f608146d51b8f6081461351bb670814613403":
    raise ValueError("Full GETPTR code differs from reviewed layout")
events = []
return_pop = False
trace_bytes = args.trace.read_bytes()
for line in trace_bytes.decode().splitlines():
    p = line.split()
    if len(p) == 25 and p[0] == "access" and p[8] == "regs":
        if (
            int(p[5], 16) == int(p[7], 16) == 0x88985
            and int(p[16], 16) == 0x88B48
            and int(p[24], 16) == 0x3C8F0
        ):
            return_pop = True
    if len(p) == 10 and p[0] == "register":
        events.append(tuple(int(p[i], 16) for i in (1, 3, 5, 7)))
if not return_pop:
    raise ValueError("Trace lacks original ARM-return stack consumption")
expected = [
    (0x970, 0, 1, 0x88985),
    (0x954, 0x80E9B, 2, 0x67D2),
    (0x954, 0x8076B, 2, 0x67F7),
    (0x954, 0x8076B, 1, 0x6801),
    (0x91C, 0x188D67, 2, 5),
    (0x954, thread, 2, 0x6804),
    (0x954, thread, 1, 0x8899E),
    (0x90C, 0x1C0312B, 2, 1),
    (0x954, thread + 5, 2, 0x8899E),
    (0x90C, 0x1C0312B, 1, 0x889A1),
]
index = 0
for off, value, access, pc in expected:
    while index < len(events) and not (
        events[index][0] == off
        and events[index][1] == value
        and events[index][2] & access
        and events[index][3] == pc
    ):
        index += 1
    if index == len(events):
        raise ValueError("Normal-return trace lacks required ordered provenance")
    index += 1
if index >= len(events) or events[index][3] != 0x3130:
    raise ValueError("Trace lacks final ROM dispatch")
for entry_kind in ("getptr-tail", "arm-return"):
    for cursor in range(8):
        baseline = None
        for mode in ("baseline", "move", "stale-ip", "stale-return") + (
            ("stale-pc",) if entry_kind == "arm-return" else ()
        ):
            uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
            uc.mem_map(0, len(flash))
            uc.mem_write(0, flash)
            uc.mem_map(RAM, len(ram))
            uc.mem_write(RAM, ram)
            uc.mem_map(0x300000, 0x10000)

            def put(at, value):
                for i in range(5):
                    p = mapping.physical(at + i)
                    shift = 4 * ((at + i) & 1)
                    old = uc.mem_read(p, 1)[0]
                    uc.mem_write(
                        p,
                        bytes([(old & ~(15 << shift)) | ((value >> (4 * i) & 15) << shift)]),
                    )

            def word(at):
                return sum(
                    (uc.mem_read(mapping.physical(at + i), 1)[0] >> (4 * ((at + i) & 1)) & 15)
                    << (4 * i)
                    for i in range(5)
                )

            # Only the saved IP is reconstructed from the observed trace; code/SEMI/ROM unchanged.
            put(0x8076B, thread)
            ret = 0x88996 if entry_kind == "getptr-tail" else 0x88985
            entry = 0x67F7 if entry_kind == "getptr-tail" else 0x88B48
            writes = 0
            if mode != "baseline":
                main = bytearray()
                for page in range(128, 256):
                    main.extend(
                        n
                        for value in uc.mem_read(mapping.physical(page << 12), 2048)
                        for n in (value & 15, value >> 4)
                    )
                status, changed, repaired, entry, writes = grow(
                    main,
                    ret,
                    entry,
                    0x86500,
                    1 if mode == "stale-return" else 2 if mode == "stale-pc" else 0,
                    cursor,
                )
                assert status == 0
                for page in range(128, 256):
                    lo = (page - 128) * 4096
                    uc.mem_write(
                        mapping.physical(page << 12),
                        bytes(changed[i] | changed[i + 1] << 4 for i in range(lo, lo + 4096, 2)),
                    )
                ret = repaired
                assert word(0x8076B) == thread + 16 and word(thread + 16) == 0x312B
                if mode == "stale-ip":
                    put(0x8076B, thread)
            expected_b = word(0x806F3)
            expected_d = word(0x80E9B) + 1
            expected_d1 = word(0x806F8) + 5
            for off, value in (
                (0x954, 0x8076B),
                (0x958, 0xE1C86),
                (0x924, 0x11C91),
                (0x970, 1 if entry_kind == "arm-return" else 0),
                (0x95C, 0),
                (0xC24, 0),
                (0x998, cursor),
            ):
                uc.mem_write(cpu + off, struct.pack("<I", value))
            uc.mem_write(cpu + 0x90C, struct.pack("<Q", 0x1234567801C00000))
            for i in range(8):
                uc.mem_write(
                    cpu + 0x978 + 4 * i,
                    struct.pack("<I", ret if i == (cursor - 1) & 7 else 0xA1000 + 0x101 * i),
                )
            uc.reg_write(UC_ARM_REG_R4, cpu)
            uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
            uc.reg_write(UC_ARM_REG_R7, entry)
            uc.reg_write(UC_ARM_REG_SP, 0x308000)
            trace = []
            stack_before = bytes(uc.mem_read(cpu + 0x978, 32))
            stack_steps = []
            saved_accesses = []

            def register_access(_, access, at, size, value, userdata):
                if at < cpu + 0x954 and at + size > cpu + 0x92C:
                    saved_accesses.append((access, at, size))

            def stack_write(_, access, at, size, value, userdata):
                if at == cpu + 0x998:
                    assert size == 4 and value < 8
                    previous = int.from_bytes(uc.mem_read(at, size), "little")
                    delta = (value - previous) & 7
                    assert delta in (1, 7)
                    stack_steps.append(1 if delta == 1 else -1)

            def hook(_, at, size, userdata):
                if at == 0x28898:
                    pc = uc.reg_read(UC_ARM_REG_R7)
                    trace.append(pc)
                    if pc == 0x3130 or len(trace) >= 40:
                        uc.emu_stop()

            uc.hook_add(UC_HOOK_CODE, hook)
            uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, register_access)
            uc.hook_add(UC_HOOK_MEM_WRITE, stack_write)
            error = None
            try:
                uc.emu_start(0x28828, 0x400000, count=20000)
            except UcError as exc:
                error = str(exc)
            d0 = int.from_bytes(uc.mem_read(cpu + 0x954, 4), "little")
            a = int.from_bytes(uc.mem_read(cpu + 0x90C, 8), "little")
            row = dict(
                model=model,
                entry_kind=entry_kind,
                entry=hex(entry),
                cursor=cursor,
                mode=mode,
                writes=writes,
                trace=[hex(x) for x in trace],
                d0=hex(d0),
                a=hex(a),
                error=error,
            )
            if mode in ("baseline", "move"):
                assert not saved_accesses, (row, saved_accesses)
                depth = low = high = 0
                for delta in stack_steps:
                    depth += delta
                    low, high = min(low, depth), max(high, depth)
                assert (depth, low, high) == (-1, -1, 0), (row, stack_steps)
                stack_after = bytes(uc.mem_read(cpu + 0x978, 32))
                for slot in range(8):
                    expected_slot = (
                        bytes(4)
                        if slot == (cursor - 1) & 7
                        else stack_before[4 * slot : 4 * slot + 4]
                    )
                    assert stack_after[4 * slot : 4 * slot + 4] == expected_slot, row
                row.update(
                    saved_register_accesses=0,
                    stack_depth_delta=depth,
                    stack_depth_minimum=low,
                    stack_depth_maximum=high,
                )
            if mode in ("baseline", "move") and entry_kind == "arm-return":
                assert (
                    int.from_bytes(uc.mem_read(cpu + 0x914, 4), "little") & 0xFFFFF == expected_b
                ), row
                assert (
                    int.from_bytes(uc.mem_read(cpu + 0x924, 4), "little") & 0xFFFFF == expected_d
                ), row
                assert int.from_bytes(uc.mem_read(cpu + 0x958, 4), "little") == expected_d1, row
                assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == (
                    (cursor - 1) & 7
                ), row
            if mode == "baseline":
                assert (
                    not error
                    and trace[-1] == 0x3130
                    and d0 == thread + 5
                    and a == 0x1234567801C0312B
                ), row
                if entry_kind == "arm-return":
                    assert trace == [
                        0x88B48,
                        0x88985,
                        0x8898F,
                        0x67D2,
                        0x67D9,
                        0x67DC,
                        0x67DE,
                        0x67E5,
                        0x67E8,
                        0x67EA,
                        0x67F1,
                        0x67F4,
                        0x67F7,
                        0x67FE,
                        0x6801,
                        0x6804,
                        0x88996,
                        0x88999,
                        0x8899B,
                        0x8899E,
                        0x889A1,
                        0x3130,
                    ], row
                baseline = trace
                rows.append(row)
            else:
                expected = [x + 16 if 0x86500 <= x < 0x894CF else x for x in baseline]
                passed = (
                    not error
                    and trace == expected
                    and d0 == thread + 21
                    and a == 0x1234567801C0312B
                )
                if mode == "move":
                    assert passed, row
                    rows.append(row)
                else:
                    assert not passed, row
                    negatives.append(row)
assert source_hashes == {
    name: hashlib.sha256((base / name).read_bytes()).hexdigest() for name in names
}
assert stage_hash == hashlib.sha256(stage.read_bytes()).hexdigest()
assert shim_hash == hashlib.sha256(shim.read_bytes()).hexdigest()
assert driver_hash == hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
report = dict(
    cases=rows,
    omissions=negatives,
    source_sha256=source_hashes,
    inputs={
        str(p): hashlib.sha256(data).hexdigest()
        for p, data in ((args.trace, trace_bytes), (state / "sram", ram), (state / "flash", flash))
    },
    stage_sha256=stage_hash,
    shim_sha256=shim_hash,
    driver_sha256=driver_hash,
    scope="Saved code/data with saved-IP reconstructed from trace and controlled CPU/RSTK; actual C geometry then original GETPTR tail or post-ARMSYS RTNCC, condition, full GETPTR and dispatch. The production stage return-root collector identifies the original caller cell before geometry changes. Both suspended PC and caller return are repaired by C; generated GETPTR returns come from moved code. Seven distinct other slots survive; one existing caller slot is consumed without extra stack capacity. R0..R4 are neither read nor written in accepted runs. This does not prove the earlier ARMJUMP path, arbitrary caller state, later Stock continuation, catalog publication or external-reference closure.",
)
(OUT / "getptr-move.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(dict(baseline_and_move=len(rows), omission_controls=len(negatives))))
