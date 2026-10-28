"""Execute the linked Thumb launcher collector with a controlled read callback.

This tests the shipped collector's ABI and output, not the entire stage1/MMU path.
"""

import importlib
from pathlib import Path
import struct
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_SP,
)


class ArmStageRoots:
    def __init__(self, elf):
        root = Path(__file__).resolve().parents[3]
        sys.path.insert(0, str(root / "tools"))
        blob, _, segments = importlib.import_module("hpclang-pack").parse_elf32(elf)
        self.blob, self.segments = blob, segments
        section_offset = struct.unpack_from("<I", blob, 32)[0]
        section_size, section_count = struct.unpack_from("<HH", blob, 46)
        assert section_size == 40
        sections = [
            struct.unpack_from("<10I", blob, section_offset + i * section_size)
            for i in range(section_count)
        ]
        matches = []
        for section in sections:
            if section[1] != 2:  # SHT_SYMTAB
                continue
            assert section[9] == 16 and section[5] % 16 == 0
            strings = sections[section[6]]
            names = blob[strings[4] : strings[4] + strings[5]]
            for offset in range(section[4], section[4] + section[5], 16):
                name, value, _, info, _, index = struct.unpack_from("<IIIBBH", blob, offset)
                assert name < len(names)
                end = names.index(0, name)
                if names[name:end] == b"hpclang_stage_execution_roots" and index and info & 15 == 2:
                    matches.append(value)
        assert len(matches) == 1
        self.entry = matches[0] | 1

    def inspect(self, address, stack, cursor, carry, a, cv, read):
        uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
        pages = set()
        for segment in self.segments:
            pages.update(
                range(segment.vaddr & ~4095, (segment.vaddr + segment.memsz + 4095) & ~4095, 4096)
            )
        for page in sorted(pages):
            uc.mem_map(page, 4096)
        for segment in self.segments:
            uc.mem_write(segment.vaddr, self.blob[segment.offset : segment.offset + segment.filesz])
        base, size = 0x03000000, 0x10000
        assert not any(base <= page < base + size for page in pages)
        uc.mem_map(base, size)
        io, machine, aa, cc = base, base + 0x100, base + 0x200, base + 0x204
        roots, masks, callback, finish, sp = (
            base + 0x300,
            base + 0x400,
            base + 0x500,
            base + 0x600,
            base + 0xF000,
        )
        uc.mem_write(io, struct.pack("<III", 0, callback | 1, 0))
        uc.mem_write(machine, stack)
        uc.mem_write(aa, struct.pack("<II", a, cv))
        sentinel = b"\xa5" * 40
        uc.mem_write(roots, sentinel)
        uc.mem_write(masks, sentinel)
        uc.mem_write(callback, b"\x70\x47")  # BX LR, after the callback fills r0/output.
        uc.mem_write(finish, b"\x00\xbe")
        uc.mem_write(sp, struct.pack("<IIIII", carry & 0xFFFFFFFF, aa, cc, roots, masks))
        for register, value in (
            (UC_ARM_REG_R0, io),
            (UC_ARM_REG_R1, address),
            (UC_ARM_REG_R2, machine),
            (UC_ARM_REG_R3, cursor),
            (UC_ARM_REG_SP, sp),
            (UC_ARM_REG_LR, finish | 1),
        ):
            uc.reg_write(register, value)
        completed = False

        def hook(_, pc, instruction_size, context):
            nonlocal completed
            if pc == finish:
                completed = True
                uc.emu_stop()
            elif pc == callback:
                value = read(uc.reg_read(UC_ARM_REG_R1), uc.reg_read(UC_ARM_REG_R2))
                if value is not None:
                    uc.mem_write(uc.reg_read(UC_ARM_REG_R3), struct.pack("<I", value))
                uc.reg_write(UC_ARM_REG_R0, int(value is not None))

        uc.hook_add(UC_HOOK_CODE, hook)
        uc.emu_start(self.entry, base + size, count=100000)
        assert completed and uc.reg_read(UC_ARM_REG_PC) == finish
        count = uc.reg_read(UC_ARM_REG_R0)
        assert count <= 10 and uc.reg_read(UC_ARM_REG_SP) == sp
        assert bytes(uc.mem_read(machine, 32)) == stack
        assert bytes(uc.mem_read(aa, 8)) == struct.pack("<II", a, cv)
        assert bytes(uc.mem_read(roots + 4 * count, 40 - 4 * count)) == sentinel[4 * count :]
        assert bytes(uc.mem_read(masks + 4 * count, 40 - 4 * count)) == sentinel[4 * count :]
        pointers = struct.unpack("<10I", uc.mem_read(roots, 40))[:count]
        values = struct.unpack("<10I", uc.mem_read(masks, 40))[:count]
        identities = {aa: "a", cc: "c", **{machine + 4 * i: f"stack-{i}" for i in range(8)}}
        return [(identities[pointer], mask) for pointer, mask in zip(pointers, values, strict=True)]
