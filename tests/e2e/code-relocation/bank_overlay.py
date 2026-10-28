"""Check ERAM overlay precedence and exit behavior in the reviewed Stock ROMs."""

import ctypes as c
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
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


def verify_attachment_switches(lib, read, ram, flash, cpu, banks):
    """Fixture-specific aliases: compare maps/state, without granting runtime identity."""
    results = []
    read_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))

    class Reader(c.Structure):
        _fields_ = [("read", read_fn), ("context", c.c_void_p)]

    class Literal(c.Structure):
        _fields_ = [
            (name, c.c_uint32)
            for name in ("instruction", "length", "field", "destination", "nibbles")
        ]
        _fields_ += [("mask", c.c_uint64), ("value", c.c_uint64)]

    @read_fn
    def nibble(_, address, value):
        value[0] = read(address, 1)
        return 1

    reader = Reader(nibble, None)
    dependency_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint8)

    class Program(c.Structure):
        _fields_ = [(name, c.c_uint32) for name in ("selector", "dispatch", "body")]

    lib.hpclang_rpl_code_bank_entry_inspect.argtypes = [
        c.POINTER(Reader),
        c.c_uint32,
        c.c_uint32,
        c.c_uint32,
        c.c_uint,
        c.POINTER(Program),
        dependency_fn,
        c.c_void_p,
    ]

    class Route(c.Structure):
        _fields_ = [(name, c.c_uint32) for name in ("flags", "io", "ram", "eram", "port")]

    lib.hpclang_rpl_stock_bank_route_check.argtypes = [c.POINTER(Route), c.c_uint32, c.c_uint32]
    programs = {}
    lib.hpclang_rpl_code_literal_inspect.argtypes = [
        c.POINTER(Reader),
        c.c_uint32,
        c.c_uint32,
        c.c_uint,
        c.POINTER(Literal),
    ]

    def selector_operand(entry):
        literal = Literal()
        assert (
            lib.hpclang_rpl_code_literal_inspect(c.byref(reader), entry, 7, 0, c.byref(literal))
            == 0
        )
        assert (literal.destination, literal.length, literal.nibbles, literal.mask) == (
            11,
            7,
            5,
            0xFFFFF,
        )
        observed = set()

        @dependency_fn
        def observe(_, address, value):
            assert value == read(address, 1)
            observed.add(address)
            return 0

        program = Program()
        assert (
            lib.hpclang_rpl_code_bank_entry_inspect(
                c.byref(reader), 0, 0x40000, entry, 0, c.byref(program), observe, None
            )
            == 0
        )
        assert (program.selector, program.dispatch, program.body) == (
            literal.value,
            0x22FEC,
            0x2305A,
        )
        expected = set(range(entry, entry + 11))
        expected.update(range(program.dispatch, program.dispatch + 5))
        expected.update(range(program.body, program.body + 29))
        assert observed == expected
        programs[entry] = dict(
            dispatch=program.dispatch, body=program.body, observed_code_nibbles=len(observed)
        )
        return literal.value

    def execute(initial, entry, selector=None, ram_collision=False):
        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        for address, data in ((0, flash), (0x08000000, ram)):
            uc.mem_map(address, len(data))
            uc.mem_write(address, data)
        uc.mem_map(0x300000, 0x10000)
        if ram_collision:
            # UNCONFIG checks RAM before the bank-control device. Changing this
            # initial field alone must invalidate a claimed bank-only effect.
            assert initial is None and selector is None
            uc.mem_write(cpu + 0xBA8, struct.pack("<I", 0x7F))
        for reg, value in (
            (UC_ARM_REG_R4, cpu),
            (UC_ARM_REG_R5, 0xFFFFF),
            (UC_ARM_REG_SP, 0x308000),
        ):
            uc.reg_write(reg, value)
        returned = []
        config_stages = []
        writes = []

        def stop(_, pc, size, user):
            if pc == 0x28898:
                native_pc = uc.reg_read(UC_ARM_REG_R7)
                if native_pc in (0x23063, 0x23066, 0x2306F, 0x23075):
                    config_stages.append(
                        dict(
                            pc=native_pc,
                            flags=struct.unpack("<I", uc.mem_read(cpu + 0xB9C, 4))[0],
                            d0=struct.unpack("<I", uc.mem_read(cpu + 0x954, 4))[0],
                            c_low=struct.unpack("<I", uc.mem_read(cpu + 0x91C, 4))[0] & 0xFFFFF,
                        )
                    )
            if pc == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
                returned.append(True)
                uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, stop)

        def record_write(_, access, address, size, value, user):
            # Reviewed interpreter-state window and controlled ARM stack only.
            # This is a bounded regression, not a proof for unexecuted paths.
            assert (cpu <= address and address + size <= cpu + 0xC20) or (
                0x300000 <= address and address + size <= 0x310000
            ), (hex(address), size)
            writes.append((address, size))

        uc.hook_add(UC_HOOK_MEM_WRITE, record_write)

        def call(switch, p):
            for offset, value in ((0x998, 1), (0x978, 0x3130), (0x95C, p), (0x970, 0)):
                uc.mem_write(cpu + offset, struct.pack("<I", value))
            uc.reg_write(UC_ARM_REG_R7, switch)
            returned.clear()
            config_stages.clear()
            writes.clear()
            before_flags = struct.unpack("<I", uc.mem_read(cpu + 0xB9C, 4))[0]
            route_result = None
            if switch != banks[16]["switch"]:
                field = selector_operand(switch) if p == 0 else 0x860B8
                physical = struct.unpack("<I", uc.mem_read(cpu + (field >> 12) * 4, 4))[0]
                physical += (field & 4095) // 2
                configuration = int.from_bytes(uc.mem_read(physical, 3), "little")
                configuration = (configuration >> (4 * (field & 1))) & 0xFFFFF
                route = Route(
                    *(
                        struct.unpack("<I", uc.mem_read(cpu + offset, 4))[0]
                        for offset in (0xB9C, 0xBA0, 0xBA8, 0xBB8, 0xBC4)
                    )
                )
                route_result = lib.hpclang_rpl_stock_bank_route_check(
                    c.byref(route), field, configuration
                )
                assert (route_result == 0) == (not ram_collision)

            uc.emu_start(0x28828, 0x400000, count=100000)
            assert returned
            assert struct.unpack("<I", uc.mem_read(cpu + 0x95C, 4))[0] == 0
            assert bytes(uc.mem_read(0, len(flash))) == flash
            if switch != banks[16]["switch"]:
                assert [stage["pc"] for stage in config_stages] == [
                    0x23063,
                    0x23066,
                    0x2306F,
                    0x23075,
                ]
                assert not before_flags & 0x24
                assert [stage["flags"] for stage in config_stages] == [
                    before_flags,
                    before_flags | 0x20,
                    before_flags | 4,
                    5 if ram_collision else before_flags,
                ]
                assert config_stages[0]["c_low"] == 0xFF000
                assert config_stages[2]["d0"] == config_stages[2]["c_low"]
                assert 0x7F000 <= config_stages[2]["d0"] < 0x80000
            return dict(
                pages=list(struct.unpack("<256I", uc.mem_read(cpu, 1024))),
                owners=list(uc.mem_read(cpu + 0x808, 256)),
                flags=struct.unpack("<I", uc.mem_read(cpu + 0xB9C, 4))[0],
                banks=list(struct.unpack("<2I", uc.mem_read(cpu + 0xBCC, 8))),
                write_pages=list(struct.unpack("<256I", uc.mem_read(cpu + 0x400, 1024))),
                configuration_flags=[stage["flags"] for stage in config_stages],
                write_scope_checked=bool(writes),
                route_result=route_result,
            )

        if initial is not None:
            call(banks[initial]["switch"], 0)
        if selector is not None:
            address, value = selector
            physical = struct.unpack("<I", uc.mem_read(cpu + (address >> 12) * 4, 4))[0]
            physical += (address & 4095) // 2
            assert 0x08000000 <= physical < 0x08080000
            shift = 4 * (address & 1)
            old = uc.mem_read(physical, 1)[0]
            uc.mem_write(physical, bytes([(old & ~(15 << shift)) | (value << shift)]))
        if ram_collision:
            assert struct.unpack("<I", uc.mem_read(cpu + 0xB9C, 4))[0] == 3
            return [call(entry, 0)]
        return [call(entry, 0), call(entry, 1)]

    # These addresses are evidence for the two captured supported ROMs only.
    # A lookup must not recognize them in another mapping merely by address.
    for switch, bank in ((0x22E83, 2), (0x22E99, 4)):
        selector = selector_operand(switch)
        canonical_selector = selector_operand(banks[bank]["switch"])
        assert selector == (0x86063 if bank == 2 else 0x8606D)
        assert canonical_selector == 0x860CC + bank * 5
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
            # Entry code sets D0 to a RAM selector cell. Equal ROM code does
            # not establish equal bank selection if that mutable data changes.
            alternate = 4 if bank == 2 else 2
            changed = execute(initial, switch, (selector, alternate * 2))
            assert [stage["banks"][1] for stage in changed] == [alternate, 2]
            if initial == 16:
                assert all(stage["pages"][64:128] == expected for stage in changed)
            else:
                assert changed[0]["pages"][64:128] == banks[alternate]["pages"]
                assert changed[1]["pages"][64:128] == banks[2]["pages"]
            collision = None
            if initial is None:
                collision = execute(None, switch, ram_collision=True)[0]
                assert collision == execute(None, banks[bank]["switch"], ram_collision=True)[0]
                assert collision["banks"] == actual[0]["banks"]
                assert collision["configuration_flags"] == [3, 0x23, 7, 5]
                assert collision["flags"] != actual[0]["flags"]
                assert collision["pages"] != actual[0]["pages"]
            results.append(
                dict(
                    switch=switch,
                    bank=bank,
                    initial=initial,
                    stages=actual,
                    selector_field=selector,
                    canonical_selector_field=canonical_selector,
                    entry_program=programs[switch],
                    changed_selector_stages=changed,
                    ram_configuration_collision=collision,
                )
            )
    return results
