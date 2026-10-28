"""Compose alias selector resolution with mapping-qualified dependency validation."""

import ctypes as c


def verify_alias_selectors(lib, read, ram, cpu):
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

    @mapped_fn
    def mapped(_, mapping, address, value):
        return (
            nibble(None, address, value)
            if mapping == 1
            else native(None, address, value)
            if mapping == 2
            else 0
        )

    reader, mapped_reader = Reader(nibble, None), MappedReader(mapped, None)
    rows = []
    # Derive D0 from validated program syntax on every capture. Code and mutable
    # data share one guard set. Interpreter/device identity remains external.
    for switch, selector, expected in ((0x22E83, 1, 2), (0x22E99, 3, 4)):
        guards, builder = (Guard * 48)(), Builder()
        assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 48) == 0
        observed = []

        @dependency_fn
        def observe(_, address, value):
            observed.append((1, address))
            return lib.hpclang_rpl_reference_guards_observe(c.byref(builder), 1, address, value)

        @dependency_fn
        def observe_native(_, address, value):
            observed.append((2, address))
            return lib.hpclang_rpl_reference_guards_observe(c.byref(builder), 2, address, value)

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
            return result if result else 1

        assert resolve() == 1
        dependencies = sorted(set(observed))
        assert bank.value == expected and len(dependencies) == 170 and builder.count <= 48
        assert (
            lib.hpclang_rpl_reference_guards_validate(c.byref(mapped_reader), guards, builder.count)
            == 0
        )
        for mapping, address in dependencies:
            changed[(mapping, address)] = (
                read(address, 1) if mapping == 1 else state_value(address)
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
        assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 48) == 0
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
            )
        )
    return rows
