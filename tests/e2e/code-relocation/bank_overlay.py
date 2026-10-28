"""Check ERAM overlay precedence and exit behavior in the reviewed Stock ROMs."""

import ctypes as c
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP


def verify_overlay(lib, ram, flash, cpu, banks):
    """Do not infer payload validity or caller qualification from these maps."""
    results = []
    eram = tuple(0x08020000 + i * 0x800 for i in range(64))
    original = ram[cpu - 0x08000000 : cpu - 0x08000000 + 1024]
    initial_bank = struct.unpack_from("<I", ram, cpu - 0x08000000 + 0xBD0)[0]
    assert initial_bank == 2

    class Geometry(c.Structure):
        _fields_ = [("physical", c.c_uint32), ("bytes", c.c_uint32)]

    lib.hpclang_rpl_stock_bank_window_select.argtypes = [
        c.POINTER(c.c_uint32),
        c.POINTER(c.c_uint8),
        c.POINTER(Geometry),
        c.POINTER(c.c_uint32),
    ]
    for selected in range(16):
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        for address, data in ((0, flash), (0x08000000, ram)):
            uc.mem_map(address, len(data))
            uc.mem_write(address, data)
        uc.mem_map(0x300000, 0x10000)
        for reg, value in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, 0xFFFFF),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(reg, value)
        returned = []

        def stop(_, pc, size, user):
            if pc == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
                returned.append(True)
                uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, stop)
        stages = []
        for index, p in ((16, 0), (selected, 0), (16, 1), (selected, 1)):
            predicted = None
            if index != 16:
                # The reviewed P=1 exit selects bank 2; the C function models
                # the page update, not the entry/exit handler's control flow.
                target = banks[2 if p else index]
                geometry = Geometry(target["physical"] or 0, target["valid_bytes"])
                previous = (c.c_uint32 * 64).from_buffer_copy(uc.mem_read(cpu + 0x100, 256))
                owners = (c.c_uint8 * 64).from_buffer_copy(uc.mem_read(cpu + 0x848, 64))
                candidate = (c.c_uint32 * 64)(*([0xDEAD] * 64))
                result = lib.hpclang_rpl_stock_bank_window_select(
                    previous, owners, c.byref(geometry), candidate
                )
                if target["populated"]:
                    assert result == 0
                    predicted = list(candidate)
                else:
                    assert result < 0 and list(candidate) == [0xDEAD] * 64
            for offset, value in ((0x998, 1), (0x978, 0x3130), (0x95C, p), (0x970, 0)):
                uc.mem_write(cpu + offset, struct.pack("<I", value))
            uc.reg_write(UC_ARM_REG_R7, banks[index]["switch"])
            returned.clear()
            uc.emu_start(0x28828, 0x400000, count=100000)
            assert returned
            mapping = bytes(uc.mem_read(cpu, 1024))
            assert mapping[:256] == original[:256] and mapping[512:] == original[512:]
            pages = struct.unpack_from("<64I", mapping, 256)
            if predicted is not None:
                assert list(pages) == predicted
            assert struct.unpack("<I", uc.mem_read(cpu + 0x95C, 4))[0] == 0
            # Reviewed interpreter state, not a portable runtime layout. The
            # Flash selector and visible page ownership are separate state.
            flags = struct.unpack("<I", uc.mem_read(cpu + 0xB9C, 4))[0]
            flash_bank = struct.unpack("<I", uc.mem_read(cpu + 0xBD0, 4))[0]
            read_owners = list(uc.mem_read(cpu + 0x848, 64))
            stages.append(
                dict(
                    index=index,
                    incoming_p=p,
                    pages=list(pages),
                    mapping_flags=flags,
                    flash_bank=flash_bank,
                    read_owners=read_owners,
                    c_prediction_matched=predicted is not None,
                )
            )
        assert [stage["mapping_flags"] & 8 for stage in stages] == [8, 8, 0, 0]
        assert [stage["flash_bank"] for stage in stages] == [initial_bank, selected, selected, 2]
        assert all(stage["read_owners"] == [2] * 64 for stage in stages[:2])
        assert all(stage["read_owners"] == [0] * 64 for stage in stages[2:])
        assert tuple(stages[0]["pages"]) == eram
        # Flash selection cannot bypass an already enabled ERAM overlay.
        assert tuple(stages[1]["pages"]) == eram
        if banks[selected]["populated"]:
            assert stages[2]["pages"] == banks[selected]["pages"]
        else:
            # The original ROM can expose a linear physical-zero window here.
            # This still is NOT populated bank geometry or permission to read it.
            assert banks[selected]["physical"] is None and banks[selected]["valid_bytes"] == 0
            assert stages[2]["pages"] == [i * 0x800 for i in range(64)]
        assert stages[3]["pages"] == banks[2]["pages"]
        assert bytes(uc.mem_read(0, len(flash))) == flash
        results.append(
            dict(selected=selected, populated=banks[selected]["populated"], stages=stages)
        )
    return results


def verify_attachment_switches(ram, flash, cpu, banks):
    """Fixture-specific aliases: compare maps/state, without granting runtime identity."""
    results = []

    def execute(initial, entry):
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        for address, data in ((0, flash), (0x08000000, ram)):
            uc.mem_map(address, len(data))
            uc.mem_write(address, data)
        uc.mem_map(0x300000, 0x10000)
        for reg, value in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, 0xFFFFF),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(reg, value)
        returned = []

        def stop(_, pc, size, user):
            if pc == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
                returned.append(True)
                uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, stop)

        def call(switch, p):
            for offset, value in ((0x998, 1), (0x978, 0x3130), (0x95C, p), (0x970, 0)):
                uc.mem_write(cpu + offset, struct.pack("<I", value))
            uc.reg_write(UC_ARM_REG_R7, switch)
            returned.clear()
            uc.emu_start(0x28828, 0x400000, count=100000)
            assert returned
            assert struct.unpack("<I", uc.mem_read(cpu + 0x95C, 4))[0] == 0
            assert bytes(uc.mem_read(0, len(flash))) == flash
            return dict(
                pages=list(struct.unpack("<256I", uc.mem_read(cpu, 1024))),
                owners=list(uc.mem_read(cpu + 0x808, 256)),
                flags=struct.unpack("<I", uc.mem_read(cpu + 0xB9C, 4))[0],
                banks=list(struct.unpack("<2I", uc.mem_read(cpu + 0xBCC, 8))),
            )

        if initial is not None:
            call(banks[initial]["switch"], 0)
        return [call(entry, 0), call(entry, 1)]

    # These addresses are evidence for the two captured supported ROMs only.
    # A lookup must not recognize them in another mapping merely by address.
    for switch, bank in ((0x22E83, 2), (0x22E99, 4)):
        for initial in (None, 0, 16):
            actual = execute(initial, switch)
            canonical = execute(initial, banks[bank]["switch"])
            assert actual == canonical
            assert [stage["banks"][1] for stage in actual] == [bank, 2]
            if initial == 16:
                expected = [0x08020000 + i * 0x800 for i in range(64)]
                assert all(stage["pages"][64:128] == expected for stage in actual)
            else:
                assert actual[0]["pages"][64:128] == banks[bank]["pages"]
                assert actual[1]["pages"][64:128] == banks[2]["pages"]
            results.append(dict(switch=switch, bank=bank, initial=initial, stages=actual))
    return results
