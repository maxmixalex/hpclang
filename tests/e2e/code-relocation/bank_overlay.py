"""Check ERAM overlay precedence and exit behavior in the reviewed Stock ROMs."""

import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP


def verify_overlay(ram, flash, cpu, banks):
    """Do not infer payload validity or caller qualification from these maps."""
    results = []
    eram = tuple(0x08020000 + i * 0x800 for i in range(64))
    original = ram[cpu - 0x08000000 : cpu - 0x08000000 + 1024]
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
            for offset, value in ((0x998, 1), (0x978, 0x3130), (0x95C, p), (0x970, 0)):
                uc.mem_write(cpu + offset, struct.pack("<I", value))
            uc.reg_write(UC_ARM_REG_R7, banks[index]["switch"])
            returned.clear()
            uc.emu_start(0x28828, 0x400000, count=100000)
            assert returned
            mapping = bytes(uc.mem_read(cpu, 1024))
            assert mapping[:256] == original[:256] and mapping[512:] == original[512:]
            pages = struct.unpack_from("<64I", mapping, 256)
            assert struct.unpack("<I", uc.mem_read(cpu + 0x95C, 4))[0] == 0
            stages.append(dict(index=index, incoming_p=p, pages=list(pages)))
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
