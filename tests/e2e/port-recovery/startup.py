"""Independent oracle for the Stock 2.15 startup inventory witness."""

import struct

from verify import native_words, require, require_stock_pc, symbol_offsets


def verify_startup(before, after, build, nm, base):
    flash = (before / "flash").read_bytes()
    original_ram = (before / "sram").read_bytes()
    read = native_words(original_ram, flash)
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
    switches = [read(0x26BBB + 7 * i) for i in range(16)] + [read(0x26C39)]
    require(all(switches) and len(set(switches)) == 17, "Ambiguous ROM bank switches")
    expected = []
    for i in range(read(0x8611D, 3)):
        entry = 0x86120 + 16 * i
        number, header, switch = read(entry, 3), read(entry + 3), read(entry + 8)
        bank = switches.index(switch) if switch else 0xFFFFFFFF
        if switch:
            if bank == 16:
                address, size, storage = 0x20000, 0x20000, original_ram
                require(len(storage) == 0x80000, "Incomplete native RAM capture")
            else:
                require(bank < len(extents), "Library selects an unpopulated bank")
                address, size = extents[bank]
                storage = flash

            def word(at, n=5):
                require(0x40000 <= at and at + n <= 0x40000 + size * 2, "Bank read overflow")
                return sum(
                    ((storage[address + (at + j - 0x40000) // 2] >> (4 * ((at + j) & 1))) & 15)
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


def run_port1_inventory(args, clean, root, *, transfer, verify_busy_reads):
    from installation import verify_placement, verify_preparation, verify_probe

    installed = root / "port1-installed"
    transfer(
        args,
        installed,
        clean,
        args.build,
        "TESTLIB",
        execute=False,
        package_name="expected-library.hp",
        remote_commands=["-95 CF HOME CLEAR", "'TESTLIB' RCL 1 STO 'TESTLIB' PURGE"],
    )
    before = installed / "state"
    require_stock_pc(before)
    require(
        (before / "flash").read_bytes() == (clean / "flash").read_bytes(),
        "Port 1 setup changed Flash",
    )
    require(verify_busy_reads(args, installed) == 0, "Port 1 setup programmed Flash")
    phase = root / "port1-inventory"
    transfer(args, phase, before, args.build, "ERAMCHK", package_name="installation.hp")
    after = phase / "state"
    base = verify_probe(before, after, args.build, args.nm, -8, 0xFFFFFFFF)
    verify_placement(before, after, args.build, args.nm, base, existing=True)
    verify_preparation(before, after, args.build, args.nm, base, existing=True)
    inventory = verify_startup(before, after, args.build, args.nm, base)
    require(
        (before / "sram").read_bytes()[0x20000:0x40000]
        == (after / "sram").read_bytes()[0x20000:0x40000],
        "Read-only startup inventory changed Port 1 bytes",
    )
    require(verify_busy_reads(args, phase) == 0, "Port 1 inventory programmed Flash")
    print("Native startup inventory includes Port 1 without mutating RAM libraries", flush=True)
    return inventory
