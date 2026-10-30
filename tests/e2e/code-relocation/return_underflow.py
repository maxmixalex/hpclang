"""Check cleared return-stack slots before qualifying later register uses."""

import argparse
import importlib
from pathlib import Path
import hashlib
import json
import struct
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP
from arm_stage_roots import ArmStageRoots

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--model", choices=("hp49gplus", "hp50g"), required=True)
parser.add_argument("--stage1-elf", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if not __debug__:
    raise RuntimeError("Assertions required")
root = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(root / "tests/e2e/port-recovery"), str(root / "tests/e2e/code-relocation")]
native_words = importlib.import_module("verify").native_words

elf = args.stage1_elf
arm = ArmStageRoots(elf)
rows = []
for model in (args.model,):
    state = args.state
    flash, ram = (state / "flash").read_bytes(), (state / "sram").read_bytes()
    mapping = native_words(ram, flash)
    cpu = 0x08000000 + (mapping.table - 128) * 4
    for cursor in range(8):
        instructions = {
            0x90000: (0x10, 2),
            0x91000: (0xF4, 3),
            0x91003: (0x10, 2),
            0x91010: (0x2B18, 4),
            **{0x91100 + i * 0x100: (0x20 if i == 6 else 0x10, 2) for i in range(7)},
        }
        nibbles = {
            at + i: (word >> (4 * i)) & 15
            for at, (word, width) in instructions.items()
            for i in range(width)
        }
        stack = [0] * 8
        for i in range(8):
            stack[(cursor - 1 - i) % 8] = 0x91000 + i * 0x100

        def read(at, width):
            if not all(at + i in nibbles for i in range(width)):
                return None
            return sum(nibbles[at + i] << (4 * i) for i in range(width))

        actual = arm.inspect(
            0x90000, struct.pack("<8I", *stack), cursor, 0, 0xABC95000, 0xDEF96000, read
        )
        expected = [(f"stack-{(cursor - 1 - i) % 8}", 0xFFFFFFFF) for i in range(8)]
        assert actual == expected, actual
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        uc.mem_map(0, len(flash))
        uc.mem_write(0, flash)
        uc.mem_map(0x08000000, len(ram))
        uc.mem_write(0x08000000, ram)
        uc.mem_map(0x300000, 0x10000)
        for at, nibble in nibbles.items():
            physical, shift = mapping.physical(at), 4 * (at & 1)
            old = uc.mem_read(physical, 1)[0]
            uc.mem_write(physical, bytes([(old & ~(15 << shift)) | (nibble << shift)]))
        uc.mem_write(cpu + 0x978, struct.pack("<8I", *stack))
        for offset, value in (
            (0x998, cursor),
            (0x970, 0),
            (0xC24, 0),
            (0x90C, 0xABC95000),
            (0x91C, 0xDEF96000),
        ):
            uc.mem_write(cpu + offset, struct.pack("<I", value))
        for register, value in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, 0xFFFFF),
            (UC_ARM_REG_R7, 0x90000),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(register, value)
        trace = []

        def hook(_, pc, size, context):
            if pc == 0x28898:
                trace.append(uc.reg_read(UC_ARM_REG_R7))
                if trace[-1] == 0 or len(trace) > 20:
                    uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, hook)
        uc.emu_start(0x28828, 0x400000, count=100000)
        assert trace == [0x90000, 0x91000, 0x91003, *range(0x91100, 0x91800, 0x100), 0], trace
        assert bytes(uc.mem_read(cpu + 0x978, 32)) == bytes(32)
        assert int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little") == (cursor - 9) % 8
        assert bytes(uc.mem_read(0, len(flash))) == flash
        rows.append(
            dict(
                model=model,
                cursor=cursor,
                roots=actual,
                expected_roots=expected,
                original_trace=trace,
            )
        )
args.output.mkdir(parents=True, exist_ok=True)
out = args.output / "return-underflow.json"
out.write_text(
    json.dumps(
        dict(
            elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
            cases=rows,
            collector_sha256=hashlib.sha256(
                (root / "platform/armtoolbox/stage-stack.c").read_bytes()
            ).hexdigest(),
            driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            adapter_sha256=hashlib.sha256(
                Path(__file__).with_name("arm_stage_roots.py").read_bytes()
            ).hexdigest(),
            flash_sha256=hashlib.sha256(flash).hexdigest(),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
            scope="Controlled nine-pop paths; no general exhausted-stack collection or full startup proof",
        ),
        indent=2,
    )
    + "\n"
)
print("PASS", args.model, len(rows), "cleared-stack ROM/ARM paths; no false A root")
