"""Independently verify the HP49g+/HP50g recovery witness from saved native state."""

import argparse
import configparser
import re
from pathlib import Path
import struct
import subprocess
import zlib


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_flash(before, after, slot, target, *, retire=False):
    require(len(before) == len(after) == 0x200000, "Expected complete 2 MiB Flash images")
    require(0x140000 <= target <= 0x1FF000 and target % 4096 == 0, "Invalid target sector")
    require(0x140000 <= slot <= 0x200000 - 4124, "Invalid journal address")
    require(slot + 4124 <= target or target + 4096 <= slot, "Journal overlaps target")
    record = bytearray(before[slot : slot + 4124])
    status = record[0]
    require(
        (status == 0x7E or (retire and status != 0 and status & 0x7E == status))
        and record[4:12] == b"HPCSECT1",
        "Expected ready or authorized retiring sector journal",
    )
    record[0] = 0x7E
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
    already_restored = before[target : target + 4096] == record[24:4120]
    require(status == 0x7E or already_restored, "Retiring journal has an unrestored target")
    expected[target : target + 4096] = record[24:4120]
    if retire:
        expected[slot] = 0
    require(after == expected, "Flash differs from authorized replay/journal retirement")
    return bytes(record), already_restored


def verify_power_cut(before_path, after_path, log_path, slot, target, cut):
    before, after = before_path.read_bytes(), after_path.read_bytes()
    record = before[slot : slot + 4124]
    repaired = bytearray(before)
    repaired[target : target + 4096] = record[24:4120]
    verify_flash(before, repaired, slot, target)  # Validate journal independently first.
    operation, reads, mask = cut
    image = record[24:4120]
    identity_offset = 0x100 if target % 0x20000 == 0 else 4096
    writes = [
        (target + i, int.from_bytes(image[i : i + 2], "little"))
        for i in range(0, 4096, 2)
        if i != identity_offset and image[i : i + 2] != b"\xff\xff"
    ]
    if identity_offset < 4096:
        identity = int.from_bytes(image[identity_offset : identity_offset + 2], "little")
        if identity != 0xFFFF:
            writes.append((target + identity_offset, identity))
    require(1 <= operation <= len(writes) + 1, "Cut operation outside replay")
    expected = bytearray(before)
    if operation == 1:
        address, size, value = target, 4096, 0xFFFF
        for i in range(4096):
            expected[target + i] |= (mask >> (8 * (i % 2))) & 255
    else:
        expected[target : target + 4096] = b"\xff" * 4096
        for address, value in writes[: operation - 2]:
            expected[address : address + 2] = value.to_bytes(2, "little")
        address, value = writes[operation - 2]
        size = 2
        partial = value | (mask ^ 0xFFFF)
        expected[address : address + 2] = partial.to_bytes(2, "little")
    events = re.findall(
        r"^hpclang-flash-cut operation=(\d+) address=(\d+) bytes=(\d+) "
        r"value=(\d+) mask=(\d+) reads=(\d+)$",
        log_path.read_text(),
        re.M,
    )
    require(len(events) == 1, "Expected exactly one native busy-operation cut")
    require(
        tuple(map(int, events[0])) == (operation, address, size, value, mask, reads),
        "Native cut does not match requested replay operation",
    )
    require(after == expected, "Power-cut Flash differs from selected torn operation")
    return after[target : target + 4096] == image


def verify_retirement_cut(before_path, after_path, log_path, slot, target, cut):
    """Check one torn status-word program after an already verified replay."""
    before, after = before_path.read_bytes(), after_path.read_bytes()
    retired = bytearray(before)
    retired[slot] = 0
    _, restored = verify_flash(before, retired, slot, target, retire=True)
    require(restored, "Retirement cut requires an already restored target")
    operation, reads, mask = cut
    require(operation == 1 and reads > 0 and 0 <= mask <= 0xFFFF, "Invalid retirement cut")
    address = slot & ~1
    old = int.from_bytes(before[address : address + 2], "little")
    value = old & ~(255 << (8 * (slot & 1)))
    expected = bytearray(before)
    partial = old & (value | (mask ^ 0xFFFF))
    expected[address : address + 2] = partial.to_bytes(2, "little")
    events = re.findall(
        r"^hpclang-flash-cut operation=(\d+) address=(\d+) bytes=(\d+) "
        r"value=(\d+) mask=(\d+) reads=(\d+)$",
        log_path.read_text(),
        re.M,
    )
    require(
        len(events) == 1
        and tuple(map(int, events[0])) == (operation, address, 2, value, mask, reads),
        "Native retirement cut does not match requested status operation",
    )
    require(after == expected, "Retirement cut changed unrelated Flash bytes")


def verify_retired(before, after, slot, target):
    """Verify a known retired transaction remains unchanged on a later launch."""
    require(len(before) == len(after) == 0x200000, "Expected complete 2 MiB Flash images")
    require(0 <= slot < len(before) and before[slot] == 0, "Expected a retired journal")
    require(before == after, "Clean recovery invocation changed Flash")
    # Normalize a private oracle copy; the durable record must remain retired.
    ready = bytearray(before)
    ready[slot] = 0x7E
    record, restored = verify_flash(ready, ready, slot, target)
    require(restored, "Retired transaction target no longer matches")
    return record


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


def require_stock_pc(state_directory):
    state = configparser.ConfigParser()
    state.read(state_directory / "state")
    pc = int(state["s3c2410-arm"]["reg-15"], 0)
    require(0 <= pc < 0x140000, "Execution has not returned to Stock ROM")


def symbol_offsets(elf, nm):
    symbols = {}
    for line in subprocess.check_output([nm, elf], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    return symbols


def verify_interruption(args):
    """Require a complete sector body, unpublished identity and owned failure loop."""
    before = (args.before / "flash").read_bytes()
    after = (args.after / "flash").read_bytes()
    require(args.target % 0x20000 == 0, "Interruption scenario requires a bank header sector")
    record = before[args.slot : args.slot + 4124]
    restored = bytearray(before)
    restored[args.target : args.target + 4096] = record[24:4120]
    verify_flash(before, restored, args.slot, args.target)
    require(record[24 + 0x100 : 24 + 0x102] != b"\xff\xff", "Journal contains no native identity")
    restored[args.target + 0x100 : args.target + 0x102] = b"\xff\xff"
    require(
        after == restored, "Interruption did not preserve exactly the unpublished bank identity"
    )
    ram = (args.after / "sram").read_bytes()
    symbols = symbol_offsets(args.elf, args.nm)
    binary = args.binary.read_bytes()
    candidates = [
        i
        for i in range(len(ram) - symbols["__image_end"] + 1)
        if ram[i : i + 32] == binary[:32]
        and ram[i + symbols["workspace"] : i + symbols["workspace"] + 4]
        == struct.pack("<I", 0x48504641)
    ]
    require(len(candidates) == 1, "Interrupted image is missing or ambiguous")
    start = candidates[0]
    require(start % 8 == 0, "Interrupted image is misaligned")
    require(ram[start : start + len(binary)] == binary, "Interrupted initialized image changed")
    physical = 0x08000000 + start
    result = struct.unpack_from("<Ii4I", ram, start + symbols["workspace"])
    require(result[1:4] == (1, args.slot, args.target), "Interrupted preflight/discovery failed")
    scratch = start + symbols["workspace"] + 24
    require(ram[scratch : scratch + 4124] == record, "Interrupted RAM journal changed")
    require(
        ram[scratch + 4124 : scratch + 8248] == record,
        "Interrupted replay did not retain a separate journal reload",
    )
    for symbol, values in (
        ("guard_low", (0x13579BDF, 0x2468ACE0)),
        ("guard_high", (0x89ABCDEF, 0xFEDCBA98)),
    ):
        require(
            struct.unpack_from("<2I", ram, start + symbols[symbol]) == values,
            "Interrupted stack guard changed",
        )
    d = struct.unpack_from("<32I", ram, start + symbols["workspace"] + 8272)
    programs = sum(record[24 + i : 26 + i] != b"\xff\xff" for i in range(0, 4096, 2) if i != 0x100)
    require(d[26:32] == (0, 0xFFFFFFFC, 1, programs, 0, 0), "Unexpected interrupted replay result")
    state = configparser.ConfigParser()
    state.read(args.after / "state")
    cpu = state["s3c2410-arm"]
    require(
        physical <= int(cpu["reg-15"], 0) < physical + len(binary),
        "Interrupted recovery returned instead of retaining ownership",
    )
    require(
        physical + symbols["stack_start"]
        <= int(cpu["reg-13"], 0)
        < physical + symbols["stack_end"],
        "Interrupted stack escaped owned storage",
    )
    require(int(cpu["cpsr"], 0) & 0xC0 == 0xC0, "Interrupted recovery released IRQ ownership")


def verify_restart(clean, repaired, restarted):
    """Check native catalog reconstruction against the original clean boot."""
    require(
        (repaired / "flash").read_bytes() == (restarted / "flash").read_bytes(),
        "Post-recovery reset changed Flash or its retained journal",
    )
    catalogs = []
    for directory in (clean, restarted):
        read = native_words((directory / "sram").read_bytes(), (directory / "flash").read_bytes())
        require(read(0x806FD) - read(0x806F8) == 5, "Restart did not preserve the empty stack")
        count = read(0x8611D, 3)
        require(0 < count < 128, "Invalid native library catalog count")
        catalogs.append(
            sorted(
                tuple(
                    read(0x86120 + 16 * i + offset, width)
                    for offset, width in ((0, 3), (3, 5), (8, 5), (13, 3))
                )
                for i in range(count)
            )
        )
    require(
        catalogs[0] == catalogs[1], "Restart did not restore the original native library catalog"
    )
    require_stock_pc(restarted)
    return len(catalogs[0])


def verify(args):
    before = (args.before / "flash").read_bytes()
    after = (args.after / "flash").read_bytes()
    operation = getattr(args, "operation", "replay")
    require(operation in ("replay", "retire", "check"), "Unknown recovery operation")
    retire = operation == "retire"
    if operation == "check":
        record = verify_retired(before, after, args.slot, args.target)
        already_restored, phase = True, 0
    else:
        record, already_restored = verify_flash(
            before, after, args.slot, args.target, retire=retire
        )
        phase = 1 if before[args.slot] == 0x7E else 2
    ram = (args.after / "sram").read_bytes()
    read = native_words(ram, after)
    require(read(0x806FD) - read(0x806F8) == 5, "Recovery did not preserve the empty stack")
    count = read(0x8611D, 3)
    require(count < 128, "Invalid library catalog")
    require(all(read(0x86120 + 16 * i, 3) != 275 for i in range(count)), "Library 275 is present")
    original_read = native_words((args.before / "sram").read_bytes(), before)
    require(original_read(0x806FD) - original_read(0x806F8) == 5, "Baseline stack must be empty")

    symbols = symbol_offsets(args.elf, args.nm)
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
    discovery = (phase, args.slot, args.target) if phase else (0, 0, 0)
    require(result[1:4] == discovery and result[4] > 0, "Preflight/discovery failed")
    require(
        physical + symbols["stack_start"] <= result[5] < physical + symbols["stack_end"]
        and result[5] % 8 == 0,
        "C stack escaped its owned reservation",
    )
    scratch = start + symbols["workspace"] + 24
    if phase:
        require(ram[scratch : scratch + 4124] == record, "RAM journal differs from durable journal")
    if phase == 1:
        require(
            ram[scratch + 4124 : scratch + 8248] == record,
            "Replay did not retain a separate journal reload",
        )
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
        d[26:28] == (0, 0) and d[30:32] == (0, int(retire)),
        "Replay/retirement failed or repeated operation wrote Flash",
    )
    require(
        d[28] == (0 if already_restored else 1)
        and (d[29] == int(retire) if already_restored else d[29] > int(retire)),
        "Unexpected erase/program count",
    )
    require_stock_pc(args.after)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("before", "after", "elf", "binary", "package"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument("--slot", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--target", type=lambda value: int(value, 0), required=True)
    parser.add_argument(
        "--operation",
        choices=("replay", "retire", "check"),
        default="replay",
        help="expected operation; check requires a previously retired transaction",
    )
    verify(parser.parse_args())
    print("PASS: independent recovery, exact Flash changes, owned memory and Stock return")


if __name__ == "__main__":
    main()
