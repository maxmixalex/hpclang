"""Independent oracle for the Stock 2.15 startup inventory witness."""

import struct

from verify import native_words, require, symbol_offsets


def verify_startup(before, after, build, nm, base):
    flash = (before / "flash").read_bytes()
    read = native_words((before / "sram").read_bytes(), flash)
    count_at, directory = struct.unpack_from("<II", flash, 0x248A0)
    require(count_at < len(flash) and directory == count_at + 1, "Invalid ROM size directory")
    banks = flash[count_at]
    require(0 < banks <= 16 and directory + banks <= len(flash), "Invalid ROM bank count")
    physical = directory + banks
    extents = []
    for size in flash[directory : directory + banks]:
        require(0 < size <= 128, "ROM bank exceeds its Saturn window")
        extents.append((physical, size * 1024))
        physical += size * 1024
    require(physical == len(flash), "ROM bank directory does not cover Flash")
    switches = [read(0x26BBB + 7 * i) for i in range(16)]
    require(len(set(switches)) == 16, "Ambiguous ROM bank switches")
    expected = []
    for i in range(read(0x8611D, 3)):
        entry = 0x86120 + 16 * i
        number, header, switch = read(entry, 3), read(entry + 3), read(entry + 8)
        bank = switches.index(switch) if switch else 0xFFFFFFFF
        if switch:
            require(bank < len(extents), "Library selects an unpopulated bank")
            address, size = extents[bank]

            def word(at, n=5):
                require(0x40000 <= at and at + n <= 0x40000 + size * 2, "Bank read overflow")
                return sum(
                    ((flash[address + (at + j - 0x40000) // 2] >> (4 * ((at + j) & 1))) & 15)
                    << (4 * j)
                    for j in range(n)
                )
        else:
            word = read
        require(word(header, 3) == number, "Library header identity differs")
        relative = word(header + 18)
        target = (header + 18 + relative) & 0xFFFFF if relative else 0
        if target and switch:
            require(0x40000 <= target < 0x40000 + size * 2, "Callback leaves its bank")
        expected.append((number, header, bank, target))
    ram = (after / "sram").read_bytes()
    symbols = symbol_offsets(build / "installation.elf", nm)
    require(
        struct.unpack_from("<3I", ram, base + symbols["startup_result"])
        == (0, len(expected), banks),
        "Native startup inventory was incomplete",
    )
    actual = [
        struct.unpack_from("<4I", ram, base + symbols["startup_entries"] + 16 * i)
        for i in range(len(expected))
    ]
    require(
        actual == expected, "Native startup inventory differs from independent ROM/catalog view"
    )
    return {"libraries": len(expected), "callbacks": sum(bool(row[3]) for row in expected)}
