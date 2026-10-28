"""Check exhausted-capacity ROM execution and its unsafe movable-RAM contrast.

Requires Unicorn and a retained predicate fixture. The RAM mode succeeds only
when it reproduces stale execution registers; it is not a relocation pass.
No allocator or Saturn instruction is stubbed.
"""

from pathlib import Path
import argparse
import collections
import hashlib
import importlib
import json
import struct
import sys
from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--state",
    type=Path,
    required=True,
    help="retained Stock predicate fixture: ten Boolean/empty-string pairs",
)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument(
    "--ram-interior",
    action="store_true",
    help="reproduce the stale interior continuation; NOT a passing relocation case",
)
args = parser.parse_args()
if not __debug__:
    raise RuntimeError("Verification requires assertions enabled")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output
out.mkdir(parents=True, exist_ok=True)
(out / (args.model + ".json")).unlink(missing_ok=True)
for model in (args.model,):
    state = args.state
    flash = (state / "flash").read_bytes()
    original = (state / "sram").read_bytes()
    ram = bytearray(original)
    mapping = native_words(ram, flash)
    offset = (mapping.table - 128) * 4
    cpu = 0x08000000 + offset

    def value(at):
        result = 0
        for i in range(5):
            physical = mapping.physical(at + i)
            assert 0x08000000 <= physical < 0x08000000 + len(ram)
            result |= ((ram[physical - 0x08000000] >> (4 * ((at + i) & 1))) & 15) << (4 * i)
        return result

    globals_before = {
        hex(at): value(at) for at in (0x806E9, 0x806EE, 0x806F3, 0x806F8, 0x806FD, 0x8076B, 0x80E9B)
    }

    def setword(at, v):
        struct.pack_into("<I", ram, offset + at, v)

    header = 0x35064
    old_blocks = []
    if args.ram_interior:

        def put(at, number, count=5):
            for i in range(count):
                pos = at + i
                physical = mapping.physical(pos)
                assert 0x08000000 <= physical < 0x08000000 + len(ram)
                index, shift = physical - 0x08000000, 4 * (pos & 1)
                ram[index] = (ram[index] & ~(15 << shift)) | (((number >> (i * 4)) & 15) << shift)

        bottom, top, ret, stack = (value(a) for a in (0x806E9, 0x806EE, 0x806F3, 0x806F8))

        def check_chain():
            cursor = value(0x806EE)
            blocks = []
            assert value(bottom) == 0
            while cursor > bottom + 5:
                size = value(cursor - 5)
                assert 11 <= size <= cursor - bottom - 5
                cursor -= size
                blocks.append((cursor, size))
            assert cursor == bottom + 5
            return blocks

        old_blocks = check_chain()
        # Model the established TEMP envelope: marker, DOCSTR, even payload, back-link.
        # Shift only the existing return-stack interval; this is fixture construction,
        # not a claim that the public allocator can already install this object.
        extent = 32
        assert ret + extent + 5 < stack
        for pos in reversed(range(top, ret)):
            physical = mapping.physical(pos)
            nibble = (ram[physical - 0x08000000] >> (4 * (pos & 1))) & 15
            put(pos + extent, nibble, 1)
        put(top, 0, 1)
        put(top + 1, 0x2A2C)
        put(top + 6, 21)
        header = top + 11
        put(header, 0x37258)
        put(header + 5, 0x351CC)
        put(header + 10, 0x2A2C)
        put(header + 15, 0, 1)
        put(top + 27, extent)
        put(0x806EE, top + extent)
        put(0x806F3, ret + extent)
        put(0x80E9B, value(0x80E9B) - ((stack - ret) // 5 - (stack - ret - extent) // 5))
        assert check_chain() == [(top, extent)] + old_blocks
        # Add an explicit object-start data root, keeping all original stack cells.
        put(stack - 5, top + 1)
        put(0x806F8, stack - 5)
        globals_before["0x806f8"] = stack - 5
        original = bytes(ram)
    # D.A=0 deliberately requests the slow path; it does not simulate low RAM.
    setword(0x90C, header)
    setword(0x914, value(0x806F3))
    setword(0x924, 0)
    setword(0x954, value(0x8076B))
    setword(0x958, value(0x806F8))
    setword(0x95C, 0)
    setword(0xC24, 0)
    # No caller machine-stack frames; the actual GOSBVL supplies its return.
    for i in range(8):
        setword(0x978 + 4 * i, 0)
    setword(0x998, 0)
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(0, len(flash))
    uc.mem_write(0, flash)
    uc.mem_map(0x08000000, len(ram))
    uc.mem_write(0x08000000, bytes(ram))
    uc.mem_map(0x300000, 0x10000)
    for reg, v in (
        (UC_ARM_REG_SP, 0x308000),
        (UC_ARM_REG_R4, cpu),
        (UC_ARM_REG_R5, struct.unpack_from("<I", flash, 0x296F8)[0]),
        (UC_ARM_REG_R7, 0x37258),
    ):
        uc.reg_write(reg, v)
    counts = collections.Counter()
    tail = collections.deque(maxlen=40)
    reached = []

    def hook(uc, address, size, data):
        if address != 0x28898:
            return
        pc = uc.reg_read(UC_ARM_REG_R7)
        counts[pc] += 1
        tail.append(pc)
        if pc in (0x5F0F, 0x679B, 0x613E, 0x67D2, 0x3727D, 0x351CC):
            reached.append(pc)
        if pc == 0x351CC or sum(counts.values()) >= 200000:
            uc.emu_stop()

    uc.hook_add(UC_HOOK_CODE, hook, begin=0x28898, end=0x28898)
    error = None
    try:
        uc.emu_start(0x28828, 0, count=20000000)
    except UcError as exc:
        error = str(exc)
    final = bytes(uc.mem_read(0x08000000, len(ram)))
    final_mapping = native_words(final, flash)

    def read_word(data, table, at):
        def nib(pos):
            physical = table.physical(pos)
            source, index = (
                (data, physical - 0x08000000) if physical >= 0x08000000 else (flash, physical)
            )
            return (source[index] >> (4 * (pos & 1))) & 15

        return sum(nib(at + i) << (4 * i) for i in range(5))

    top, end = globals_before["0x806f8"], globals_before["0x806fd"]
    preserved = []
    for slot in range(top, end - 5, 5):
        before = read_word(original, mapping, slot)
        after = read_word(final, final_mapping, slot)
        if before in (0x3A81, 0x3AC0):
            assert after == before, (model, "Boolean changed", hex(slot))
            kind = "Boolean"
        else:
            assert read_word(original, mapping, before) == 0x2A2C
            assert read_word(original, mapping, before + 5) == (
                21 if args.ram_interior and slot == top else 5
            )
            assert read_word(final, final_mapping, after) == 0x2A2C
            assert read_word(final, final_mapping, after + 5) == (
                21 if args.ram_interior and slot == top else 5
            )
            kind = (
                "DOCSTR continuation fixture"
                if args.ram_interior and slot == top
                else "empty string"
            )
        preserved.append(dict(slot=slot, before=before, after=after, kind=kind))
    assert len(preserved) == (21 if args.ram_interior else 20)

    def get(at):
        return int.from_bytes(uc.mem_read(cpu + at, 4), "little")

    assert error is None and list(tail)[-1] == 0x351CC
    assert reached == [0x5F0F, 0x679B, 0x613E, 0x67D2, 0x3727D, 0x351CC]
    continuation = get(0x90C) & 0xFFFFF
    assert get(0x934) & 0xFFFFF == continuation
    assert continuation == header + 5
    expected_continuation = continuation
    if args.ram_interior:
        moved_object = read_word(final, final_mapping, top)
        expected_continuation = moved_object + 15
        assert moved_object != header - 10, "Fixture did not move"
        assert read_word(final, final_mapping, expected_continuation) == 0x351CC
        assert read_word(final, final_mapping, expected_continuation - 5) == 0x37258
        assert read_word(final, final_mapping, expected_continuation + 5) == 0x2A2C
        assert continuation != expected_continuation, (
            "Expected stale-address contrast no longer reproduces"
        )
    assert get(0x954) == globals_before["0x8076b"]
    assert get(0x958) == top - 5 and get(0x998) == 0
    assert read_word(final, final_mapping, top - 5) == read_word(final, final_mapping, top)
    report = dict(
        model=model,
        scope="Original ROM interpreter with IRQs disabled and caller machine stack cleared. Forced capacity exhaustion, not actual low RAM; modeled RAM variant diagnoses an unsupported movable interior entry, not public allocator behavior.",
        globals_before=globals_before,
        reached=[hex(v) for v in reached],
        tail=[hex(v) for v in tail],
        instructions=sum(counts.values()),
        error=error,
        registers={
            hex(at): int.from_bytes(uc.mem_read(cpu + at, 4), "little")
            for at in (0x90C, 0x914, 0x924, 0x934, 0x954, 0x958, 0x998)
        },
        changed_bytes=sum(a != b for a, b in zip(ram, final)),
        inputs={n: hashlib.sha256((state / n).read_bytes()).hexdigest() for n in ("flash", "sram")},
        driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    )
    report["outcome"] = (
        "stale RAM continuation reproduced" if args.ram_interior else "ROM continuation preserved"
    )
    report["continuation"] = dict(
        before=header + 5, observed=continuation, expected=expected_continuation
    )
    report["fixture"] = dict(
        mode="modeled TEMP DOCSTR with explicit data-stack root"
        if args.ram_interior
        else "retained Stock heap",
        original_temp_blocks=old_blocks,
    )
    report["native_reader_sha256"] = hashlib.sha256(
        Path(importlib.import_module("verify").__file__).read_bytes()
    ).hexdigest()
    report["preserved_stack"] = preserved
    report["checks"] = (
        "Original 20 user values preserved; duplicated top, D0 and machine-stack cursor checked. RAM contrast additionally checks moved data root and stale A/R1; not relocation acceptance."
    )
    (out / (model + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    (out / (model + "-sram")).write_bytes(final)
    print(model, report["outcome"], report["continuation"], flush=True)
