"""Independently verify the HP49g+/HP50g recovery witness from saved native state."""

import argparse
import configparser
import json
import re
from pathlib import Path
import struct
import subprocess
import zlib


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_tls(image, symbols, physical, diagnostics):
    """Check the live TLS cells as well as the witness's recorded observations."""
    require(
        diagnostics[20:22] == (0xFFFFFFFF, 22) and diagnostics[23:25] == (0x12345678, 0x12345679),
        "TLS initialization failed",
    )
    first, end = symbols["__tls_base"], symbols["__tls_end"]
    require(0 <= first < end <= len(image), "TLS extent is outside the execution image")
    for name, expected in (("errno", 22), ("tls_probe", 0x12345679)):
        # ELF TLS symbol values are offsets within the TLS block.
        offset = first + symbols[name]
        require(first <= offset <= end - 4 and offset % 4 == 0, "Invalid TLS cell extent")
        if name == "errno":
            require(diagnostics[22] == physical + offset, "errno is not its owned TLS cell")
        require(struct.unpack_from("<I", image, offset)[0] == expected, f"TLS {name} corrupted")


def verify_authority(flash, authority_slot, slot, target, status):
    require(authority_slot == slot + 4124, "Binding is not at the fixture allocation boundary")
    binding = flash[authority_slot : authority_slot + 32]
    require(len(binding) == 32 and binding[0] == status, "Unexpected transaction binding phase")
    require(binding[1:12] == bytes.fromhex("3e0040") + b"HPCAUTH1", "Invalid binding envelope")
    require(
        struct.unpack_from("<4I", binding, 12)
        == (
            authority_slot,
            slot,
            target,
            int.from_bytes(flash[slot + 4120 : slot + 4124], "little"),
        ),
        "Transaction binding identity mismatch",
    )
    require(
        zlib.crc32(binding[1:28]) == int.from_bytes(binding[28:], "little"), "Binding CRC mismatch"
    )


def verify_flash(before, after, slot, target, *, retire=False, authority_slot=None):
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
    if authority_slot is not None:
        verify_authority(before, authority_slot, slot, target, 0x7E)
        if retire:
            expected[authority_slot] = 0
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


def verify_retired(before, after, slot, target, *, authority_slot=None):
    """Verify a known retired transaction remains unchanged on a later launch."""
    require(len(before) == len(after) == 0x200000, "Expected complete 2 MiB Flash images")
    require(0 <= slot < len(before) and before[slot] == 0, "Expected a retired journal")
    require(before == after, "Clean recovery invocation changed Flash")
    if authority_slot is not None:
        verify_authority(before, authority_slot, slot, target, 0)
    # Normalize a private oracle copy; the durable record must remain retired.
    ready = bytearray(before)
    ready[slot] = 0x7E
    record, restored = verify_flash(ready, ready, slot, target)
    require(restored, "Retired transaction target no longer matches")
    return record


def installed_recovery(flash, package, library_id, target, *, read):
    """Verify the exact recovery library at a live native allocation boundary.

    An installation witness, not transaction-root authorization. In particular,
    a matching payload inside a deleted allocation is not an installed library.
    The recovery allocation must survive erasing the proposed target sector.
    """
    require(len(flash) == 0x200000, "Expected complete hp4950 Flash")
    require(package[:8] == b"HPHP49-C", "Invalid recovery library envelope")
    expected = nibbles(package[8:])

    def word(data, at, width=5):
        require(0 <= at <= len(data) - width, "Library field outside allocation")
        return int(data[at : at + width][::-1], 16)

    require(word(expected, 0) == 0x2B40, "Expected a recovery library")
    length = 5 + word(expected, 5)
    require(length >= 34 and expected[length:] in ("", "0"), "Invalid library extent")
    require(
        word(expected, 14 + 2 * word(expected, 10, 2), 3) == library_id,
        "Recovery library ID differs from package",
    )
    crc = 0
    for value in expected[5 : length - 4]:
        crc = (crc >> 4) ^ (((crc ^ int(value, 16)) & 15) * 0x1081)
    require(word(expected, length - 4, 4) == crc, "Invalid packaged library CRC")
    require(0x140000 <= target < 0x200000 and target % 4096 == 0, "Invalid recovery target sector")
    matches = []
    for bank in range(0x140000, 0x200000, 0x20000):
        identity = int.from_bytes(flash[bank + 0x100 : bank + 0x105], "little")
        require(identity == 0xF0 + ((bank // 0x20000 - 2) << 8), "Invalid installed bank identity")
        cursor, end = bank + 0x105, bank + 0x20000
        while cursor < end and flash[cursor] != 0xFF:
            status = flash[cursor]
            require(status in (0, 0x53) and cursor + 4 <= end, "Unresolved installed allocation")
            extent = (int.from_bytes(flash[cursor + 1 : cursor + 4], "little") & 0xFFFFF) + 2
            require(
                extent >= 8 and extent % 8 == 0 and extent // 2 < end - cursor,
                "Invalid installed allocation extent",
            )
            size = extent // 2
            if status == 0x53:
                body = nibbles(flash[cursor + 4 : cursor + size])
                if word(body, 0) == 0x2B40:
                    stop = 5 + word(body, 5)
                    require(34 <= stop <= len(body), "Invalid installed library extent")
                    header = 14 + 2 * word(body, 10, 2)
                    require(header + 23 <= stop - 4, "Invalid installed library header")
                    if word(body, header, 3) == library_id:
                        require(
                            body[:stop] == expected[:length],
                            "Installed recovery differs from package",
                        )
                        require(
                            target + 4096 <= cursor or target >= cursor + size,
                            "Recovery target overlaps installed recovery",
                        )
                        matches.append((cursor, size))
            cursor += size
        require(
            cursor < end and flash[cursor:end] == b"\xff" * (end - cursor),
            "Invalid installed bank tail",
        )
    require(len(matches) == 1, "Expected exactly one live recovery installation")
    slot, _ = matches[0]
    count = read(0x8611D, 3)
    require(0 < count < 128, "Invalid installed library catalog")
    entries = [0x86120 + 16 * i for i in range(count) if read(0x86120 + 16 * i, 3) == library_id]
    require(len(entries) == 1, "Expected one recovery catalog binding")
    header = 0x40000 + 2 * ((slot + 4) & 0x1FFFF) + 14 + 2 * word(expected, 10, 2)
    require(read(entries[0] + 3) == header, "Recovery catalog points to another object")
    # Stock 2.15 fixture table: entries map physical Flash banks 2..17.
    switch = read(entries[0] + 8)
    banks = [index + 2 for index in range(16) if read(0x26BBB + 7 * index) == switch]
    require(banks == [slot // 0x20000], "Recovery catalog selects another Flash bank")
    return matches[0]


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


def home_object(read, name):
    """Locate a named fixture independently of the recovery diagnostics."""
    home = read(0x80711)
    require(0x80000 <= home < 0x100000 and read(home) == 0x2A96, "Invalid HOME directory")
    link = home + 8 + 13 * read(home + 5, 3)
    offset = read(link)
    item = link + offset
    seen = set()
    while offset:
        require(
            0x80005 <= item < 0x100000 and item not in seen,
            "Invalid HOME variable chain",
        )
        seen.add(item)
        length = read(item, 2)
        variable = bytes(read(item + 2 + 2 * i, 2) for i in range(length))
        if variable == name:
            return item + (4 + 2 * length if length else 2)
        offset = read(item - 5)
        item = item - 5 - offset
    raise ValueError("HOME fixture variable is missing")


def home_string(read, name):
    """Read an owned fixture string independently of the recovery diagnostics."""
    obj = home_object(read, name)
    require(read(obj) == 0x2A2C, "HOME fixture value is not a string")
    extent = read(obj + 5)
    require(
        extent >= 5 and (extent - 5) % 2 == 0 and obj + 5 + extent <= 0x100000,
        "Invalid HOME fixture string extent",
    )
    return bytes(read(obj + 10 + i, 2) for i in range(0, extent - 5, 2))


def verify_zero_real_array(read, name, count):
    """Check actual DOREAL storage, excluding compact exact-integer containers."""
    obj = home_object(read, name)
    require(0 < count <= (0x100000 - obj - 25) // 16, "Invalid real array extent")
    require(
        [read(obj + 5 * i) for i in range(5)] == [0x29E8, 20 + 16 * count, 0x2933, 1, count],
        "Unexpected real array shape/type",
    )
    require(
        all(read(obj + 25 + i, 1) == 0 for i in range(16 * count)),
        "Real array contents changed",
    )


def require_stock_pc(state_directory):
    """Reject unfinished ARM entry and CPU exceptions, even inside the ROM range.

    This is only a processor-state check; callers must also validate the native
    memory, catalog, stack and retained objects appropriate to their scenario.
    """
    state = configparser.ConfigParser()
    state.read(state_directory / "state")
    pc = int(state["s3c2410-arm"]["reg-15"], 0)
    require(0 <= pc < 0x140000, "Execution has not returned to Stock ROM")
    mode = int(state["s3c2410-arm"]["cpsr"], 0) & 0x1F
    require(mode not in (0x17, 0x1B), "Stock return stopped in ARM abort/undefined mode")


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


def nibbles(data):
    return "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in data)


def executed_image(ram, initialized, symbols, *, relocated=False):
    """Locate a saved image, optionally moved by Saturn GC after ARM return.

    GC can move whole CODE objects by an odd nibble. Runtime addresses in the
    diagnostics remain the original addresses; never infer execution alignment
    from the final object's position. The caller still verifies the complete
    image, owning CODE envelope, stack, TLS and hardware-state diagnostics.
    """
    packed = nibbles(ram)
    marker = nibbles(struct.pack("<I", 0x48505046))
    prefix = nibbles(initialized[:32])
    candidates = []
    hit = packed.find(marker)
    while hit >= 0:
        start = hit - 2 * symbols["workspace"]
        if start >= 0 and packed.startswith(prefix, start):
            candidates.append(start)
        hit = packed.find(marker, hit + 1)
    require(len(candidates) == 1, "Executed recovery image is missing or ambiguous")
    start = candidates[0]
    size = symbols["__image_end"]
    require(start + 2 * size <= len(packed), "Recovery image bounds/alignment")
    image = bytes(int(packed[i : i + 2][::-1], 16) for i in range(start, start + 2 * size, 2))
    if relocated:
        errno_address = struct.unpack_from("<I", image, symbols["workspace"] + 8272 + 22 * 4)[0]
        physical = errno_address - symbols["__tls_base"] - symbols["errno"]
    else:
        require(start % 16 == 0, "Recovery image bounds/alignment")
        physical = 0x08000000 + start // 2
    require(
        physical % 8 == 0 and 0x08000000 <= physical <= 0x08000000 + len(ram) - size,
        "Original recovery image bounds/alignment",
    )
    return image, start, physical


def reset_snapshot(prefix):
    metadata = json.loads(prefix.with_suffix(".json").read_text())
    require(
        isinstance(metadata, dict) and set(metadata) == {"version", "pc", "cpsr", "r0", "bytes"},
        "Invalid reset checkpoint schema",
    )
    require(
        all(type(value) is int and 0 <= value <= 0xFFFFFFFF for value in metadata.values()),
        "Invalid reset checkpoint values",
    )
    require(
        metadata["version"] == 1 and metadata["bytes"] == 0x80000 and metadata["r0"] == 1,
        "Invalid reset checkpoint request",
    )
    require(
        0x08000004 <= metadata["pc"] <= 0x08080000
        and metadata["pc"] % 4 == 0
        and not metadata["cpsr"] & 0x20
        and metadata["cpsr"] & 0x1F not in (0x17, 0x1B),
        "Invalid reset checkpoint CPU state",
    )
    ram = prefix.with_suffix(".sram").read_bytes()
    require(len(ram) == 0x80000, "Incomplete reset checkpoint SRAM")
    return ram, metadata


def verify(args):
    before = (args.before / "flash").read_bytes()
    after = (args.after / "flash").read_bytes()
    operation = getattr(args, "operation", "replay")
    require(operation in ("replay", "retire", "check"), "Unknown recovery operation")
    retire = operation == "retire"
    authority_slot = getattr(args, "authority_slot", None)
    if operation == "check":
        record = verify_retired(
            before, after, args.slot, args.target, authority_slot=authority_slot
        )
        already_restored, phase = True, 0
    else:
        record, already_restored = verify_flash(
            before, after, args.slot, args.target, retire=retire, authority_slot=authority_slot
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
    checkpoint = getattr(args, "reset_checkpoint", None)
    execution_ram, reset = reset_snapshot(checkpoint) if checkpoint is not None else (ram, None)
    size = symbols["__image_end"]
    expected = bytearray(initialized + bytes(size - len(initialized)))
    struct.pack_into("<2I", expected, symbols["guard_low"], 0x13579BDF, 0x2468ACE0)
    struct.pack_into("<2I", expected, symbols["guard_high"], 0x89ABCDEF, 0xFEDCBA98)
    image, start, physical = executed_image(
        execution_ram,
        initialized,
        symbols,
        relocated=False if reset is not None else getattr(args, "relocated_image", False),
    )
    result = struct.unpack_from("<Ii4I", image, symbols["workspace"])
    discovery = (phase, args.slot, args.target) if phase else (0, 0, 0)
    require(result[1:4] == discovery and result[4] > 0, "Preflight/discovery failed")
    require(
        physical + symbols["stack_start"] <= result[5] < physical + symbols["stack_end"]
        and result[5] % 8 == 0,
        "C stack escaped its owned reservation",
    )
    scratch = symbols["workspace"] + 24
    if phase:
        require(
            image[scratch : scratch + 4124] == record, "RAM journal differs from durable journal"
        )
    if phase == 1:
        require(
            image[scratch + 4124 : scratch + 8248] == record,
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
            require(image[i] == value, f"Code/guard corruption at image offset {i:#x}")

    package = args.package.read_bytes()
    require(package[:8] == b"HPHP49-C", "Invalid binary package header")
    program = nibbles(package[8:])
    code_end = 10 + int(program[10:15][::-1], 16)
    reserve = size * 2 + 15
    prefix = program[: code_end - reserve]
    packed_ram = nibbles(execution_ram)
    owners = [
        i
        for i in range(len(packed_ram))
        if packed_ram.startswith(prefix, i)
        and i + len(prefix) <= start
        and start + size * 2 <= i + code_end
    ]
    require(len(owners) == 1, "Execution image is not inside one owning CODE object")
    original_owner = 2 * (physical - 0x08000000) - (start - owners[0])
    require(
        0 <= original_owner and original_owner + code_end <= 2 * len(execution_ram),
        "Original owning CODE object is outside RAM",
    )

    d = struct.unpack_from("<32I", image, symbols["workspace"] + 8272)
    if reset is not None:
        require(
            "recovery_reset" in symbols and reset["pc"] == physical + symbols["recovery_reset"] + 4,
            "Reset checkpoint came from another instruction",
        )
        require(
            reset["cpsr"] & 0xFF == d[7] & 0xFF,
            "Reset checkpoint did not preserve caller CPU state",
        )
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
    verify_tls(image, symbols, physical, d)
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
    parser.add_argument(
        "--relocated-image",
        action="store_true",
        help="verify a CODE image moved by Stock GC after return",
    )
    for name in ("before", "after", "elf", "binary", "package"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument("--slot", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--target", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--authority-slot", type=lambda value: int(value, 0))
    parser.add_argument(
        "--reset-checkpoint",
        type=Path,
        help="prefix of execution SRAM/CPU evidence captured before Stock reset",
    )
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
