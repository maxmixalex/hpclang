"""Compare C library-header resolution with genuine HOME attachment records."""

import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

if not __debug__:
    raise RuntimeError("Assertions required")
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
from verify import native_words  # noqa: E402

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", action="append", required=True, help="hp50g:PATH or hp49gplus:PATH")
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
report = args.output / "attachments.json"
report.unlink(missing_ok=True)
base = root / "runtime/core"
files = [base / name for name in ("rpl-port.c", "rpl-backup.c", "rpl-object-span.c")]
sources = {
    str(p): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [*files, base / "rpl-port.h", Path(__file__)]
}
shared = args.output / "attachments.so"
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
        "-I" + str(root / "include"),
        *map(str, files),
        "-o",
        str(shared),
    ],
    check=True,
)
lib = c.CDLL(str(shared.resolve()))
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


class Targets(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("present", "hash", "messages")]


lib.hpclang_rpl_library_attachment_resolve.argtypes = [
    c.POINTER(Reader),
    *([c.c_uint32] * 4),
    c.POINTER(Targets),
]
rows = []
for item in args.state:
    model, path = item.split(":", 1)
    if model not in ("hp50g", "hp49gplus"):
        parser.error("Only reviewed HP49g+/HP50g profiles are supported")
    state = Path(path)
    ram, flash = (state / "sram").read_bytes(), (state / "flash").read_bytes()
    assert len(ram) == 0x80000 and len(flash) == 0x200000
    read = native_words(ram, flash)
    cpu = 0x08000000 + (read.table - 128) * 4
    catalog = {}
    for index in range(read(0x8611D, 3)):
        at = 0x86120 + 16 * index
        number = read(at, 3)
        assert number not in catalog
        catalog[number] = read(at + 3), read(at + 8)
    home = read(0x80711)
    assert read(home) == 0x2A96
    count = read(home + 5, 3)
    assert 0 < count <= (0x100000 - home - 8) // 13
    for index in range(count):
        at = home + 8 + 13 * index
        number = read(at, 3)
        header, switch = catalog[number]
        mapped = read
        if switch:
            uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
            for address, data in ((0, flash), (0x08000000, ram)):
                uc.mem_map(address, len(data))
                uc.mem_write(address, data)
            uc.mem_map(0x300000, 0x10000)
            uc.mem_write(cpu + 0x998, struct.pack("<I", 1))
            uc.mem_write(cpu + 0x978, struct.pack("<I", 0x3130))
            for register, value in (
                (UC_ARM_REG_R4, cpu),
                (UC_ARM_REG_R5, 0xFFFFF),
                (UC_ARM_REG_R7, switch),
                (UC_ARM_REG_SP, 0x308000),
            ):
                uc.reg_write(register, value)

            def stop(_, pc, size, user):
                if pc == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
                    uc.emu_stop()

            uc.hook_add(UC_HOOK_CODE, stop)
            uc.emu_start(0x28828, 0x400000, count=100000)
            assert uc.reg_read(UC_ARM_REG_R7) == 0x3130
            mapped = native_words(bytes(uc.mem_read(0x08000000, len(ram))), flash)
        corrupt = None

        @Read
        def nibble(_, address, out):
            out[0] = mapped(address, 1) ^ (address == corrupt)
            return 1

        reader = Reader(nibble, None)
        first, end = (0x40000, 0x80000) if switch else (0, 0x100000)

        def matches(targets):
            for bit, target, offset in ((1, targets.hash, 3), (2, targets.messages, 8)):
                actual = read(at + offset)
                if not targets.present & bit:
                    if actual:
                        return False
                elif switch:
                    if (
                        not actual
                        or read(actual) != 0x2BAA
                        or read(actual + 5) != target
                        or read(actual + 10) != switch
                    ):
                        return False
                elif actual != target:
                    return False
            return True

        result = Targets()
        assert (
            lib.hpclang_rpl_library_attachment_resolve(
                c.byref(reader), first, end, header, number, c.byref(result)
            )
            == 1
        )
        assert matches(result), (model, number)
        rejected = 0
        for corrupt in range(header, header + 13):
            changed = Targets(7, 0xDEAD, 0xBEEF)
            status = lib.hpclang_rpl_library_attachment_resolve(
                c.byref(reader), first, end, header, number, c.byref(changed)
            )
            assert status < 0 or not matches(changed), (model, number, corrupt)
            rejected += 1
        rows.append(
            dict(
                model=model,
                number=number,
                header=hex(header),
                bank_switch=hex(switch),
                present=result.present,
                hash=hex(result.hash),
                messages=hex(result.messages),
                rejected_header_corruptions=rejected,
                sram_sha256=hashlib.sha256(ram).hexdigest(),
                flash_sha256=hashlib.sha256(flash).hexdigest(),
            )
        )
assert all(
    hashlib.sha256(Path(p).read_bytes()).hexdigest() == digest for p, digest in sources.items()
)
report.write_text(json.dumps(dict(sources=sources, rows=rows), indent=2) + "\n")
print(
    f"PASS {len(rows)} original HOME attachments; {sum(r['rejected_header_corruptions'] for r in rows)} header corruptions rejected"
)
