"""Compare predicted CPU slices with actual reviewed ROM bank-program returns."""

import ctypes as c
import hashlib
import itertools
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R7, UC_ARM_REG_SP


class Effects(c.Structure):
    _fields_ = [("state", c.c_uint8 * 0x90), ("continuation", c.c_uint32)]


Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))
Dependency = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint8)


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


def verify_bank_cpu(lib, read, ram, flash, cpu):
    lib.hpclang_rpl_bank_cpu_predict.argtypes = [
        c.POINTER(c.c_uint8),
        c.c_uint32,
        c.POINTER(Effects),
    ]
    lib.hpclang_rpl_bank_cpu_capture_predict.argtypes = [
        c.POINTER(Reader),
        c.c_uint32,
        c.c_uint,
        c.POINTER(Effects),
        Dependency,
        c.c_void_p,
    ]
    digest = hashlib.sha256()
    count = 0
    for bank, p, cursor, pattern in itertools.product(range(14), (0, 1), range(8), (0, 1)):
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        for address, data in ((0, flash), (0x08000000, ram)):
            uc.mem_map(address, len(data))
            uc.mem_write(address, data)
        uc.mem_map(0x300000, 0x10000)
        for i in range(9):
            value = (0xABCDEF1234567890 if pattern else 0x123456789ABCDEF0) ^ (
                i * 0x1111111111111111
            )
            uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", value))
        for offset, value in (
            (0x954, 0xA1234),
            (0x958, 0xB5678),
            (0x95C, p),
            (0x970, pattern),
            (0x998, cursor),
            (0xC24, 0),
        ):
            uc.mem_write(cpu + offset, struct.pack("<I", value))
        stack = [0xA1000 + 0x101 * i for i in range(8)]
        stack[(cursor - 1) & 7] = 0x3130
        uc.mem_write(cpu + 0x978, struct.pack("<8I", *stack))
        for reg, value in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, 0xFFFFF),
            (UC_ARM_REG_R7, read(0x26BBB + 7 * bank)),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(reg, value)
        before = bytes(uc.mem_read(cpu + 0x90C, 0x90))
        captured = bytes(uc.mem_read(cpu, 0xC60))
        returned = []

        def stop(_, pc, size, user):
            if pc == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
                returned.append(True)
                uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, stop)
        uc.emu_start(0x28828, 0x400000, count=100000)
        assert returned, (bank, p, cursor, pattern)
        after = bytes(uc.mem_read(cpu + 0x90C, 0x90))
        assert bytes(uc.mem_read(0, len(flash))) == flash
        stack[(cursor - 1) & 7] = 0
        assert struct.unpack("<8I", uc.mem_read(cpu + 0x978, 32)) == tuple(stack)
        assert struct.unpack("<I", uc.mem_read(cpu + 0x998, 4))[0] == (cursor - 1) & 7
        # Input table value is independent of the resulting CPU registers.
        configuration = read(0x860B8 if p else 0x860CC + 5 * bank)
        output = Effects()
        state = (c.c_uint8 * 0x90).from_buffer_copy(before)
        assert lib.hpclang_rpl_bank_cpu_predict(state, configuration, c.byref(output)) == 0
        assert bytes(output.state) == after and output.continuation == 0x3130
        observed = []

        @Read
        def native(_, address, value):
            if address >= 2 * len(captured):
                return 0
            value[0] = captured[address // 2] >> (4 * (address & 1)) & 15
            return 1

        @Dependency
        def dependency(_, address, value):
            assert value == captured[address // 2] >> (4 * (address & 1)) & 15
            observed.append(address)
            return 0

        guarded = Effects()
        reader = Reader(native, None)
        assert (
            lib.hpclang_rpl_bank_cpu_capture_predict(
                c.byref(reader), configuration, p, c.byref(guarded), dependency, None
            )
            == 0
        )
        assert bytes(guarded) == bytes(output)
        assert observed == list(range(0xC24 * 2, 0xC28 * 2)) + list(range(0x90C * 2, 0x99C * 2))
        digest.update(struct.pack("<4I", bank, p, cursor, pattern) + before + after)
        count += 1
    return dict(
        cases=count,
        guarded_nibbles_per_case=296,
        compared_state_bytes=0x90,
        cpu_cases_sha256=digest.hexdigest(),
        consumed_return_slot_cleared=True,
        other_return_slots_preserved=True,
        scope="Controlled caller registers with pending Saturn interrupt cleared; not execution authorization",
    )
