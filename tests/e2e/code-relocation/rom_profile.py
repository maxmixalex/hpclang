"""Identify reviewed immutable firmware without depending on mutable user Flash."""

import ctypes as c
import hashlib


class Profile(c.Structure):
    _fields_ = [
        (name, c.c_uint32)
        for name in (
            "model",
            "flash_bytes",
            "immutable_bytes",
            "geometry_operands",
            "geometry_bases",
        )
    ]


def identify(lib, data, model, fail=None, error=-73):
    read_fn = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8), c.c_uint32)
    lib.hpclang_rpl_rom_profile_identify.argtypes = [
        read_fn,
        c.c_void_p,
        c.c_uint32,
        c.POINTER(c.POINTER(Profile)),
    ]
    seen = []

    @read_fn
    def read(_, offset, output, length):
        assert length and length <= 64 and offset + length <= 0x140000
        seen.append((offset, length))
        if offset == fail:
            return error
        c.memmove(output, bytes(data[offset : offset + length]), length)
        return 0

    sentinel = Profile(99, 99, 99, 99, 99)
    output = c.pointer(sentinel)
    result = lib.hpclang_rpl_rom_profile_identify(read, None, model, c.byref(output))
    if result:
        assert c.addressof(output.contents) == c.addressof(sentinel)
        return result, None, len(seen)
    profile = {name: getattr(output.contents, name) for name, _ in Profile._fields_}
    assert sum(size for _, size in seen) == 0x140000
    return result, profile, len(seen)


def verify_rom_profile(lib, flash, model):
    number = 5 if model == "hp49gplus" else 6
    result, profile, calls = identify(lib, flash, number)
    assert result == 0 and profile == dict(
        model=number,
        flash_bytes=0x200000,
        immutable_bytes=0x140000,
        geometry_operands=0x248A0,
        geometry_bases=0xBE0,
    )
    offsets = (0, 0x248A0, 0x248DC, 0x28828, 0x41440, 0x47FF1, 0x47FF2, 0x60000, 0x100000, 0x13FFFF)
    for offset in offsets:
        changed = bytearray(flash)
        changed[offset] ^= 1
        assert identify(lib, changed, number)[0] < 0
    changed = bytearray(flash)
    changed[0x140000] ^= 1
    changed[-1] ^= 1
    assert identify(lib, changed, number)[1] == profile
    assert identify(lib, flash, 11 - number)[0] < 0
    assert identify(lib, flash, 0)[2] == 0
    for offset in (0, 0x13FFC0):
        assert identify(lib, flash, number, offset)[0] == -73
        assert identify(lib, flash, number, offset, 73)[0] < 0
    return dict(
        profile=profile,
        read_blocks=calls,
        immutable_sha256=hashlib.sha256(flash[:0x140000]).hexdigest(),
        changed_firmware_rejections=len(offsets),
        user_flash_change_accepted=True,
        wrong_model_rejected=True,
        read_failure_cases=4,
    )
