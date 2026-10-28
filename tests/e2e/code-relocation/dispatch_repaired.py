"""Actual catalog growth and original-ROM continuation with controlled A/R1 repairs.

Input is the RAM contrast output from dispatch_capacity.py; this validates
caller-proven repairs, not automatic discovery or Stock GC interception.
"""

from pathlib import Path
import argparse
import hashlib
import ctypes as c
import importlib
import json
import struct
import subprocess
import sys
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--state",
    type=Path,
    required=True,
    help="original Stock predicate snapshot, supplies flash",
)
parser.add_argument(
    "--prepared-sram",
    type=Path,
    required=True,
    help="dispatch_capacity.py --ram-interior final SRAM",
)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if not __debug__:
    raise RuntimeError("Verification requires assertions enabled")
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output
out.mkdir(parents=True, exist_ok=True)
(out / "results.json").unlink(missing_ok=True)
base = root / "runtime/core"
names = [
    "rpl-mutation.c",
    "rpl-object-store.c",
    "rpl-stack-view.c",
    "object-snapshot.c",
    "rpl-object-span.c",
]
subprocess.run(
    [
        "cc",
        "-shared",
        "-fPIC",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I" + str(base),
        "-I" + str(root / "include"),
        "-I" + str(root / "platform/armtoolbox"),
        str(root / "platform/armtoolbox/stage-stack.c"),
        str(root / "tests/e2e/code-relocation/threaded-return.c"),
        *[str(base / n) for n in names],
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
rows = []
for model in (args.model,):
    flash = (args.state / "flash").read_bytes()
    saved = args.prepared_sram.read_bytes()
    for mask in range(4):
        ram = bytearray(saved)
        mapping = native_words(ram, flash)
        cpu = 0x08000000 + (mapping.table - 128) * 4
        top = mapping(0x806F8)
        obj = mapping(top)
        header = obj + 10
        assert mapping(obj) == 0x2A2C and mapping(obj + 5) == 21
        assert mapping(header) == 0x37258 and mapping(header + 5) == 0x351CC
        before = bytes(mapping(at, 1) for at in range(0x80000, 0x100000))
        array = (c.c_uint8 * len(before)).from_buffer_copy(before)
        a, r1, writes = (
            c.c_uint32(0xA5B00000 | header + 5),
            c.c_uint32(0xC6D00000 | header + 5),
            c.c_uint32(),
        )
        status = lib.bank_catalog_grow_dispatch(
            array, c.byref(a), c.byref(r1), mask, c.byref(writes)
        )
        assert status == 0, (model, mask, status)
        changed = bytes(array)
        for at in range(0x80000, 0x100000, 2):
            physical = mapping.physical(at)
            ram[physical - 0x08000000] = (
                changed[at - 0x80000] | changed[at + 1 - 0x80000] << 4
            )
        for offset, v in (
            (0x90C, a.value),
            (0x934, r1.value),
            (0x914, mapping(0x806F3) + 16),
            (0x958, top),
            (0xC24, 0),
        ):
            struct.pack_into("<I", ram, cpu - 0x08000000 + offset, v)
        moved = native_words(ram, flash)
        assert moved(top) == obj + 16
        assert a.value == (0xA5B00000 | (header + 5 + (16 if mask & 1 else 0)))
        assert r1.value == (0xC6D00000 | (header + 5 + (16 if mask & 2 else 0)))
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        uc.mem_map(0, len(flash))
        uc.mem_write(0, flash)
        uc.mem_map(0x08000000, len(ram))
        uc.mem_write(0x08000000, bytes(ram))
        uc.mem_map(0x300000, 0x10000)
        for reg, v in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, struct.unpack_from("<I", flash, 0x296F8)[0]),
            (UC_ARM_REG_R7, 0x3727D),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(reg, v)
        trace = []

        def hook(uc, address, size, data):
            pc = uc.reg_read(UC_ARM_REG_R7)
            trace.append(pc)
            if (
                pc == 0x351E8
                or (len(trace) == 7 and pc != 0x351CC)
                or len(trace) >= 100
            ):
                uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, hook, begin=0x28898, end=0x28898)
        error = None
        try:
            uc.emu_start(0x28828, 0, count=100000)
        except UcError as exc:
            error = str(exc)
        actual = int.from_bytes(uc.mem_read(cpu + 0x90C, 4), "little") & 0xFFFFF
        wanted = int.from_bytes(uc.mem_read(cpu + 0x91C, 4), "little") & 0xFFFFF
        success = bool(trace and trace[-1] == 0x351E8 and actual == wanted == 0x2A2C)
        assert success == bool(mask & 2), (
            model,
            mask,
            trace,
            error,
            hex(actual),
            hex(wanted),
        )
        assert error is None
        assert trace[:6] == [0x3727D, 0x37283, 0x37263, 0x37266, 0x37269, 0x3726C]
        if not mask & 2:
            assert len(trace) == 7 and trace[-1] != 0x351CC, (
                "Wrong dispatch must be observed, not a timeout"
            )
        row = dict(
            model=model,
            mask=mask,
            status=status,
            writes=writes.value,
            trace=[hex(pc) for pc in trace],
            error=error,
            actual=actual,
            wanted=wanted,
            continuation_pass=success,
        )
        rows.append(row)
        print(
            model,
            mask,
            "continued",
            success,
            "trace",
            len(trace),
            "error",
            error,
            flush=True,
        )
sources = [base / n for n in names] + [
    base / "rpl-mutation.h",
    root / "platform/armtoolbox/stage-stack.c",
    root / "tests/e2e/code-relocation/threaded-return.c",
    Path(__file__),
    Path(importlib.import_module("verify").__file__),
]
report = dict(
    cases=rows,
    scope="Controlled catalog geometry transaction without catalog publication; original interpreter stopped at comparison or wrong dispatch. Does not establish full reference closure or public registration.",
    sources={
        str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sources
    },
    inputs={
        str(p): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (args.state / "flash", args.prepared_sram)
    },
    binary_sha256=hashlib.sha256((out / "geometry.so").read_bytes()).hexdigest(),
)
(out / "results.json").write_text(json.dumps(report, indent=2) + "\n")
