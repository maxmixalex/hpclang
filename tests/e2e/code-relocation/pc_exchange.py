"""Compare PC-exchange decoding with one instruction of the original interpreter.

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
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output
out.mkdir(parents=True, exist_ok=True)
report = out / "pc-exchange.json"
report.unlink(missing_ok=True)
base = root / "runtime/core"
files = [
    base / name
    for name in (
        "rpl-code.c",
        "rpl-reference.c",
        "rpl-backup.c",
        "rpl-port.c",
        "rpl-object-store.c",
        "rpl-object-span.c",
    )
]
subprocess.run(
    [
        args.cc,
        "-shared",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        *map(str, files),
        "-o",
        str(out / "code.so"),
    ],
    check=True,
)
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


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


class PCExchange(c.Structure):
    _fields_ = [("capture", Assignment), ("target", c.c_uint32)]


lib = c.CDLL(str(out / "code.so"))
inspect = lib.hpclang_rpl_code_pc_exchange_inspect
inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint32,
    c.c_uint32,
    c.POINTER(PCExchange),
]
flash = (args.state / "flash").read_bytes()
ram = (args.state / "sram").read_bytes()
mapping = native_words(ram, flash)
cpu = 0x08000000 + (mapping.table - 128) * 4
rows = []
for address in (0x90000, 0x90001, 0xFFFE0):
    for opcode in (6, 7):
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
            def read(_, at, value):
                if not address <= at < address + 4:
                    return 0
                value[0] = uc.mem_read(mapping.physical(at), 1)[0] >> (4 * (at & 1)) & 15
                return 1

            reader, exchange = Reader(read, None), PCExchange()
            a_low = struct.unpack_from("<I", before, 0)[0]
            c_low = struct.unpack_from("<I", before, 16)[0]
            assert inspect(c.byref(reader), address, 4, a_low, c_low, c.byref(exchange)) == 0
            assignment = exchange.capture
            assert exchange.target == ((a_low if opcode == 6 else c_low) & 0xFFFFF)
            assert (assignment.source, assignment.destination, assignment.nibbles) == (
                13,
                0 if opcode == 6 else 2,
                5,
            )
            assert assignment.constant == address + 4
            assert assignment.source_first == assignment.destination_first == 0
            assert assignment.instruction == address and assignment.length == 4
            offset = 8 * assignment.destination
            expected = bytearray(before)
            old = struct.unpack_from("<Q", expected, offset)[0]
            struct.pack_into("<Q", expected, offset, (old & ~0xFFFFF) | assignment.constant)
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
            assert trace == [address, exchange.target]
            assert bytes(uc.mem_read(cpu + 0x90C, len(before))) == expected
            rows.append(dict(address=address, opcode=opcode, p=p, carry=p & 1, cursor=p % 8))
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            cases=rows,
            sources={
                str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in files + [base / "rpl-code.h"]
            },
            driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            scope="One synthetic instruction; no reachability, wrap-boundary or relocation proof",
        ),
        indent=2,
    )
    + "\n"
)
print("PASS", args.model, len(rows), "PC exchange cases; unrelated registers/flags/stack preserved")
