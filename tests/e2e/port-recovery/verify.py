"""Independently verify the HP49g+/HP50g recovery witness from saved native state."""

import argparse
import configparser
from pathlib import Path
import struct
import subprocess
import zlib


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_flash(before, after, slot, target):
    require(len(before) == len(after) == 0x200000, "Expected complete 2 MiB Flash images")
    require(0x140000 <= target <= 0x1FF000 and target % 4096 == 0, "Invalid target sector")
    require(0x140000 <= slot <= 0x200000 - 4124, "Invalid journal address")
    require(slot + 4124 <= target or target + 4096 <= slot, "Journal overlaps target")
    record = before[slot : slot + 4124]
    require(record[0] == 0x7E and record[4:12] == b"HPCSECT1", "Expected ready sector journal")
    require(
        int.from_bytes(record[1:4], "little") == (2 * 4124 - 2) | 0x300000,
        "Invalid journal extent/metadata",
    )
    require(
        struct.unpack_from("<3I", record, 12) == (slot, target, 4096), "Journal geometry mismatch"
    )
    require(
        zlib.crc32(record[1:4120]) == int.from_bytes(record[4120:], "little"),
        "Journal CRC mismatch",
    )
    expected = bytearray(before)
    expected[target : target + 4096] = record[24:4120]
    require(after == expected, "Flash differs from exactly the authorized sector replay")
    return record, before[target : target + 4096] == record[24:4120]


def native_words(ram, flash):
    require(len(ram) == 0x80000, "Expected hp4950 SRAM")
    words = struct.unpack("<" + "I" * (len(ram) // 4), ram)
    for table, value in enumerate(words[:-128]):
        if not 0x08000000 <= value < 0x08080000 or value % 2048:
            continue
        if any(words[table + j] != value + j * 2048 for j in (1, 2, 3, 16, 64, 127)):
            continue

        def read(address, count=5):
            result = 0
            for i in range(count):
                at = address + i
                index = table + (at >> 12) - 128
                require(0 <= index < len(words), "Native map index outside SRAM")
                physical = words[index] + (at & 4095) // 2
                data, offset = (
                    (ram, physical - 0x08000000) if physical >= 0x08000000 else (flash, physical)
                )
                require(0 <= offset < len(data), "Native mapping outside saved memory")
                result |= ((data[offset] >> (4 * (at & 1))) & 15) << (4 * i)
            return result

        try:
            home = read(0x80711)
            if home >= 0x80000 and read(home) == 0x2A96:
                return read
        except ValueError:
            continue
    raise ValueError("Native Saturn map not found")


def verify(args):
    before = (args.before / "flash").read_bytes()
    after = (args.after / "flash").read_bytes()
    record, already_restored = verify_flash(before, after, args.slot, args.target)
    ram = (args.after / "sram").read_bytes()
    read = native_words(ram, after)
    require(read(0x806FD) - read(0x806F8) == 5, "Recovery did not preserve the empty stack")
    count = read(0x8611D, 3)
    require(count < 128, "Invalid library catalog")
    require(all(read(0x86120 + 16 * i, 3) != 275 for i in range(count)), "Library 275 is present")
    original_read = native_words((args.before / "sram").read_bytes(), before)
    require(original_read(0x806FD) - original_read(0x806F8) == 5, "Baseline stack must be empty")

    symbols = {}
    for line in subprocess.check_output([args.nm, args.elf], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    initialized = args.binary.read_bytes()
    size = symbols["__image_end"]
    expected = bytearray(initialized + bytes(size - len(initialized)))
    struct.pack_into("<2I", expected, symbols["guard_low"], 0x13579BDF, 0x2468ACE0)
    struct.pack_into("<2I", expected, symbols["guard_high"], 0x89ABCDEF, 0xFEDCBA98)
    candidates = []
    for hit in range(len(ram) - 3):
        if ram[hit : hit + 4] != struct.pack("<I", 0x48505046):
            continue
        start = hit - symbols["workspace"]
        if start >= 0 and ram[start : start + 32] == initialized[:32]:
            candidates.append(start)
    require(len(candidates) == 1, "Executed recovery image is missing or ambiguous")
    start = candidates[0]
    require(start % 8 == 0 and start + size <= len(ram), "Recovery image bounds/alignment")
    physical = 0x08000000 + start
    result = struct.unpack_from("<Ii4I", ram, start + symbols["workspace"])
    require(
        result[1:4] == (1, args.slot, args.target) and result[4] > 0, "Preflight/discovery failed"
    )
    require(
        physical + symbols["stack_start"] <= result[5] < physical + symbols["stack_end"]
        and result[5] % 8 == 0,
        "C stack escaped its owned reservation",
    )
    scratch = start + symbols["workspace"] + 24
    require(ram[scratch : scratch + 4124] == record, "RAM journal differs from durable journal")
    mutable = [
        (symbols["workspace"], symbols["workspace"] + 8400),
        (symbols["stack_start"], symbols["stack_end"]),
        (symbols["__tls_base"], symbols["__tls_end"]),
        (symbols["state"], symbols["state"] + 56),
    ]
    for i, value in enumerate(expected):
        if not any(first <= i < last for first, last in mutable):
            require(ram[start + i] == value, f"Code/guard corruption at image offset {i:#x}")

    def nibbles(data):
        return "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in data)

    package = args.package.read_bytes()
    require(package[:8] == b"HPHP49-C", "Invalid binary package header")
    program = nibbles(package[8:])
    code_end = 10 + int(program[10:15][::-1], 16)
    reserve = size * 2 + 15
    prefix = program[: code_end - reserve]
    packed_ram = nibbles(ram)
    owners = [
        i
        for i in range(len(packed_ram))
        if packed_ram.startswith(prefix, i)
        and i + len(prefix) <= start * 2
        and start * 2 + size * 2 <= i + code_end
    ]
    require(len(owners) == 1, "Execution image is not inside one owning CODE object")

    d = struct.unpack_from("<32I", ram, start + symbols["workspace"] + 8272)
    require(d[1] & 0xC0 == 0xC0 and d[0] & 0xFF == d[7] & 0xFF, "IRQ state not restored")
    require(d[2] == d[10] and d[3:5] == d[8:10], "Mapping/vector not restored")
    require(
        d[5] == (d[3] & ~0xC) | 0xC00 and d[6] == (d[4] & ~0xC) | 0xC00,
        "Flash was not mapped writable and uncached",
    )
    require(
        d[16] == 0 and 1000 <= (d[18] - d[17]) & 0xFFFFFFFF < 100000 and d[19] < 100000,
        "Recovery monotonic clock failed",
    )
    require(
        d[20:22] == (0xFFFFFFFF, 22) and d[23:25] == (0x12345678, 0x12345679),
        "TLS initialization failed",
    )
    require(
        physical + symbols["__tls_base"] <= d[22] < physical + symbols["__tls_end"],
        "errno is outside owned TLS",
    )
    require(
        d[26:28] == (0, 0) and d[30:32] == (0, 0), "Replay failed or repeated replay wrote Flash"
    )
    require(
        d[28] == (0 if already_restored else 1) and (d[29] == 0 if already_restored else d[29] > 0),
        "Unexpected erase/program count",
    )
    state = configparser.ConfigParser()
    state.read(args.after / "state")
    pc = int(state["s3c2410-arm"]["reg-15"], 0)
    require(0 <= pc < 0x140000, "Execution has not returned to Stock ROM")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("before", "after", "elf", "binary", "package"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument("--slot", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--target", type=lambda value: int(value, 0), required=True)
    verify(parser.parse_args())
    print(
        "PASS: independent recovery, exact Flash replay, retained journal, owned memory and Stock return"
    )


if __name__ == "__main__":
    main()
