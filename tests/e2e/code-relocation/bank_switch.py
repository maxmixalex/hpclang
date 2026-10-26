"""Execute profiled original-ROM bank switches in saved states; no hardware writes."""

from pathlib import Path
import json
import struct
import sys
import itertools
import argparse
import subprocess
import hashlib
import ctypes as c
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
from verify import native_words  # noqa: E402

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--state",
    action="append",
    required=True,
    help="hp50g:PATH or hp49gplus:PATH; repeat for saved states",
)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "bank-execution.json").unlink(missing_ok=True)
states = []
for item in args.state:
    model, path = item.split(":", 1)
    if model not in ("hp50g", "hp49gplus"):
        parser.error("Only reviewed HP50g/HP49g+ ROM profiles are supported")
    states.append((model, Path(path)))
shared = args.output / "spans.so"
# The unit harness supplies the bootstrap symbol; its main is never invoked.
subprocess.run(
    [
        args.cc,
        "-std=c11",
        "-shared",
        "-fPIC",
        "-O2",
        "-I" + str(root / "include"),
        "-I" + str(root / "runtime/core"),
        str(root / "runtime/core/rpl-stock-memory.c"),
        *[
            str(root / "runtime/core" / name)
            for name in (
                "rpl-reference.c",
                "rpl-code.c",
                "rpl-backup.c",
                "rpl-port.c",
                "rpl-object-store.c",
                "rpl-object-span.c",
            )
        ],
        str(root / "tests/unit/test_rpl_stock_memory.c"),
        "-o",
        str(shared),
    ],
    check=True,
)
rows = []
lib = c.CDLL(str(shared.resolve()))


class Edge(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("kind", "field", "target", "extent", "auxiliary")]


class Mappings(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("field", "target", "moving")]


class Patch(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("field", "value")]


lib.hpclang_rpl_reference_plan_offset_shift.argtypes = [
    c.POINTER(Edge),
    c.POINTER(Mappings),
    c.c_int32,
    c.c_int32,
    c.c_uint32,
    c.c_uint32,
    c.c_int32,
    c.POINTER(Patch),
]
lib.hpclang_rpl_stock_resolve_span.argtypes = [
    c.POINTER(c.c_uint32),
    c.c_uint32,
    c.c_uint32,
    c.POINTER(c.c_uint32),
]
lib.hpclang_rpl_stock_spans_overlap.argtypes = [
    c.POINTER(c.c_uint32),
    c.c_uint32,
    c.c_uint32,
    c.POINTER(c.c_uint32),
    c.c_uint32,
    c.c_uint32,
]
for (model, state), cursor in itertools.product(states, range(8)):
    state_kind = str(state)
    ram, flash = (state / "sram").read_bytes(), (state / "flash").read_bytes()
    reader = native_words(ram, flash)
    cpu = 0x08000000 + (reader.table - 128) * 4
    bases = struct.unpack_from("<16I", ram, cpu - 0x08000000 + 0xBE0)
    assert all(bases[:14]) and bases[14:] == (0, 0)

    def word(at):
        return sum(
            ((flash[reader.physical(at + i)] >> (4 * ((at + i) & 1))) & 15) << (4 * i)
            for i in range(5)
        )

    entries = [(str(i), word(0x26BBB + 7 * i)) for i in range(16)]
    entries.append(("eram", word(0x26C39)))
    views = []
    for label, entry in entries:
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        for base, data in ((0, flash), (0x08000000, ram)):
            uc.mem_map(base, len(data))
            uc.mem_write(base, data)
        uc.mem_map(0x300000, 0x10000)
        uc.mem_write(cpu + 0x998, struct.pack("<I", cursor))
        uc.mem_write(cpu + 0x978 + 4 * ((cursor - 1) & 7), struct.pack("<I", 0x3130))
        for reg, value in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, 0xFFFFF),
            (UC_ARM_REG_R7, entry),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(reg, value)
        trace = []

        def hook(_, at, size, userdata):
            if at == 0x28898:
                pc = uc.reg_read(UC_ARM_REG_R7)
                trace.append(pc)
                if pc == 0x3130 or len(trace) >= 100:
                    uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, hook)
        error = None
        try:
            uc.emu_start(0x28828, 0x400000, count=100000)
        except UcError as exc:
            error = str(exc)
        before = struct.unpack_from("<256I", ram, cpu - 0x08000000)
        after = struct.unpack("<256I", uc.mem_read(cpu, 1024))
        assert error is None and trace[-1] == 0x3130
        assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == (cursor - 1) & 7
        assert before[:64] == after[:64] and before[128:] == after[128:]
        base = 0x08020000 if label == "eram" else bases[int(label)]
        if base:
            assert after[64:128] == tuple(base + i * 2048 for i in range(64))
        else:
            assert len(set(after[64:128])) == 1 and after[64] % 2048 != 0
        physical = c.c_uint32(0xDEADBEEF)
        status = lib.hpclang_rpl_stock_resolve_span(
            (c.c_uint32 * 256)(*after), 0x40000, 0x40000, c.byref(physical)
        )
        if base:
            assert status == 0 and physical.value == base * 2
        else:
            assert status < 0 and physical.value == 0xDEADBEEF
        changes = [(i, hex(a), hex(b)) for i, (a, b) in enumerate(zip(before, after)) if a != b]
        row = dict(
            model=model,
            state=state_kind,
            cursor=cursor,
            sram_sha256=hashlib.sha256(ram).hexdigest(),
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            bank=label,
            entry=hex(entry),
            trace=[hex(x) for x in trace],
            error=error,
            changes=changes,
            resolve_status=status,
        )
        rows.append(row)
        views.append(((c.c_uint32 * 256)(*after), base))
    # Compare executed maps, not hypothetical windows truncated to allocations.
    # Bank 0's final 32 KiB aliases the beginning of bank 1.
    comparisons = 0
    for left, left_base in views:
        for right, right_base in views:
            result = lib.hpclang_rpl_stock_spans_overlap(
                left, 0x40000, 0x40000, right, 0x40000, 0x40000
            )
            if not left_base or not right_base:
                assert result < 0
            else:
                expected = int(
                    left_base < right_base + 0x20000 and right_base < left_base + 0x20000
                )
                assert result == expected
            comparisons += 1
    for offset in (0, 1, 0xFFE, 0xFFF, 0xFFFE, 0xFFFF):
        assert (
            lib.hpclang_rpl_stock_spans_overlap(
                views[0][0], 0x70000 + offset, 1, views[1][0], 0x40000 + offset, 1
            )
            == 1
        )
        assert (
            lib.hpclang_rpl_stock_spans_overlap(
                views[0][0], 0x70000 + offset, 1, views[1][0], 0x40000 + (offset ^ 1), 1
            )
            == 0
        )
        comparisons += 2
    rows[-1]["executed_map_comparisons"] = comparisons
    # Hypothetical shift only: these ROM ranges are not writable allocations.
    # Derive the translation from the executed maps, then verify the native
    # patched address resolves to the canonical destination's physical nibble.
    target_offset = 2 * (views[0][1] - views[1][1])
    assert target_offset == -0x30000
    for offset in (0, 1, 0xFFE, 0xFFF, 0xFEFF):
        edge = Edge(3, 0x80000, 0x70000 + offset, 0, entries[0][1])
        mappings = Mappings(2, 1, 1)
        patch = Patch(0xDEAD, 0xBEEF)
        assert (
            lib.hpclang_rpl_reference_plan_offset_shift(
                c.byref(edge),
                c.byref(mappings),
                0,
                target_offset,
                0x40000,
                0x4FF00,
                16,
                c.byref(patch),
            )
            == 1
        )
        assert patch.field == edge.field and patch.value == edge.target + 16
        native, canonical = c.c_uint32(), c.c_uint32()
        assert lib.hpclang_rpl_stock_resolve_span(views[0][0], patch.value, 1, c.byref(native)) == 0
        assert (
            lib.hpclang_rpl_stock_resolve_span(
                views[1][0], 0x40000 + offset + 16, 1, c.byref(canonical)
            )
            == 0
        )
        assert native.value == canonical.value
    rows[-1]["offset_plan_checks"] = 5
    print(
        "PASS",
        model,
        state_kind,
        "cursor",
        cursor,
        "17 original-ROM switches; 301 executed-map overlap/alias checks",
        flush=True,
    )
out = args.output / "bank-execution.json"
out.write_text(json.dumps(rows, indent=2) + "\n")
assert len(rows) == len(states) * 8 * 17
