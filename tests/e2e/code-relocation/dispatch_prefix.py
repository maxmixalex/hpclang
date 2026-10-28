"""Execute the reviewed predicate prefix in the original ARM Saturn interpreter."""

from pathlib import Path
import argparse
import subprocess
import ctypes as c
import hashlib
import importlib
import json
import struct
import sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
parser.add_argument(
    "--through-dispatch",
    action="store_true",
    help="continue through Boolean writeback and the next RPL dispatch",
)
parser.add_argument(
    "--duplicate-entry",
    action="store_true",
    help="start at the DUP/inline-entry handler with existing stack capacity",
)
args = parser.parse_args()
if args.duplicate_entry and not args.through_dispatch:
    parser.error("--duplicate-entry requires --through-dispatch")
if not __debug__:
    raise RuntimeError("Verification requires assertions enabled")
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words

OUT = args.output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "predicate-prefix.json").unlink(missing_ok=True)
base = root / "runtime/core"
names = (
    "rpl-code.c",
    "rpl-reference.c",
    "rpl-backup.c",
    "rpl-port.c",
    "rpl-object-store.c",
    "rpl-object-span.c",
)
subprocess.run(
    [
        args.cc,
        "-shared",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        *[str(base / n) for n in names],
        "-o",
        str(OUT / "code.so"),
    ],
    check=True,
)
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


class Exchange(c.Structure):
    _fields_ = [
        (k, c.c_uint32) for k in ("instruction", "length", "left", "right", "first", "nibbles")
    ]


class Assignment(c.Structure):
    _fields_ = [
        (k, c.c_uint32)
        for k in (
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


class Descriptor(c.Structure):
    _fields_ = [("dispatch", c.c_uint32), ("schema", c.c_int)]


class Edge(c.Structure):
    _fields_ = [("kind", c.c_int)] + [
        (name, c.c_uint32) for name in ("field", "target", "extent", "auxiliary")
    ]


Visit = c.CFUNCTYPE(c.c_int, c.c_void_p, c.POINTER(Edge))


lib = c.CDLL(str(OUT / "code.so"))
lib.hpclang_rpl_dispatch_reference_visit.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.POINTER(Descriptor),
    Visit,
    c.c_void_p,
]
for name, kind in [("exchange", Exchange), ("assignment", Assignment)]:
    getattr(lib, "hpclang_rpl_code_" + name + "_inspect").argtypes = [
        c.POINTER(Reader),
        c.c_uint32,
        c.c_uint32,
        c.c_uint,
        c.POINTER(kind),
    ]
TRACE = [
    0x351CC,
    0x351CF,
    0x351D2,
    0x351D5,
    0x351D8,
    0x351DA,
    0x351DD,
    0x351E0,
    0x351E3,
    0x351E6,
    0x351E8,
]
DUP_TRACE = [0x37258, 0x3725E, 0x37260, 0x37263, 0x37266, 0x37269, 0x3726C]
RAM = 0x08000000
HEADER, SLOT, OBJECT, D0 = 0x90000, 0x91000, 0x92000, 0x93000


def execute(model, cursor, wanted, actual, p, fault=None, carry=0, capacity=1):
    result_slot = SLOT - 5 if args.duplicate_entry else SLOT
    state = args.state
    flash = bytearray((state / "flash").read_bytes())
    ram = bytearray((state / "sram").read_bytes())
    mapping = native_words(ram, flash)
    offset = (mapping.table - 128) * 4
    cpu = RAM + offset

    def setword(at, value):
        struct.pack_into("<I", ram, offset + at, value)

    def put(at, value, count=5):
        for n in range(count):
            pos = at + n
            physical = mapping.physical(pos)
            shift = 4 * (pos & 1)
            assert RAM <= physical < RAM + len(ram) and not cpu <= physical < cpu + 0xD00
            i = physical - RAM
            ram[i] = (ram[i] & ~(15 << shift)) | (((value >> (4 * n)) & 15) << shift)

    if args.duplicate_entry:
        put(HEADER, 0x37258)
        put(HEADER + 5, 0x351CC)
        put(HEADER + 10, wanted)
    else:
        put(HEADER, 0x351CC)
        put(HEADER + 5, wanted)
    put(SLOT, OBJECT)
    put(OBJECT, actual)
    if args.through_dispatch:
        put(D0, 0x94000)
        put(0x94000, 0x94005)
    for at in range(0x90C, 0x954, 4):
        setword(at, 0xA5000000 + at)
    setword(0x90C, 0xA5100000 | HEADER)
    setword(0x91C, 0xC5200000 | 0x12345)
    if args.duplicate_entry:
        assert capacity in (1, 16)
        setword(0x924, 0xA5300000 | capacity)
    setword(0x954, D0)
    setword(0x958, SLOT)
    setword(0x95C, p)
    setword(0x970, carry)
    setword(0xC24, 0)
    for i in range(8):
        setword(0x978 + 4 * i, 0x70000 + i * 0x10)
    setword(0x998, cursor)
    if fault:
        at, mask = {
            "first-exchange": (0x351CE, 2),
            "restore-stack-address": (0x351E5, 4),
            "true-result": (0x35201, 1),
            "false-result": (0x35221, 1),
            "duplicate-depth": (0x37268, 1),
        }[fault]
        physical = mapping.physical(at)
        shift = 4 * (at & 1)
        assert physical < len(flash)
        flash[physical] ^= mask << shift
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(0, len(flash))
    uc.mem_write(0, bytes(flash))
    uc.mem_map(RAM, len(ram))
    uc.mem_write(RAM, bytes(ram))
    uc.mem_map(0x300000, 0x10000)
    uc.reg_write(UC_ARM_REG_SP, 0x308000)
    uc.reg_write(UC_ARM_REG_R4, cpu)
    uc.reg_write(UC_ARM_REG_R5, struct.unpack_from("<I", flash, 0x296F8)[0])
    uc.reg_write(UC_ARM_REG_R7, DUP_TRACE[0] if args.duplicate_entry else TRACE[0])

    def word(at):
        return int.from_bytes(uc.mem_read(cpu + at, 4), "little")

    def reg(index):
        at = {0: 0x90C, 2: 0x91C, 11: 0x954, 12: 0x958}[index]
        return int.from_bytes(uc.mem_read(cpu + at, 4 if index >= 11 else 8), "little")

    @Read
    def read(_, at, value):
        physical = mapping.physical(at)
        value[0] = uc.mem_read(physical, 1)[0] >> (4 * (at & 1)) & 15
        return 1

    reader = Reader(read, None)
    trace = []
    pending = {}
    scalar = []
    checked = []
    entry_edges = []

    def check_entry(address, dispatch, schema):
        edges = []

        @Visit
        def visit(_, pointer):
            e = pointer.contents
            edges.append((e.kind, e.field, e.target, e.extent, e.auxiliary))
            return 0

        descriptor = Descriptor(dispatch, schema)
        assert (
            lib.hpclang_rpl_dispatch_reference_visit(
                c.byref(reader), address, 6 if schema == 0 else 10, c.byref(descriptor), visit, None
            )
            == 0
        )
        expected = [(8, address, dispatch, 0, 0)]
        if schema == 0:
            expected.append((9, address + 5, address + 5, 0, 0))
        assert edges == expected
        entry_edges.extend(edges)

    def check_prefix():
        assert word(0x954) == D0 and word(0x958) == result_slot and word(0x95C) == p, (
            "address restoration"
        )
        assert reg(0) >> 20 == int.from_bytes(ram[offset + 0x90C : offset + 0x914], "little") >> 20
        assert reg(2) >> 20 == int.from_bytes(ram[offset + 0x91C : offset + 0x924], "little") >> 20
        for first, last in ((0x914, 0x91C), (0x924, 0x954)):
            expected = bytearray(ram[offset + first : offset + last])
            if args.duplicate_entry and first == 0x924:
                struct.pack_into("<I", expected, 0, 0xA5300000 | (capacity - 1))
            assert uc.mem_read(cpu + first, last - first) == expected
        assert reg(0) & 0xFFFFF == actual and reg(2) & 0xFFFFF == wanted
        assert word(0x998) == cursor and word(0x978 + cursor * 4) == 0
        for i in range(8):
            if i != cursor:
                assert word(0x978 + i * 4) == 0x70000 + i * 0x10
        assert len(scalar) == 1 and len(checked) == 4
        for at in (HEADER, HEADER + 5, SLOT, OBJECT):
            for n in range(5):
                physical = mapping.physical(at + n)
                assert uc.mem_read(physical, 1) == ram[physical - RAM : physical - RAM + 1]

    def hook(_, at, size, user):
        if at != 0x28898:
            return
        pc = uc.reg_read(UC_ARM_REG_R7)
        expected_trace = (
            TRACE
            + (
                [0x351FD, 0x351FF, 0x35206, 0x35209, 0x94005]
                if wanted == actual
                else [0x351ED, 0x3521D, 0x3521F, 0x35226, 0x35229, 0x94005]
            )
            if args.through_dispatch
            else TRACE
        )
        if args.duplicate_entry:
            expected_trace = DUP_TRACE + expected_trace
        assert len(trace) < len(expected_trace) and pc == expected_trace[len(trace)], hex(pc)
        trace.append(pc)
        if args.duplicate_entry and pc == DUP_TRACE[0]:
            check_entry(HEADER, 0x37258, 0)
        if pc == TRACE[0]:
            entry = HEADER + 5 if args.duplicate_entry else HEADER
            assert reg(0) & 0xFFFFF == entry
            check_entry(entry, 0x351CC, 1)

        for index, value in pending.items():
            assert reg(index) == value, (hex(pc), index, hex(reg(index)), hex(value))
        pending.clear()
        if pc == 0x351DA:
            assert word(0x998) == (cursor + 1) & 7
            assert word(0x978 + cursor * 4) == wanted
            scalar.append(word(0x978 + cursor * 4))
        if pc == TRACE[-1]:
            check_prefix()
            if not args.through_dispatch:
                uc.emu_stop()
            return
        if args.through_dispatch and pc == 0x94005:
            uc.emu_stop()
            return
        if pc in (0x351CC, 0x351D5, 0x351DD):
            value = Exchange()
            result = lib.hpclang_rpl_code_exchange_inspect(
                c.byref(reader), pc, 3, p, c.byref(value)
            )
            if fault == "first-exchange" and pc == 0x351CC:
                return
            assert result == 0
            mask = ((1 << (4 * value.nibbles)) - 1) << (4 * value.first)
            left, right = reg(value.left), reg(value.right)
            pending[value.left] = (left & ~mask) | (right & mask)
            pending[value.right] = (right & ~mask) | (left & mask)
            checked.append(pc)
        if pc == 0x351E3:
            value = Assignment()
            assert (
                lib.hpclang_rpl_code_assignment_inspect(c.byref(reader), pc, 3, p, c.byref(value))
                == 0
            )
            mask = (1 << (4 * value.nibbles)) - 1
            pending[value.destination] = (
                reg(value.destination) & ~(mask << (4 * value.destination_first))
            ) | (
                ((reg(value.source) >> (4 * value.source_first)) & mask)
                << (4 * value.destination_first)
            )
            checked.append(pc)

    def write(_, access, at, size, value, user):
        assert (
            cpu <= at
            and at + size <= cpu + 0xD00
            or 0x300000 <= at
            and at + size <= 0x310000
            or args.through_dispatch
            and (mapping.physical(result_slot) & ~3) <= at
            and at + size <= (mapping.physical(result_slot) & ~3) + 8
        ), "unexpected memory write"

    uc.hook_add(UC_HOOK_MEM_WRITE, write)
    uc.hook_add(UC_HOOK_CODE, hook)
    uc.emu_start(0x28828, 0x400000, count=100000)
    if args.through_dispatch:
        assert trace[-1] == 0x94005, "next RPL dispatch"
        assert word(0x954) == D0 + 5 and word(0x958) == result_slot and word(0x95C) == 0
        assert reg(0) & 0xFFFFF == 0x94000
        expected_result = 0x3A81 if wanted == actual else 0x3AC0
        value = sum(
            (
                uc.mem_read(mapping.physical(result_slot + n), 1)[0]
                >> (4 * ((result_slot + n) & 1))
                & 15
            )
            << (4 * n)
            for n in range(5)
        )
        assert value == expected_result, "predicate result"
        expected_ram = bytearray(ram)
        for n in range(5):
            physical = mapping.physical(result_slot + n) - RAM
            shift = 4 * ((result_slot + n) & 1)
            expected_ram[physical] = (expected_ram[physical] & ~(15 << shift)) | (
                (expected_result >> (4 * n) & 15) << shift
            )
        after = bytes(uc.mem_read(RAM, len(ram)))
        assert (
            after[:offset] == expected_ram[:offset]
            and after[offset + 0xD00 :] == expected_ram[offset + 0xD00 :]
        ), "unexpected native mutation"
        assert word(0x998) == cursor and word(0x978 + 4 * cursor) == 0
        for i in range(8):
            if i != cursor:
                assert word(0x978 + 4 * i) == 0x70000 + i * 0x10
    else:
        assert trace == TRACE
    return dict(
        model=model,
        cursor=cursor,
        p=p,
        wanted=wanted,
        actual=actual,
        scalar=scalar[0],
        decoder_transitions=len(checked),
        initial_carry=carry,
        saturn_trace=trace,
        through_dispatch=args.through_dispatch,
        duplicate_entry=args.duplicate_entry,
        structural_edges=entry_edges,
        capacity=capacity if args.duplicate_entry else None,
    )


rows = []
negatives = []
for model in (args.model,):
    for cursor in range(8):
        for i, (wanted, actual) in enumerate(
            ((0x2A2C, 0x2A2C), (0x2933, 0x2A2C), (0x90001, 0x90001), (0x90001, 0x2A2C))
        ):
            for carry in (0, 1) if args.through_dispatch else (0,):
                for capacity in (1, 16) if args.duplicate_entry else (1,):
                    rows.append(
                        execute(
                            model,
                            cursor,
                            wanted,
                            actual,
                            (cursor * 2 + i) % 16,
                            carry=carry,
                            capacity=capacity,
                        )
                    )
    for fault in (
        ("first-exchange", "restore-stack-address")
        + (("true-result", "false-result") if args.through_dispatch else ())
        + (("duplicate-depth",) if args.duplicate_entry else ())
    ):
        try:
            execute(model, 3, 0x2933 if fault == "false-result" else 0x2A2C, 0x2A2C, 15, fault)
        except AssertionError as error:
            assert str(error) == (
                "predicate result" if fault.endswith("result") else "address restoration"
            ), str(error)
            negatives.append(dict(model=model, fault=fault, reason=str(error)))
        else:
            raise AssertionError("corrupt handler accepted")
report = dict(
    cases=rows,
    negative_controls=negatives,
    scope="Original ARM interpreter, controlled registers/data and no IRQ. "
    + (
        "Includes comparison, Boolean writeback and next RPL dispatch. "
        if args.through_dispatch
        else "Stops before comparison. "
    )
    + (
        "Starts at DUP/inline-entry with capacity 1 or 16; the allocation/GC branch is excluded. "
        if args.duplicate_entry
        else ""
    )
    + "Temporary scalar RSTK slot is cleared, seven others preserved. No unrestricted caller/startup proof or relocation permission.",
    inputs={
        name: hashlib.sha256((args.state / name).read_bytes()).hexdigest()
        for name in ("sram", "flash")
    },
    sources={name: hashlib.sha256((base / name).read_bytes()).hexdigest() for name in names},
    headers={
        name: hashlib.sha256((base / name).read_bytes()).hexdigest()
        for name in ("rpl-code.h", "rpl-reference.h", "rpl-object-span.h")
    },
    reader_sha256=hashlib.sha256(
        (root / "tests/e2e/port-recovery/verify.py").read_bytes()
    ).hexdigest(),
    library_sha256=hashlib.sha256((OUT / "code.so").read_bytes()).hexdigest(),
    driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
)
(OUT / "predicate-prefix.json").write_text(json.dumps(report, indent=2) + "\n")
print(
    "PASS",
    len(rows),
    "original-interpreter predicate prefixes;",
    len(negatives),
    "changed-code controls",
)
