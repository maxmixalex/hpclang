"""Compare full read-map prediction with the original ARM bank-control handler."""

import ctypes as c
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR


def expected_effects(before, cpu_offset, state, banks, lower, upper):
    """Model the reviewed handler's writes, including conditional overlay caches."""
    expected = bytearray(before)
    writes = set()

    def word(offset):
        return struct.unpack_from("<I", expected, cpu_offset + offset)[0]

    def put(offset, value):
        struct.pack_into("<I", expected, cpu_offset + offset, value)
        writes.update(range(offset, offset + 4))

    if (lower, upper) == (state.lower, state.upper):
        return expected, writes
    put(0xBCC, lower)
    put(0xBD0, upper)
    for page in range(256):
        if state.owners[page] == 0:
            bank = upper if page & 64 else lower
            put(page * 4, banks[bank]["physical"] + (page % 64) * 0x800)
    # Read[256] precedes the write-map entries; publish before overlay repair.
    put(0x400, word(0))
    page = state.overlay_page
    if page:
        for i in range(2):
            offset = (page + i) * 4
            physical = state.overlay_physical + i * 0x800
            if word(offset) != physical:
                put(0xC4C + i * 4, word(offset))
                put(offset, physical)
                put(0xC54 + i * 4, word(offset + 0x404))
                put(offset + 0x404, physical)
    return expected, writes


def verify_bank_map(lib, ram, flash, cpu, banks):
    class Geometry(c.Structure):
        _fields_ = [("physical", c.c_uint32), ("bytes", c.c_uint32)]

    class State(c.Structure):
        _fields_ = [("pages", c.c_uint32 * 256), ("owners", c.c_uint8 * 256)]
        _fields_ += [
            (name, c.c_uint32) for name in ("lower", "upper", "overlay_page", "overlay_physical")
        ]

    class Effects(c.Structure):
        _fields_ = [
            ("map", State),
            ("sentinel", c.c_uint32),
            ("writes", c.c_uint32 * 256),
            ("cache", c.c_uint32 * 4),
        ]

    Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))
    Dependency = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint8)

    class Reader(c.Structure):
        _fields_ = [("read", Read), ("context", c.c_void_p)]

    lib.hpclang_rpl_bank_effects_capture.argtypes = [
        c.POINTER(Reader),
        c.POINTER(Effects),
        Dependency,
        c.c_void_p,
    ]

    lib.hpclang_rpl_bank_effects_select.argtypes = [
        c.POINTER(Effects),
        c.POINTER(Geometry),
        c.c_uint32,
        c.c_uint32,
        c.POINTER(Effects),
    ]

    lib.hpclang_rpl_stock_bank_map_select.argtypes = [
        c.POINTER(State),
        c.POINTER(Geometry),
        c.c_uint32,
        c.c_uint32,
        c.POINTER(c.c_uint32),
    ]
    lib.hpclang_rpl_execution_mapping_preserved.argtypes = [
        c.POINTER(c.c_uint32),
        c.POINTER(c.c_uint32),
        c.c_uint32,
        c.c_uint32,
    ]
    geometry = (Geometry * 16)(
        *(Geometry(row["physical"] or 0, row["valid_bytes"]) for row in banks[:16])
    )
    results = []
    for context in (
        "baseline",
        "owners",
        "overlay",
        "overlay-matches",
        "overlay-first",
        "overlay-second",
    ):
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
                if context.startswith("overlay"):
                    # Cross a quadrant boundary and override both owner meanings.
                    uc.mem_write(cpu + 0xC48, struct.pack("<I", 63))
                    uc.mem_write(cpu + 0xC5C, struct.pack("<I", 0x08060000))
                if context == "overlay-matches":
                    uc.mem_write(cpu + 0x808 + 63, bytes((2, 2)))
                    uc.mem_write(cpu + 63 * 4, struct.pack("<2I", 0x08060000, 0x08060800))
                if context in ("overlay-first", "overlay-second"):
                    i = int(context == "overlay-second")
                    uc.mem_write(cpu + 0x808 + 63 + i, bytes((2,)))
                    uc.mem_write(cpu + (63 + i) * 4, struct.pack("<I", 0x08060000 + i * 0x800))
                before = bytes(uc.mem_read(0x08000000, len(ram)))
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
                effects = Effects()
                effects.map = state
                effects.sentinel = struct.unpack("<I", uc.mem_read(cpu + 0x400, 4))[0]
                effects.writes[:] = struct.unpack("<256I", uc.mem_read(cpu + 0x404, 1024))
                effects.cache[:] = struct.unpack("<4I", uc.mem_read(cpu + 0xC4C, 16))
                dependencies = {}

                @Read
                def read_state(_, address, value):
                    if address >= 0xC60 * 2:
                        return 0
                    value[0] = (before[cpu - 0x08000000 + address // 2] >> (4 * (address & 1))) & 15
                    return 1

                @Dependency
                def observe_state(_, address, value):
                    if address in dependencies:
                        return -73
                    dependencies[address] = value
                    return 0

                reader = Reader(read_state, None)
                captured = Effects()
                assert (
                    lib.hpclang_rpl_bank_effects_capture(
                        c.byref(reader), c.byref(captured), observe_state, None
                    )
                    == 0
                )
                assert bytes(captured) == bytes(effects) and len(dependencies) == 4680
                effects = captured
                selected = Effects()
                c.memset(c.byref(selected), 0xA5, c.sizeof(selected))
                untouched = bytes(selected)
                status = lib.hpclang_rpl_bank_effects_select(
                    c.byref(effects), geometry, lower, upper, c.byref(selected)
                )
                if not banks[upper]["populated"]:
                    assert result < 0 and list(predicted) == [0xDEADBEEF] * 256
                    assert status < 0 and bytes(selected) == untouched
                    results.append(dict(context=context, lower=lower, upper=upper, rejected=True))
                    continue
                assert result == 0 and status == 0
                inplace = Effects.from_buffer_copy(bytes(effects))
                assert (
                    lib.hpclang_rpl_bank_effects_select(
                        c.byref(inplace), geometry, lower, upper, c.byref(inplace)
                    )
                    == 0
                )
                assert bytes(inplace) == bytes(selected)
                for register, value in (
                    (UC_ARM_REG_R0, cpu),
                    (UC_ARM_REG_R1, (lower << 5) | (upper << 1)),
                    (UC_ARM_REG_SP, 0x308000),
                    (UC_ARM_REG_LR, 0x300000),
                ):
                    uc.reg_write(register, value)
                expected, allowed = expected_effects(
                    before, cpu - 0x08000000, state, banks, lower, upper
                )
                returned = []
                writes = []

                def observe_write(_, access, address, size, value, user):
                    if 0x307FDC <= address and address + size <= 0x308000:
                        return  # Nine-register ARM prologue save.
                    assert all(address + i - cpu in allowed for i in range(size)), (
                        context,
                        lower,
                        upper,
                        hex(address),
                        size,
                    )
                    writes.append((address - cpu, size))

                uc.hook_add(UC_HOOK_MEM_WRITE, observe_write)

                def stop(_, pc, size, user):
                    if pc == 0x300000:
                        returned.append(True)
                        uc.emu_stop()

                uc.hook_add(UC_HOOK_CODE, stop)
                uc.emu_start(0x248DC, 0x400000, count=100000)
                assert returned
                actual = list(struct.unpack("<256I", uc.mem_read(cpu, 1024)))
                assert actual == list(predicted) == list(selected.map.pages), (
                    context,
                    lower,
                    upper,
                )
                assert selected.sentinel == struct.unpack("<I", uc.mem_read(cpu + 0x400, 4))[0]
                assert list(selected.writes) == list(
                    struct.unpack("<256I", uc.mem_read(cpu + 0x404, 1024))
                )
                assert list(selected.cache) == list(
                    struct.unpack("<4I", uc.mem_read(cpu + 0xC4C, 16))
                )
                assert (selected.map.lower, selected.map.upper) == (lower, upper)
                assert list(selected.map.owners) == list(state.owners)
                assert (selected.map.overlay_page, selected.map.overlay_physical) == (
                    state.overlay_page,
                    state.overlay_physical,
                )
                assert struct.unpack("<2I", uc.mem_read(cpu + 0xBCC, 8)) == (lower, upper)
                assert bytes(uc.mem_read(0, len(flash))) == flash
                assert bytes(uc.mem_read(0x08000000, len(ram))) == expected, (
                    context,
                    lower,
                    upper,
                    "unexpected SRAM effects",
                )
                # The shared switch body remains an execution obligation even
                # when the selected payload itself has a valid physical extent.
                preserved = lib.hpclang_rpl_execution_mapping_preserved(
                    state.pages, selected.map.pages, 0x2305A, 29
                )
                expected_preserved = state.pages[0x23] == actual[0x23]
                assert (preserved == 0) == expected_preserved
                # Also exercise a range crossing the overlay quadrant boundary.
                crossing = lib.hpclang_rpl_execution_mapping_preserved(
                    state.pages, selected.map.pages, 0x3FFF0, 32
                )
                assert (crossing == 0) == all(state.pages[i] == actual[i] for i in (63, 64))
                results.append(
                    dict(
                        context=context,
                        lower=lower,
                        upper=upper,
                        rejected=False,
                        complete_sram_effects=True,
                        runtime_effects_match=True,
                        captured_dependencies=len(dependencies),
                        switch_body_mapping_preserved=preserved == 0,
                        state_write_events=len(writes),
                        unchanged_selectors=(lower, upper) == (state.lower, state.upper),
                    )
                )
    return results
