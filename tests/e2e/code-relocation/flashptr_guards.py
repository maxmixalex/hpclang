"""Guard composition against reviewed HP49g+/HP50g saved-state ROM tables."""

import ctypes as c


def verify_flashptr(lib, read, flash, mappings):
    read_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))
    mapped_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint32, c.POINTER(c.c_uint8))
    dependency_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.c_uint8)

    class Reader(c.Structure):
        _fields_ = [("read", read_fn), ("context", c.c_void_p)]

    class MappedReader(c.Structure):
        _fields_ = [("read", mapped_fn), ("context", c.c_void_p)]

    class Pointer(c.Structure):
        _fields_ = [("selector", c.c_uint32), ("index", c.c_uint32)]

    class Geometry(c.Structure):
        _fields_ = [("physical", c.c_uint32), ("size", c.c_uint32)]

    class BankMemory(c.Structure):
        _fields_ = [("bytes", c.POINTER(c.c_uint8)), ("size", c.c_uint32)]

    lib.hpclang_rpl_stock_rom_bank_capture.argtypes = [
        c.POINTER(BankMemory),
        c.POINTER(Reader),
        c.POINTER(c.c_uint8),
        c.POINTER(Geometry),
        c.POINTER(c.c_uint32),
    ]
    flash_storage = (c.c_uint8 * len(flash)).from_buffer_copy(flash)
    # Host capture tests the same permission gates as Stock, with explicit
    # identity descriptors. This does not inspect the live machine's L1 table.
    sections = (c.c_uint32 * 2)(0x802, 0x100802)
    bank_memory, bank_reader = BankMemory(), Reader()

    class Guard(c.Structure):
        _fields_ = [(key, c.c_uint32) for key in ("mapping", "field", "expected", "width")]

    class Builder(c.Structure):
        _fields_ = [
            ("guards", c.POINTER(Guard)),
            ("capacity", c.c_uint32),
            ("count", c.c_uint32),
            ("error", c.c_int),
        ]

    dependency = [dependency_fn, c.c_void_p]
    lib.hpclang_rpl_flashptr_decode_guarded.argtypes = [
        c.POINTER(Reader),
        c.c_uint32,
        c.c_uint32,
        c.POINTER(Pointer),
        *dependency,
    ]
    lib.hpclang_rpl_flashptr_bank_guarded.argtypes = [
        c.POINTER(Reader),
        *([c.c_uint32] * 3),
        c.POINTER(c.c_uint32),
        *dependency,
    ]
    lib.hpclang_rpl_flashptr_lookup_guarded.argtypes = [
        c.POINTER(Reader),
        *([c.c_uint32] * 3),
        c.POINTER(c.c_uint32),
        *dependency,
    ]
    lib.hpclang_rpl_flashptr_table_lookup_guarded.argtypes = [
        c.POINTER(Reader),
        *([c.c_uint32] * 4),
        c.POINTER(c.c_uint32),
        *dependency,
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
    expected_banks = {1: 2, 2: 3, 3: 4, 4: 5, 6: 6, 7: 7, 15: 0}
    configurations = [read(0x860CC + 5 * i) for i in range(16)]
    for selector in range(16):
        configuration = read(0x8605E + 5 * selector)
        matches = (
            [i for i, value in enumerate(configurations) if value == configuration]
            if configuration
            else []
        )
        assert matches == ([expected_banks[selector]] if selector in expected_banks else [])

    guards, builder = (Guard * 23)(), Builder()
    source, observed, corrupt = [], [], None
    callback_errors = []
    active = None

    def value(mapping, address):
        if mapping == 0:
            assert 0 <= address < len(source)
            result = source[address]
        elif mapping == 1:
            result = read(address, 1)
        else:
            assert mapping == 2 and 0x40000 <= address < 0x40000 + active["valid_bytes"] * 2
            offset = active["physical"] + (address - 0x40000) // 2
            nibble = c.c_uint8(99)
            assert bank_reader.read(bank_reader.context, address, c.byref(nibble)) == 1
            result = nibble.value
            assert result == (flash[offset] >> (4 * (address & 1))) & 15
        return result ^ (1 if corrupt == (mapping, address) else 0)

    def make_read(mapping):
        @read_fn
        def callback(_, address, out):
            try:
                out[0] = value(mapping, address)
                return 1
            except Exception as error:
                callback_errors.append(repr(error))
                return 0

        return callback

    def make_observer(mapping):
        @dependency_fn
        def callback(_, address, nibble):
            observed.append((mapping, address))
            return lib.hpclang_rpl_reference_guards_observe(
                c.byref(builder), mapping, address, nibble
            )

        return callback

    callbacks = [make_read(i) for i in range(3)]
    readers = [Reader(callback, None) for callback in callbacks]
    observers = [make_observer(i) for i in range(3)]

    @mapped_fn
    def validate_read(_, mapping, address, out):
        try:
            out[0] = value(mapping, address)
            return 1
        except Exception as error:
            callback_errors.append(repr(error))
            return 0

    validator = MappedReader(validate_read, None)
    rows = []
    for selector, bank in expected_banks.items():
        active = mappings[bank]
        assert active["populated"]
        geometry = Geometry(active["physical"], active["valid_bytes"])
        packed = c.cast(c.byref(flash_storage, active["physical"]), c.POINTER(c.c_uint8))
        assert (
            lib.hpclang_rpl_stock_rom_bank_capture(
                c.byref(bank_memory), c.byref(bank_reader), packed, c.byref(geometry), sections
            )
            == 1
        )
        for outside in (0x3FFFF, 0x40000 + active["valid_bytes"] * 2, 0x80000):
            nibble = c.c_uint8(99)
            assert bank_reader.read(bank_reader.context, outside, c.byref(nibble)) == 0
            assert nibble.value == 99
        # These locations come from the reviewed fixture handlers, not header scanning.
        header = 0x60200 if selector == 15 else 0x40200
        table = header + 29

        def word(address, width=5):
            return sum(value(2, address + i) << (4 * i) for i in range(width))

        if selector != 15:
            assert word(header, 4) == 0xF | (bank << 8)
        else:
            assert word(header, 4) == 0x18
        count = word(table)
        assert 0 < count < 0x10000 and table + 5 + 5 * count <= 0x40000 + active["valid_bytes"] * 2
        rejected, present = 0, 0
        for index in range(count):
            source = [
                (word >> (4 * j)) & 15
                for word, width in ((0x26AC, 5), (selector, 3), (index, 4))
                for j in range(width)
            ]
            observed = []
            assert lib.hpclang_rpl_reference_guards_begin(c.byref(builder), guards, 23) == 0
            pointer, resolved_bank, target = Pointer(), c.c_uint32(99), c.c_uint32(0xDEAD)
            assert (
                lib.hpclang_rpl_flashptr_decode_guarded(
                    c.byref(readers[0]), 0, 12, c.byref(pointer), observers[0], None
                )
                == 1
            )
            assert (pointer.selector, pointer.index) == (selector, index)
            assert (
                lib.hpclang_rpl_flashptr_bank_guarded(
                    c.byref(readers[1]),
                    0x8605E,
                    0x860CC,
                    pointer.selector,
                    c.byref(resolved_bank),
                    observers[1],
                    None,
                )
                == 1
            )
            assert resolved_bank.value == bank
            if selector == 15:
                result = lib.hpclang_rpl_flashptr_table_lookup_guarded(
                    c.byref(readers[2]),
                    0x40000,
                    0x40000 + active["valid_bytes"] * 2,
                    table,
                    pointer.index,
                    c.byref(target),
                    observers[2],
                    None,
                )
            else:
                result = lib.hpclang_rpl_flashptr_lookup_guarded(
                    c.byref(readers[2]),
                    header,
                    resolved_bank.value,
                    pointer.index,
                    c.byref(target),
                    observers[2],
                    None,
                )
            expected = word(table + 5 + 5 * index)
            assert expected < 0x80000
            assert result == int(bool(expected)) and target.value == (expected or 0xDEAD)
            present += bool(expected)
            expected_reads = {(0, i) for i in range(12)}
            expected_reads.update((1, 0x8605E + 5 * selector + i) for i in range(5))
            expected_reads.update((1, 0x860CC + i) for i in range(80))
            expected_reads.update((2, table + i) for i in range(5))
            expected_reads.update((2, table + 5 + 5 * index + i) for i in range(5))
            if selector != 15:
                expected_reads.update((2, header + i) for i in range(4))
            assert len(observed) == len(expected_reads) and set(observed) == expected_reads
            assert (
                lib.hpclang_rpl_reference_guards_validate(c.byref(validator), guards, builder.count)
                == 0
            )
            if index in (0, count // 2, count - 1):
                for corrupt in observed:
                    assert (
                        lib.hpclang_rpl_reference_guards_validate(
                            c.byref(validator), guards, builder.count
                        )
                        < 0
                    )
                    rejected += 1
                corrupt = None
        rows.append(
            dict(
                selector=selector,
                bank=bank,
                count=count,
                present=present,
                stale_rejections=rejected,
            )
        )
    assert bytes(flash_storage) == flash
    assert not callback_errors, callback_errors
    return rows
