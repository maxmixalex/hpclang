"""Map configured catalog roots; do not infer startup execution safety."""

import ctypes as c
import struct


def verify_startup_configurations(lib, read, ram, flash, cpu, profile, select):
    read_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))

    class Reader(c.Structure):
        _fields_ = [("read", read_fn), ("context", c.c_void_p)]

    class Entry(c.Structure):
        _fields_ = [("kind", c.c_int), ("dispatch", c.c_uint32), ("span", c.c_uint32)]

    lib.hpclang_rpl_library_configuration.argtypes = [
        c.POINTER(Reader),
        *([c.c_uint32] * 4),
        c.POINTER(c.c_uint32),
    ]
    lib.hpclang_rpl_entry_inspect.argtypes = [
        c.POINTER(Reader),
        c.c_uint32,
        c.c_uint32,
        c.POINTER(Entry),
    ]
    base = list(struct.unpack_from("<256I", ram, cpu - 0x08000000))
    rows = []
    for index in range(read(0x8611D, 3)):
        slot = 0x86120 + index * 16
        number, header, switch = read(slot, 3), read(slot + 3), read(slot + 8)
        if number >= 769:
            continue
        pages = select(switch) if switch else base
        observed = {}

        def physical(at):
            return pages[at >> 12] + (at & 4095) // 2

        @read_fn
        def mapped(_, at, value):
            if at >= 0x100000:
                return 0
            location = physical(at)
            data, offset = (
                (ram, location - 0x08000000) if location >= 0x08000000 else (flash, location)
            )
            if not 0 <= offset < len(data):
                return 0
            value[0] = (data[offset] >> (4 * (at & 1))) & 15
            observed[at] = value[0]
            return 1

        reader = Reader(mapped, None)
        target = c.c_uint32(0xDEADBEEF)
        result = lib.hpclang_rpl_library_configuration(
            c.byref(reader), 0, 0x100000, header, number, c.byref(target)
        )
        assert result in (0, 1), (number, result)
        row = dict(library=number, header=header, switch=switch, configuration=None)
        if not result:
            assert target.value == 0xDEADBEEF
        else:
            entry = Entry()
            assert (
                lib.hpclang_rpl_entry_inspect(
                    c.byref(reader), target.value, 0x100000 - target.value, c.byref(entry)
                )
                == 0
            )
            assert entry.kind == 0 and entry.dispatch == 0x2D9D and entry.span > 0
            locations = [physical(at) for at in range(target.value, target.value + entry.span)]
            assert all(at < len(flash) for at in locations)
            row.update(
                configuration=target.value,
                span=entry.span,
                physical_byte=physical(target.value),
                mutable_user_flash=any(at >= profile["immutable_bytes"] for at in locations),
                observed_nibbles=len(observed),
                execution_qualified=False,
            )
        rows.append(row)
    configured = [row for row in rows if row["configuration"] is not None]
    # The maintained Stock launch fixture additionally installs ARMToolbox 275.
    launcher = [275] if any(row["library"] == 275 for row in rows) else []
    assert len(rows) == 59 + len(launcher)
    assert [row["library"] for row in configured] == [
        0,
        1,
        2,
        12,
        13,
        171,
        221,
        222,
        223,
        226,
        227,
        229,
        255,
        256,
    ] + launcher
    assert [row["library"] for row in configured if row["mutable_user_flash"]] == [
        226,
        227,
        229,
    ] + launcher
    return rows
