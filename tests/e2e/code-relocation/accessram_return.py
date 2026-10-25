"""Verify original ACCESSRAM1 call sites preserve R4 and subsequent SAT2ARM."""

if not __debug__:
    raise RuntimeError("Assertions required")
import hashlib
import json
import struct
import sys
import importlib
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP
import argparse
import ctypes as c
import subprocess
from itertools import product

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp50g", "hp49gplus"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "port-recovery"))
native_words = importlib.import_module("verify").native_words
OUT = args.output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
RAM = 0x08000000
STOP = 0x70000
rows = []
inputs = {}
(OUT / "accessram-return.json").unlink(missing_ok=True)
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


class Literal(c.Structure):
    _fields_ = [
        (name, c.c_uint32) for name in ("instruction", "length", "field", "destination", "nibbles")
    ]
    _fields_ += [("mask", c.c_uint64), ("value", c.c_uint64)]


class Exchange(c.Structure):
    _fields_ = [
        (name, c.c_uint32)
        for name in ("instruction", "length", "left", "right", "first", "nibbles")
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


base = Path(__file__).resolve().parents[3] / "runtime/core"
decoder_hashes = {
    name: hashlib.sha256((base / name).read_bytes()).hexdigest()
    for name in ("rpl-code.c", "rpl-code.h")
}
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
        str(base / "rpl-code.c"),
        "-o",
        str(OUT / "code.so"),
    ],
    check=True,
)
code = c.CDLL(str(OUT / "code.so"))
code.hpclang_rpl_code_literal_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint,
    c.POINTER(Literal),
]
code.hpclang_rpl_code_assignment_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint,
    c.POINTER(Assignment),
]

code.hpclang_rpl_code_exchange_inspect.argtypes = [
    c.POINTER(Reader),
    c.c_uint32,
    c.c_uint32,
    c.c_uint,
    c.POINTER(Exchange),
]

for model in (args.model,):
    state = args.state
    flash, ram = (state / "flash").read_bytes(), (state / "sram").read_bytes()
    mapping = native_words(ram, flash)
    inputs[model] = {
        name: hashlib.sha256(data).hexdigest() for name, data in [("sram", ram), ("flash", flash)]
    }
    cpu = RAM + (mapping.table - 128) * 4
    start = 0x8890D
    helper = start + 0x410
    indirect = start + 0x456

    def encoding(at, n):
        return "".join(format(mapping(at + i, 1), "x") for i in range(n))

    assert encoding(start + 0xCA, 13) == "3403c6281af0c"
    assert encoding(start + 0x450, 10) == "81af1c81b3"
    assert encoding(0x88A82, 0x1E) == "7792afa0787021afe7511130afe850"
    assert encoding(0x88BAC, 0x15) == "0634fff10cafe0ef60701"
    assert encoding(0x26C30, 7) == "8d91842"
    assert encoding(0x26C37, 7) == "8d12c32"
    for pc, p in ((0x889F7, 0), (0x88B30, 1), (0x889EE, 8), (0x88AB4, 1)):
        assert encoding(pc, 9) == "2" + format(p, "x") + "8f73c62"

    @Read
    def read_code(_, at, value):
        if not (start + 0xCA <= at < start + 0xD7 or 0x88A82 <= at < 0x88AA0):
            return 0
        value[0] = mapping(at, 1)
        return 1

    reader = Reader(read_code, None)
    exchanges = []
    for at in (0x88A90, 0x88A9A):
        exchange = Exchange()
        assert (
            code.hpclang_rpl_code_exchange_inspect(c.byref(reader), at, 3, 0, c.byref(exchange))
            == 0
        )
        assert (exchange.left, exchange.right, exchange.first, exchange.nibbles) == (0, 2, 0, 16)
        exchanges.append(exchange)
    literal, assignment = Literal(), Assignment()
    assert (
        code.hpclang_rpl_code_literal_inspect(
            c.byref(reader), start + 0xCA, 13, 0, c.byref(literal)
        )
        == 0
    )
    assert literal.destination == 2 and literal.length == 7
    assert (
        code.hpclang_rpl_code_assignment_inspect(
            c.byref(reader), start + 0xD1, 6, 0, c.byref(assignment)
        )
        == 0
    )
    assert assignment.source == 2 and assignment.destination == 8 and assignment.length == 6

    for address, entry, stop in (
        (0, 0x889F7, 0x88A00),
        (1, 0x88B30, 0x88B39),
        (8, 0x889EE, 0x889F7),
        (1, 0x88AB4, 0x88ABD),
    ):
        service_stop = stop
        layouts = (
            ("saved", "one-library", "two-libraries", "overflow") if address == 8 else ("saved",)
        )
        for cursor, layout in product(range(8), layouts):
            stop = service_stop
            uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
            uc.mem_map(0, len(flash))
            uc.mem_write(0, flash)
            uc.mem_map(RAM, len(ram))
            uc.mem_write(RAM, ram)
            uc.mem_map(0x300000, 0x10000)
            for i in range(9):
                uc.mem_write(
                    cpu + 0x90C + 8 * i, struct.pack("<Q", 0x1234567800000000 + 0x111111 * i)
                )
            uc.mem_write(cpu + 0x94C, struct.pack("<Q", 0xDEADBEEFABC01234))
            uc.mem_write(cpu + 0xC24, bytes(4))
            uc.mem_write(cpu + 0x998, struct.pack("<I", cursor))
            for i in range(8):
                uc.mem_write(
                    cpu + 0x978 + 4 * i, struct.pack("<I", STOP if i == (cursor - 1) & 7 else 0)
                )
            tables = bytes(uc.mem_read(cpu, 0x900))
            trace = []
            writes = []
            injected = None
            expected_free = None
            initializing = True
            uc.reg_write(UC_ARM_REG_R4, cpu)
            uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
            uc.reg_write(UC_ARM_REG_R7, entry)
            uc.reg_write(UC_ARM_REG_SP, 0x308000)

            def hook(_, at, size, userdata):
                global injected, expected_free
                if at == 0x28898:
                    pc = uc.reg_read(UC_ARM_REG_R7)
                    trace.append(pc)
                    # Inject only after the original CONFIG sequence: use its
                    # resulting page mapping, never a guessed physical alias.
                    if pc == 0x2401E and layout != "saved":
                        assert injected is None
                        library = "04b2062000108510b7000000000000000000000d5c2"
                        extent = 5 + int(library[5:10][::-1], 16)
                        assert extent == len(library)
                        payload = library * (2 if layout == "two-libraries" else 1)
                        expected_free = 0x3FFFB - len(payload)
                        if layout == "overflow":
                            payload = "04b20" + "0000c"
                            expected_free = 0
                        payload += "00000"
                        payload += "0" * (len(payload) % 2)
                        packed = bytes(
                            int(payload[i], 16) | int(payload[i + 1], 16) << 4
                            for i in range(0, len(payload), 2)
                        )
                        physical = int.from_bytes(uc.mem_read(cpu + 0x100, 4), "little")
                        assert RAM <= physical and physical + len(packed) <= RAM + len(ram)
                        assert physical + len(packed) <= cpu or physical >= cpu + 0xD00
                        uc.mem_write(physical, packed)
                        assert bytes(uc.mem_read(physical, len(packed))) == packed
                        injected = dict(
                            physical=hex(physical),
                            bytes=len(packed),
                            sha256=hashlib.sha256(packed).hexdigest(),
                        )
                    if pc == stop or len(trace) > 2000:
                        uc.emu_stop()

            def write(_, access, at, size, value, userdata):
                assert (
                    cpu <= at
                    and at + size <= cpu + 0xD00
                    or 0x300000 <= at
                    and at + size <= 0x310000
                ), (hex(at), size)
                assert initializing or not (at < cpu + 0x954 and at + size > cpu + 0x94C), (
                    "R4 clobber"
                )
                writes.append((at, size))

            uc.hook_add(UC_HOOK_CODE, hook)
            uc.hook_add(UC_HOOK_MEM_WRITE, write)
            # Establish P=0 through its original opcode, then execute the actual
            # LC(5)/R4=C.A pair. The C descriptors predict all register writes.
            stop = 0x889F9
            uc.reg_write(UC_ARM_REG_R7, 0x889F7)
            uc.emu_start(0x28828, 0x400000, count=30000)
            assert trace == [0x889F7, stop]
            assert int.from_bytes(uc.mem_read(cpu + 0x95C, 4), "little") == 0
            expected = [
                int.from_bytes(uc.mem_read(cpu + 0x90C + 8 * i, 8), "little") for i in range(9)
            ]
            expected[literal.destination] = (
                expected[literal.destination] & ~literal.mask
            ) | literal.value
            mask = (1 << (4 * assignment.nibbles)) - 1
            bits = (expected[assignment.source] >> (4 * assignment.source_first)) & mask
            shift = 4 * assignment.destination_first
            expected[assignment.destination] = (
                expected[assignment.destination] & ~(mask << shift)
            ) | (bits << shift)
            stop = start + 0xD7
            trace = []
            uc.reg_write(UC_ARM_REG_R7, start + 0xCA)
            uc.emu_start(0x28828, 0x400000, count=30000)
            assert trace == [start + 0xCA, start + 0xD1, stop]
            actual = [
                int.from_bytes(uc.mem_read(cpu + 0x90C + 8 * i, 8), "little") for i in range(9)
            ]
            assert actual == expected and actual[8] == 0xDEADBEEFABC26C30
            assert bytes(uc.mem_read(cpu, 0x900)) == tables
            assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == cursor
            initializing = False
            stop = service_stop
            trace = []
            uc.reg_write(UC_ARM_REG_R7, entry)
            uc.emu_start(0x28828, 0x400000, count=1000000)
            assert trace[-1] == stop and 0x26C37 in trace and len(trace) < 2000
            assert bytes(uc.mem_read(cpu + 0x94C, 8)) == struct.pack("<Q", 0xDEADBEEFABC26C30)
            assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == cursor
            if layout != "saved":
                assert injected is not None
                assert int.from_bytes(uc.mem_read(cpu + 0x90C, 8), "little") == expected_free
                assert bool(int.from_bytes(uc.mem_read(cpu + 0x970, 4), "little")) == (
                    layout == "overflow"
                )
                assert (
                    trace.count(0x24027)
                    == {"one-library": 2, "two-libraries": 3, "overflow": 1}[layout]
                )
            after_tables = bytes(uc.mem_read(cpu, 0x900))
            changed = [
                i for i in range(0, 0x900, 4) if tables[i : i + 4] != after_tables[i : i + 4]
            ]
            assert tables[0x200:0x400] == after_tables[0x200:0x400], "Main read mapping changed"
            service_trace = list(trace)
            service_state = bytes(uc.mem_read(cpu, 0xD00))
            conversions = []
            alignments = []
            for logical in (0x80000, 0x80001, 0x86500, 0x88D67, 0xE1C86, 0xFFFFF):
                uc.mem_write(cpu, service_state)
                uc.mem_write(cpu + 0x91C, struct.pack("<Q", logical))
                uc.mem_write(cpu + 0x978 + 4 * ((cursor - 1) & 7), struct.pack("<I", STOP))
                uc.reg_write(UC_ARM_REG_R7, helper)
                uc.reg_write(UC_ARM_REG_SP, 0x308000)
                stop = STOP
                trace = []
                uc.emu_start(0x28828, 0x400000, count=30000)
                assert trace[-1] == STOP and trace[trace.index(indirect) + 1] == 0x26C30
                physical = int.from_bytes(uc.mem_read(cpu + 0x91C, 8), "little")
                assert physical == mapping.physical(logical) == RAM + (logical >> 1)
                assert bytes(uc.mem_read(cpu, 0x900)) == after_tables
                assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == ((cursor - 1) & 7)
                conversions.append(dict(logical=hex(logical), physical=hex(physical)))
                # Execute the original conversion call and both ST-controlled
                # continuations. The saved logical address is an explicit caller
                # input, not inferred from the physical result or a scalar scan.
                for skip_alignment in (False, True):
                    uc.mem_write(cpu, service_state)
                    uc.mem_write(cpu + 0x91C, struct.pack("<Q", logical))
                    uc.mem_write(cpu + 0x978 + 4 * ((cursor - 1) & 7), struct.pack("<I", logical))
                    flags = int.from_bytes(uc.mem_read(cpu + 0x968, 4), "little")
                    uc.mem_write(cpu + 0x968, struct.pack("<I", (flags & ~1) | skip_alignment))
                    before = [
                        int.from_bytes(uc.mem_read(cpu + 0x90C + 8 * i, 8), "little")
                        for i in range(9)
                    ]
                    before_d0 = int.from_bytes(uc.mem_read(cpu + 0x954, 4), "little")
                    before_stack = bytes(uc.mem_read(cpu + 0x978, 32))
                    stop = 0x88AA0
                    trace = []
                    uc.reg_write(UC_ARM_REG_R7, 0x88A82)
                    uc.reg_write(UC_ARM_REG_SP, 0x308000)
                    uc.emu_start(0x28828, 0x400000, count=30000)
                    suffix = [0x88A86, 0x88A89, 0x88A8B]
                    expected = list(before)
                    expected[0] = physical
                    expected[2] = (physical & ~0xFFFFF) | logical
                    aligned = (logical + 0x1FFF) & 0xFE000
                    if not skip_alignment:
                        # C descriptors specify simultaneous swaps. Arithmetic,
                        # C/RSTK and D0 effects are independently predicted here.
                        for index, exchange in enumerate(exchanges):
                            mask = ((1 << (4 * exchange.nibbles)) - 1) << (4 * exchange.first)
                            left, right = exchange.left, exchange.right
                            old_left, old_right = expected[left], expected[right]
                            expected[left] = (old_left & ~mask) | (old_right & mask)
                            expected[right] = (old_right & ~mask) | (old_left & mask)
                            if index == 0:
                                expected[0] = (expected[0] & ~0xFFFFF) | aligned
                        suffix += [
                            0x88A90,
                            0x88A93,
                            0x88BAC,
                            0x88BAE,
                            0x88BB5,
                            0x88BB7,
                            0x88BB9,
                            0x88BBD,
                            0x88BBF,
                            0x88A97,
                            0x88A9A,
                            0x88A9D,
                        ]
                    suffix += [stop]
                    assert trace[trace.index(0x88A86) :] == suffix
                    assert trace[0] == 0x88A82 and trace[trace.index(indirect) + 1] == 0x26C30
                    actual = [
                        int.from_bytes(uc.mem_read(cpu + 0x90C + 8 * i, 8), "little")
                        for i in range(9)
                    ]
                    assert actual == expected, (hex(logical), skip_alignment, actual, expected)
                    assert int.from_bytes(uc.mem_read(cpu + 0x954, 4), "little") == (
                        before_d0 if skip_alignment else aligned
                    )
                    assert int.from_bytes(uc.mem_read(cpu + 0x95C, 4), "little") == 0
                    assert int.from_bytes(uc.mem_read(cpu + 0x968, 4), "little") == flags | 1
                    assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == (
                        (cursor - 1) & 7
                    )
                    # Calls may reuse the free slot and consumed caller slot.
                    # All other ring slots must survive; this is not a depth proof.
                    after_stack = bytes(uc.mem_read(cpu + 0x978, 32))
                    for slot in range(8):
                        if slot not in (cursor, (cursor - 1) & 7):
                            assert (
                                after_stack[4 * slot : 4 * slot + 4]
                                == before_stack[4 * slot : 4 * slot + 4]
                            )
                    assert bytes(uc.mem_read(cpu, 0x900)) == after_tables
                    alignments.append(
                        dict(
                            logical=hex(logical),
                            skip_alignment=skip_alignment,
                            d0=hex(before_d0 if skip_alignment else aligned),
                            physical=hex(actual[0]),
                            restored=hex(actual[2]),
                        )
                    )

            rows.append(
                dict(
                    model=model,
                    p=address,
                    entry=hex(entry),
                    cursor=cursor,
                    layout=layout,
                    initialization_matches_c_descriptors=True,
                    injected=injected,
                    expected_free=expected_free,
                    trace=[hex(x) for x in service_trace],
                    changed_mapping_words=[hex(x) for x in changed],
                    conversions=conversions,
                    alignments=alignments,
                )
            )
assert decoder_hashes == {
    name: hashlib.sha256((base / name).read_bytes()).hexdigest() for name in decoder_hashes
}
assert driver_hash == hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
report = dict(
    cases=rows,
    inputs=inputs,
    decoder_sha256=decoder_hashes,
    driver_sha256=driver_hash,
    scope="Four real command-4 ACCESSRAM1 call sites, original ROM and pinned saved mappings; R4 initialization parsed by C descriptors and executed by the original interpreter, with controlled P=0 and other caller registers. The original SAT2ARM call and both ST-controlled alignment continuations also run with an explicitly seeded saved logical address; this does not establish return-stack capacity or provenance of arbitrary caller inputs. P=8 covers saved contents, one/two controlled size-delimited library records and an overflowing extent. This does not validate arbitrary object contents, all bank configurations or full launcher closure.",
    implication="R4 is never written by the observed ROM service paths. All main-RAM read mappings remain stable and six subsequent SAT2ARM conversions retain correct physical byte addresses. Both ST-controlled original caller continuations preserve physical A while restoring or aligning the saved logical address in C; arithmetic can wrap at 20 bits and is not relocation authorization.",
)
(OUT / "accessram-return.json").write_text(json.dumps(report, indent=2) + "\n")
print(
    json.dumps(
        dict(
            cases=len(rows),
            conversions=sum(len(x["conversions"]) for x in rows),
            alignments=sum(len(x["alignments"]) for x in rows),
        )
    )
)
