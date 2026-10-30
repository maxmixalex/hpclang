"""Compose alias selector resolution with mapping-qualified dependency validation."""

import ctypes as c


def verify_alias_selectors(lib, read, ram, flash, cpu, banks, profile):
    read_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))
    mapped_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint32, c.POINTER(c.c_uint8))
    dependency_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint8)

    class Reader(c.Structure):
        _fields_ = [("read", read_fn), ("context", c.c_void_p)]

    class MappedReader(c.Structure):
        _fields_ = [("read", mapped_fn), ("context", c.c_void_p)]

    class Program(c.Structure):
        _fields_ = [(name, c.c_uint32) for name in ("selector", "dispatch", "body")]

    class Route(c.Structure):
        _fields_ = [(name, c.c_uint32) for name in ("flags", "io", "ram", "eram", "port")]

    lib.hpclang_rpl_stock_bank_route_capture.argtypes = [
        c.POINTER(Reader),
        c.POINTER(Route),
        dependency_fn,
        c.c_void_p,
    ]
    lib.hpclang_rpl_stock_bank_route_check.argtypes = [c.POINTER(Route), c.c_uint32, c.c_uint32]

    class MapState(c.Structure):
        _fields_ = [("pages", c.c_uint32 * 256), ("owners", c.c_uint8 * 256)]
        _fields_ += [
            (name, c.c_uint32) for name in ("lower", "upper", "overlay_page", "overlay_physical")
        ]

    class Effects(c.Structure):
        _fields_ = [
            ("map", MapState),
            ("sentinel", c.c_uint32),
            ("writes", c.c_uint32 * 256),
            ("cache", c.c_uint32 * 4),
        ]

    lib.hpclang_rpl_bank_effects_capture.argtypes = [
        c.POINTER(Reader),
        c.POINTER(Effects),
        dependency_fn,
        c.c_void_p,
    ]

    class Geometry(c.Structure):
        _fields_ = [("physical", c.c_uint32), ("bytes", c.c_uint32)]

    lib.hpclang_rpl_bank_effects_select.argtypes = [
        c.POINTER(Effects),
        c.POINTER(Geometry),
        c.c_uint32,
        c.c_uint32,
        c.POINTER(Effects),
    ]
    lib.hpclang_rpl_rom_bank_capture_guarded.argtypes = [
        c.POINTER(Reader),
        c.POINTER(Reader),
        c.c_uint32,
        c.c_uint32,
        c.c_uint32,
        c.POINTER(Geometry),
        dependency_fn,
        c.c_void_p,
        dependency_fn,
        c.c_void_p,
    ]

    from rom_profile import Profile

    class Sources(c.Structure):
        _fields_ = [(name, c.POINTER(Reader)) for name in ("saturn", "native", "rom")]
        _fields_ += [("profile", c.POINTER(Profile))]
        _fields_ += [
            (name, c.c_uint32)
            for name in (
                "saturn_mapping",
                "native_mapping",
                "rom_mapping",
                "code_first",
                "code_end",
                "configurations",
            )
        ]

    class Plan(c.Structure):
        _fields_ = [
            ("program", Program),
            ("bank", c.c_uint32),
            ("configuration", c.c_uint32),
            ("geometry", Geometry),
            ("effects", Effects),
        ]

    class Workspace(c.Structure):
        _fields_ = [("prior", Effects), ("banks", Geometry * 16), ("next", Plan)]

    plan_dependency = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint32, c.c_uint8)
    lib.hpclang_rpl_bank_plan_prepare.argtypes = [
        c.POINTER(Sources),
        c.c_uint32,
        c.POINTER(Workspace),
        c.POINTER(Plan),
        plan_dependency,
        c.c_void_p,
    ]

    class Guard(c.Structure):
        _fields_ = [(name, c.c_uint32) for name in ("mapping", "field", "expected", "width")]

    class Builder(c.Structure):
        _fields_ = [
            ("guards", c.POINTER(Guard)),
            ("capacity", c.c_uint32),
            ("count", c.c_uint32),
            ("error", c.c_int),
        ]

    lib.hpclang_rpl_flashptr_bank_guarded.argtypes = [
        c.POINTER(Reader),
        *([c.c_uint32] * 3),
        c.POINTER(c.c_uint32),
        dependency_fn,
        c.c_void_p,
    ]
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
    lib.hpclang_rpl_reference_guards_begin.argtypes = [
        c.POINTER(Builder),
        c.POINTER(Guard),
        c.c_uint32,
    ]
    lib.hpclang_rpl_reference_guards_observe.argtypes = [
        c.POINTER(Builder),
        c.c_uint32,
        c.c_uint32,
        c.c_uint8,
    ]
    lib.hpclang_rpl_reference_guards_validate.argtypes = [
        c.POINTER(MappedReader),
        c.POINTER(Guard),
        c.c_uint32,
    ]
    changed = {}

    @read_fn
    def nibble(_, address, value):
        value[0] = changed.get((1, address), read(address, 1))
        return 1

    def state_value(address):
        return (ram[cpu - 0x08000000 + address // 2] >> (4 * (address & 1))) & 15

    @read_fn
    def native(_, address, value):
        value[0] = changed.get((2, address), state_value(address))
        return 1

    def rom_value(address):
        return (flash[address // 2] >> (4 * (address & 1))) & 15

    @read_fn
    def physical_rom(_, address, value):
        value[0] = changed.get((3, address), rom_value(address))
        return 1

    @mapped_fn
    def mapped(_, mapping, address, value):
        return (
            nibble(None, address, value)
            if mapping == 1
            else native(None, address, value)
            if mapping == 2
            else physical_rom(None, address, value)
            if mapping == 3
            else 0
        )

    reader, mapped_reader = Reader(nibble, None), MappedReader(mapped, None)
    rows = []
    # Derive D0 from validated program syntax on every capture. Code and mutable
    # data share one guard set. Interpreter/device identity remains external.
    for switch, selector, expected in ((0x22E83, 1, 2), (0x22E99, 3, 4)):
        guards, builder = (Guard * 1024)(), Builder()
        assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 1024) == 0
        observed = []

        @dependency_fn
        def observe(_, address, value):
            observed.append((1, address))
            return lib.hpclang_rpl_reference_guards_observe(c.byref(builder), 1, address, value)

        @dependency_fn
        def observe_native(_, address, value):
            observed.append((2, address))
            return lib.hpclang_rpl_reference_guards_observe(c.byref(builder), 2, address, value)

        @dependency_fn
        def observe_rom(_, address, value):
            observed.append((3, address))
            return lib.hpclang_rpl_reference_guards_observe(c.byref(builder), 3, address, value)

        bank = c.c_uint32(99)

        def resolve():
            program = Program()
            result = lib.hpclang_rpl_code_bank_entry_inspect(
                c.byref(reader), 0, 0x40000, switch, 0, c.byref(program), observe, None
            )
            if result:
                return result
            assert program.selector == 0x8605E + selector * 5
            result = lib.hpclang_rpl_flashptr_bank_guarded(
                c.byref(reader), program.selector, 0x860CC, 0, c.byref(bank), observe, None
            )
            if result != 1:
                return result
            effects = Effects()
            result = lib.hpclang_rpl_bank_effects_capture(
                c.byref(Reader(native, None)), c.byref(effects), observe_native, None
            )
            window = effects.map
            if result:
                return result
            for i in range(256):
                assert window.pages[i] == sum(state_value(i * 8 + j) << (4 * j) for j in range(8))
                assert window.owners[i] == ram[cpu - 0x08000000 + 0x808 + i]
            route = Route()
            result = lib.hpclang_rpl_stock_bank_route_capture(
                c.byref(Reader(native, None)), c.byref(route), observe_native, None
            )
            if result:
                return result
            value = sum(
                changed.get((1, program.selector + i), read(program.selector + i, 1)) << (4 * i)
                for i in range(5)
            )
            result = lib.hpclang_rpl_stock_bank_route_check(c.byref(route), program.selector, value)
            if result:
                return result
            geometries = (Geometry * 16)()
            result = lib.hpclang_rpl_rom_bank_capture_guarded(
                c.byref(Reader(physical_rom, None)),
                c.byref(Reader(native, None)),
                profile["geometry_operands"],
                profile["geometry_bases"],
                profile["flash_bytes"],
                geometries,
                observe_rom,
                None,
                observe_native,
                None,
            )
            if result:
                return result
            assert [(g.physical, g.bytes) for g in geometries] == [
                (row["physical"] or 0, row["valid_bytes"]) for row in banks[:16]
            ]
            selected = Effects()
            lower = (value >> 5) & 3
            result = lib.hpclang_rpl_bank_effects_select(
                c.byref(effects), geometries, lower, bank.value, c.byref(selected)
            )
            predicted = selected.map.pages
            if result:
                return result
            if (lower, bank.value) == (window.lower, window.upper):
                assert list(predicted) == list(window.pages)
            else:
                expected_pages = [
                    window.pages[i]
                    if window.owners[i]
                    else banks[bank.value if (i // 64) & 1 else lower]["pages"][i % 64]
                    for i in range(256)
                ]
                if window.overlay_page:
                    expected_pages[window.overlay_page : window.overlay_page + 2] = [
                        window.overlay_physical,
                        window.overlay_physical + 0x800,
                    ]
                assert list(predicted) == expected_pages
            native_reader, rom_reader = Reader(native, None), Reader(physical_rom, None)
            qualified = Profile(**profile)
            sources = Sources(
                c.pointer(reader),
                c.pointer(native_reader),
                c.pointer(rom_reader),
                c.pointer(qualified),
                1,
                2,
                3,
                0,
                0x40000,
                0x860CC,
            )
            workspace, plan = Workspace(), Plan()

            @plan_dependency
            def plan_observe(_, mapping, address, value):
                observed.append((mapping, address))
                return lib.hpclang_rpl_reference_guards_observe(
                    c.byref(builder), mapping, address, value
                )

            assert (
                lib.hpclang_rpl_bank_plan_prepare(
                    c.byref(sources), switch, c.byref(workspace), c.byref(plan), plan_observe, None
                )
                == 0
            )
            assert bytes(plan.program) == bytes(program) and plan.bank == bank.value
            assert plan.configuration == value and bytes(plan.geometry) == bytes(
                geometries[bank.value]
            )
            assert bytes(plan.effects) == bytes(selected)
            for fault_mapping, fault_address in (
                (1, switch),
                (1, program.selector),
                (2, 0xB9C * 2),
                (3, profile["geometry_operands"] * 2),
                (2, 0),
                (2, 0x400 * 2),
                (2, 0x404 * 2),
                (2, 0xC4C * 2),
            ):
                for fault in (-73, 73):

                    @plan_dependency
                    def fail(_, mapping, address, value):
                        return fault if (mapping, address) == (fault_mapping, fault_address) else 0

                    c.memset(c.byref(plan), 0xA5, c.sizeof(plan))
                    untouched = bytes(plan)
                    status = lib.hpclang_rpl_bank_plan_prepare(
                        c.byref(sources), switch, c.byref(workspace), c.byref(plan), fail, None
                    )
                    assert status < 0 and (status == -73 if fault < 0 else status != 73)
                    assert bytes(plan) == untouched
            # A unique table match alone cannot override the control-bit meaning.
            saved_changes = changed.copy()
            other = 4 if bank.value == 2 else 2
            for i in range(5):
                changed[(1, 0x860CC + bank.value * 5 + i)] = read(0x860CC + other * 5 + i, 1)
                changed[(1, 0x860CC + other * 5 + i)] = read(0x860CC + bank.value * 5 + i, 1)
            accept = plan_dependency(lambda *_: 0)
            assert (
                lib.hpclang_rpl_bank_plan_prepare(
                    c.byref(sources), switch, c.byref(workspace), c.byref(plan), accept, None
                )
                < 0
            )
            assert bytes(plan) == untouched
            changed.clear()
            changed.update(saved_changes)
            sources.native_mapping = sources.saturn_mapping
            assert (
                lib.hpclang_rpl_bank_plan_prepare(
                    c.byref(sources), switch, c.byref(workspace), c.byref(plan), plan_observe, None
                )
                < 0
            )
            assert bytes(plan) == untouched
            return 1

        assert resolve() == 1
        dependencies = sorted(set(observed))
        assert bank.value == expected and len(dependencies) == 5024 and builder.count <= 1024
        assert (
            lib.hpclang_rpl_reference_guards_validate(c.byref(mapped_reader), guards, builder.count)
            == 0
        )
        for mapping, address in dependencies:
            changed[(mapping, address)] = (
                read(address, 1)
                if mapping == 1
                else state_value(address)
                if mapping == 2
                else rom_value(address)
            ) ^ 1
            assert (
                lib.hpclang_rpl_reference_guards_validate(
                    c.byref(mapped_reader), guards, builder.count
                )
                < 0
            )
            changed.clear()
        # A legitimate configuration change must invalidate the old result,
        # then resolve the new unique bank when captured afresh.
        alternate = 4 if expected == 2 else 2
        field = 0x8605E + selector * 5
        for i in range(5):
            changed[(1, field + i)] = read(0x860CC + alternate * 5 + i, 1)
        assert (
            lib.hpclang_rpl_reference_guards_validate(c.byref(mapped_reader), guards, builder.count)
            < 0
        )
        observed.clear()
        assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 1024) == 0
        assert resolve() == 1
        assert bank.value == alternate
        assert (
            lib.hpclang_rpl_reference_guards_validate(c.byref(mapped_reader), guards, builder.count)
            == 0
        )
        changed.clear()
        assert (
            lib.hpclang_rpl_reference_guards_validate(c.byref(mapped_reader), guards, builder.count)
            < 0
        )
        rows.append(
            dict(
                switch=switch,
                selector=selector,
                original_bank=expected,
                changed_bank=alternate,
                stale_nibble_rejections=len(dependencies),
                guards=builder.count,
                runtime_plan=True,
                prepare_failure_controls=36,
            )
        )
    entries = []
    native_reader, rom_reader = Reader(native, None), Reader(physical_rom, None)
    qualified = Profile(**profile)
    sources = Sources(
        c.pointer(reader),
        c.pointer(native_reader),
        c.pointer(rom_reader),
        c.pointer(qualified),
        1,
        2,
        3,
        0,
        0x40000,
        0x860CC,
    )
    for index in range(16):
        switch = read(0x26BBB + index * 7)
        observed = {}

        @plan_dependency
        def capture(_, mapping, address, value):
            key = (mapping, address)
            if key in observed and observed[key] != value:
                return -73
            observed[key] = value
            return 0

        plan, workspace = Plan(), Workspace()
        c.memset(c.byref(plan), 0xA5, c.sizeof(plan))
        before = bytes(plan)
        result = lib.hpclang_rpl_bank_plan_prepare(
            c.byref(sources), switch, c.byref(workspace), c.byref(plan), capture, None
        )
        if banks[index]["populated"]:
            assert result == 0 and plan.bank == index, (index, hex(switch), result)
            assert plan.program.selector == 0x860CC + index * 5
            assert plan.configuration == read(plan.program.selector)
            assert (plan.geometry.physical, plan.geometry.bytes) == (
                banks[index]["physical"],
                banks[index]["valid_bytes"],
            )
            assert plan.effects.map.upper == index
            # A valid predicted payload does not authorize remapping the body.
            # Force a transition and an unowned body page with a different
            # incoming physical address; all syntax and geometry stay valid.
            body_page = plan.program.body >> 12
            for offset, width, value in (
                (body_page * 4, 8, 0x08070000),
                (0x808 + body_page, 2, 0),
                (0xBD0, 8, (index + 1) % 14),
            ):
                for j in range(width):
                    changed[(2, offset * 2 + j)] = (value >> (4 * j)) & 15
            observed.clear()
            c.memset(c.byref(plan), 0xA5, c.sizeof(plan))
            assert (
                lib.hpclang_rpl_bank_plan_prepare(
                    c.byref(sources), switch, c.byref(workspace), c.byref(plan), capture, None
                )
                < 0
            )
            assert bytes(plan) == before
            assert workspace.next.effects.map.pages[body_page] != 0x08070000
            changed.clear()
            observed.clear()
            assert (
                lib.hpclang_rpl_bank_plan_prepare(
                    c.byref(sources), switch, c.byref(workspace), c.byref(plan), capture, None
                )
                == 0
            )
        else:
            assert result < 0 and bytes(plan) == before
        entries.append(
            dict(
                bank=index,
                entry=switch,
                result=result,
                populated=banks[index]["populated"],
                dependencies=len(observed),
                body_remap_rejected=bool(banks[index]["populated"]),
            )
        )
    return rows, entries
