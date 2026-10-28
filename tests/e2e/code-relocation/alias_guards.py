"""Compose alias selector resolution with mapping-qualified dependency validation."""

import ctypes as c


def verify_alias_selectors(lib, read):
    read_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))
    mapped_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint32, c.POINTER(c.c_uint8))
    dependency_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint8)

    class Reader(c.Structure):
        _fields_ = [("read", read_fn), ("context", c.c_void_p)]

    class MappedReader(c.Structure):
        _fields_ = [("read", mapped_fn), ("context", c.c_void_p)]

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
        value[0] = changed.get(address, read(address, 1))
        return 1

    @mapped_fn
    def mapped(_, mapping, address, value):
        return nibble(None, address, value) if mapping == 1 else 0

    reader, mapped_reader = Reader(nibble, None), MappedReader(mapped, None)
    rows = []
    # The original-ROM trace establishes D0=selectors+5*selector for these
    # entry points. This test composes data guards; it is not a code identity
    # profile and does not make the public runtime accept these addresses.
    for switch, selector, expected in ((0x22E83, 1, 2), (0x22E99, 3, 4)):
        guards, builder = (Guard * 17)(), Builder()
        assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 17) == 0
        observed = []

        @dependency_fn
        def observe(_, address, value):
            observed.append(address)
            return lib.hpclang_rpl_reference_guards_observe(c.byref(builder), 1, address, value)

        bank = c.c_uint32(99)
        assert (
            lib.hpclang_rpl_flashptr_bank_guarded(
                c.byref(reader), 0x8605E, 0x860CC, selector, c.byref(bank), observe, None
            )
            == 1
        )
        assert bank.value == expected and len(observed) == 85 and builder.count == 17
        assert (
            lib.hpclang_rpl_reference_guards_validate(c.byref(mapped_reader), guards, builder.count)
            == 0
        )
        for address in observed:
            changed[address] = read(address, 1) ^ 1
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
            changed[field + i] = read(0x860CC + alternate * 5 + i, 1)
        assert (
            lib.hpclang_rpl_reference_guards_validate(c.byref(mapped_reader), guards, builder.count)
            < 0
        )
        observed.clear()
        assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 17) == 0
        assert (
            lib.hpclang_rpl_flashptr_bank_guarded(
                c.byref(reader), 0x8605E, 0x860CC, selector, c.byref(bank), observe, None
            )
            == 1
        )
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
                stale_nibble_rejections=85,
                guards=17,
            )
        )
    return rows
