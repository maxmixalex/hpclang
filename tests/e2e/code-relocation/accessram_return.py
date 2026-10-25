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
from itertools import product

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp50g", "hp49gplus"), required=True)
parser.add_argument("--output", type=Path, required=True)
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
    assert encoding(0x26C30, 7) == "8d91842"
    assert encoding(0x26C37, 7) == "8d12c32"
    for pc, p in ((0x889F7, 0), (0x88B30, 1), (0x889EE, 8), (0x88AB4, 1)):
        assert encoding(pc, 9) == "2" + format(p, "x") + "8f73c62"
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
            uc.mem_write(cpu + 0x94C, struct.pack("<Q", 0xDEADBEEFABC26C30))
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
                assert not (at < cpu + 0x954 and at + size > cpu + 0x94C), "R4 clobber"
                writes.append((at, size))

            uc.hook_add(UC_HOOK_CODE, hook)
            uc.hook_add(UC_HOOK_MEM_WRITE, write)
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
            rows.append(
                dict(
                    model=model,
                    p=address,
                    entry=hex(entry),
                    cursor=cursor,
                    layout=layout,
                    injected=injected,
                    expected_free=expected_free,
                    trace=[hex(x) for x in service_trace],
                    changed_mapping_words=[hex(x) for x in changed],
                    conversions=conversions,
                )
            )
report = dict(
    cases=rows,
    inputs=inputs,
    driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    scope="Four real command-4 ACCESSRAM1 call sites, original ROM and pinned saved mappings; controlled R4 entry and caller registers. P=8 covers saved contents, one/two controlled size-delimited library records and an overflowing extent. This does not validate arbitrary object contents, all bank configurations or full launcher closure.",
    implication="R4 is never written by the observed ROM service paths. All main-RAM read mappings remain stable and six subsequent SAT2ARM conversions retain correct physical byte addresses.",
)
(OUT / "accessram-return.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(dict(cases=len(rows), conversions=sum(len(x["conversions"]) for x in rows))))
