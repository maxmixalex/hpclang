"""Compare full read-map prediction with the original ARM bank-control handler."""

import ctypes as c
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR


def verify_bank_map(lib, ram, flash, cpu, banks):
    class Geometry(c.Structure):
        _fields_ = [("physical", c.c_uint32), ("bytes", c.c_uint32)]

    class State(c.Structure):
        _fields_ = [("pages", c.c_uint32 * 256), ("owners", c.c_uint8 * 256)]
        _fields_ += [
            (name, c.c_uint32) for name in ("lower", "upper", "overlay_page", "overlay_physical")
        ]

    lib.hpclang_rpl_stock_bank_map_select.argtypes = [
        c.POINTER(State),
        c.POINTER(Geometry),
        c.c_uint32,
        c.c_uint32,
        c.POINTER(c.c_uint32),
    ]
    geometry = (Geometry * 16)(
        *(Geometry(row["physical"] or 0, row["valid_bytes"]) for row in banks[:16])
    )
    results = []
    for context in ("baseline", "owners", "overlay"):
        for lower in range(4):
            for upper in range(16):
                uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
                for address, data in ((0, flash), (0x08000000, ram)):
                    uc.mem_map(address, len(data))
                    uc.mem_write(address, data)
                uc.mem_map(0x300000, 0x10000)
                if context == "owners":
                    # Also distinguishes same-selector early return from an update.
                    uc.mem_write(
                        cpu,
                        struct.pack("<256I", *(0x08040000 + (i % 64) * 0x800 for i in range(256))),
                    )
                    uc.mem_write(cpu + 0x808, bytes(0 if i % 3 else 2 for i in range(256)))
                if context == "overlay":
                    # Cross a quadrant boundary and override both owner meanings.
                    uc.mem_write(cpu + 0xC48, struct.pack("<I", 63))
                    uc.mem_write(cpu + 0xC5C, struct.pack("<I", 0x08060000))
                state = State()
                state.pages[:] = struct.unpack("<256I", uc.mem_read(cpu, 1024))
                state.owners[:] = uc.mem_read(cpu + 0x808, 256)
                for field, offset in (
                    ("lower", 0xBCC),
                    ("upper", 0xBD0),
                    ("overlay_page", 0xC48),
                    ("overlay_physical", 0xC5C),
                ):
                    setattr(state, field, struct.unpack("<I", uc.mem_read(cpu + offset, 4))[0])
                predicted = (c.c_uint32 * 256)(*([0xDEADBEEF] * 256))
                result = lib.hpclang_rpl_stock_bank_map_select(
                    c.byref(state), geometry, lower, upper, predicted
                )
                if not banks[upper]["populated"]:
                    assert result < 0 and list(predicted) == [0xDEADBEEF] * 256
                    results.append(dict(context=context, lower=lower, upper=upper, rejected=True))
                    continue
                assert result == 0
                for register, value in (
                    (UC_ARM_REG_R0, cpu),
                    (UC_ARM_REG_R1, (lower << 5) | (upper << 1)),
                    (UC_ARM_REG_SP, 0x308000),
                    (UC_ARM_REG_LR, 0x300000),
                ):
                    uc.reg_write(register, value)
                returned = []

                def stop(_, pc, size, user):
                    if pc == 0x300000:
                        returned.append(True)
                        uc.emu_stop()

                uc.hook_add(UC_HOOK_CODE, stop)
                uc.emu_start(0x248DC, 0x400000, count=100000)
                assert returned
                actual = list(struct.unpack("<256I", uc.mem_read(cpu, 1024)))
                assert actual == list(predicted), (context, lower, upper)
                assert struct.unpack("<2I", uc.mem_read(cpu + 0xBCC, 8)) == (lower, upper)
                assert bytes(uc.mem_read(0, len(flash))) == flash
                results.append(
                    dict(
                        context=context,
                        lower=lower,
                        upper=upper,
                        rejected=False,
                        unchanged_selectors=(lower, upper) == (state.lower, state.upper),
                    )
                )
    return results
