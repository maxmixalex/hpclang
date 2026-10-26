"""Run controlled direct-call continuations through the standard Stock launcher."""

import json
import struct

from verify import home_string

from fixtures import word


CASES = {
    "native": b"",
    "native-remove": b"\0",
    "gosub": bytes([0x27, 0, 0x10, 0x10]),
    "gosub-rtnsxm": bytes([0x27, 0, 0x10, 0x00]),
    "gosub-rtnnc": bytes([0x37, 0, 0x05, 0, 0x03]),
    "gosubl": bytes([0xE8, 2, 0, 0x10, 0x10]),
    "gosbvl": bytes([0xF8, 0, 0, 0, 1, 1]),
}
MARKER = 0xC173A


def catalog_records(read):
    return [
        tuple(
            read(0x86120 + 16 * i + offset, width)
            for offset, width in ((0, 3), (3, 5), (8, 5), (13, 3))
        )
        for i in range(read(0x8611D, 3))
    ]


def verify_bindings(before, read, *, removed=False):
    """Check complete records and independently locate the fixture in Port 0."""
    records = catalog_records(read)
    if [record for record in records if record[0] != 123] != before:
        raise ValueError("Native call changed an existing catalog binding")
    at = read(0x80716)
    matches = []
    while 0x80000 <= at <= 0xFFFFB:
        prologue = read(at)
        if not prologue:
            break
        span = 5 + read(at + 5)
        if prologue not in (0x2B40, 0x2B62) or span < 24 or at + span > 0x100000:
            raise ValueError("Invalid native Port 0 object")
        if prologue == 0x2B40:
            length = read(at + 10, 2)
            binding = at + 14 + 2 * length
            if span < 41 + 2 * length or read(binding - 2, 2) != length:
                raise ValueError("Invalid native library title")
            if read(binding, 3) == 123:
                matches.append((123, binding, 0, 0))
        at += span
    else:
        raise ValueError("Missing native Port 0 terminator")
    expected = [] if removed else matches
    if len(matches) != 1 or [record for record in records if record[0] == 123] != expected:
        raise ValueError("Catalog binding does not identify the Port 0 fixture")


def package(launcher, name):
    if launcher[:8] != b"HPHP49-C":
        raise ValueError("Expected a native HP49 launcher")
    obj = "".join(word(byte, 2) for byte in launcher[8:])
    if int(obj[:5][::-1], 16) != 0x2A2C or len(obj) != 5 + int(obj[5:10][::-1], 16):
        raise ValueError("Expected one complete launcher string")
    payload = b"HPCSCALL" + CASES[name]
    fixture = word(0x2A2C) + word(5 + 2 * len(payload))
    fixture += "".join(word(byte, 2) for byte in payload)
    program = word(0x2D9D) + fixture + obj + word(0x2E92) + word(275, 3) + word(0, 3)
    program += word(0x2911) + word(MARKER) + word(0x312B)
    program += "0" * (len(program) % 2)
    return b"HPHP49-C" + bytes(
        int(program[i], 16) | int(program[i + 1], 16) << 4 for i in range(0, len(program), 2)
    )


def verify_entry(observation, stage0):
    """Validate the original native entry, independently of simulated returns."""
    if len(observation) != 128:
        raise ValueError("Missing native entry observation")
    words = struct.unpack("<32I", observation)
    if (
        words[0] != 1
        or words[30] != 0x454E5452
        or words[31] != 0x1596FC
        or words[8] != 3
        or words[3] >= 8
        or not 0 < words[4] <= 256
        or words[7] & 1023
        or words[6] & 3
        or words[1] & 3
        or not 0x08000000 <= words[1] <= 0x0807F000
        or not 0 < words[2] < 0x100000
        or not 0 < words[10] < 0x100000
        or not 0 < words[11] < 0x100000
        or not 0x08000000 <= words[6] <= 0x0807FFE0
    ):
        raise ValueError("Invalid native entry state")
    if observation[88:120] != stage0[:32]:
        raise ValueError("Native entry does not identify this stage0 image")
    return words


def verify_calls(args, build, source, output, *, transfer, native_words):
    original_flash = (source / "flash").read_bytes()
    before = native_words((source / "sram").read_bytes(), original_flash)
    original_ids = [before(0x86120 + 16 * i, 3) for i in range(before(0x8611D, 3))]
    original_records = catalog_records(before)
    if 123 in original_ids:
        raise ValueError("Call-root fixture requires an unregistered library 123")
    results = []
    for name, encoding in CASES.items():
        case = output / name
        package_dir = case / "package"
        package_dir.mkdir(parents=True)
        payload = package((build / "launchers" / f"{args.model}.hp").read_bytes(), name)
        (package_dir / "recovery.hp").write_bytes(payload)
        transfer(
            args,
            case / "run",
            source,
            package_dir,
            "CALLROOT",
            remote_commands=[
                "-95 CF HOME CLEAR",
                "\"0\" 1 7 START DUP + NEXT 'ENTRYOBS' STO",
                "'CALLROOT' RCL",
            ],
        )
        state = case / "run/state"
        flash = (state / "flash").read_bytes()
        if flash != original_flash:
            raise ValueError("Call-root fixture changed Flash")
        read = native_words((state / "sram").read_bytes(), flash)
        observation = home_string(read, b"ENTRYOBS")
        words = verify_entry(observation, (build / "launchers" / f"{args.model}.bin").read_bytes())
        (case / "entry.json").write_text(
            json.dumps(
                dict(
                    version=1,
                    before_fixture_rewrites=True,
                    cpu=words[1],
                    suspended_pc=words[2],
                    cursor=words[3],
                    pool_pages=words[4],
                    os_block=words[5],
                    stage0_source=words[6],
                    rom_helper=words[31],
                    pool_table=words[7],
                    hidden_launcher=words[9],
                    caller=words[10],
                    saved_ip=words[11],
                    saved_registers=list(words[12:22]),
                    stage0_prefix=observation[88:120].hex(),
                ),
                indent=2,
            )
            + "\n"
        )
        stack = read(0x806F8)
        marker = read(stack)
        if read(marker) != 0x2911 or read(marker + 5) != MARKER:
            raise ValueError("Missing post-call native marker")
        obj = read(stack + 5)
        expected = b"HPCSCALL" + encoding
        if read(obj) != 0x2A2C or read(obj + 5) != 5 + 2 * len(expected):
            raise ValueError("Call fixture object changed")
        actual = bytes(read(obj + 10 + 2 * i, 2) for i in range(len(expected)))
        if name in ("native", "native-remove", "gosub-rtnnc"):
            valid = actual == expected
        elif name == "gosbvl":
            valid = (
                actual[:9] == expected[:9]
                and read(obj + 28) == obj + 35
                and read(obj + 33, 2) in (0x10, 0x20, 0x30)
                and read(obj + 35, 2) == 0x10
                and read(obj + 37, 1) == 0
            )
        else:
            tail = 10 if name in ("gosub", "gosub-rtnsxm") else 11
            valid = (
                actual[:tail] == expected[:tail]
                and actual[tail] in (0x10, 0x20, 0x30)
                and actual[tail + 1 :] == expected[tail + 1 :]
            )
        if not valid:
            raise ValueError("Native call operands/return payload changed")
        ids = [read(0x86120 + 16 * i, 3) for i in range(read(0x8611D, 3))]
        removed = name == "native-remove"
        expected_ids = original_ids if removed else sorted(original_ids + [123])
        if ids != expected_ids or len(ids) != len(set(ids)):
            raise ValueError("Native call did not produce the expected catalog")
        verify_bindings(original_records, read, removed=removed)
        results.append(
            dict(
                case=name,
                returned=True,
                flash_unchanged=True,
                rewritten_return=name not in ("native", "native-remove"),
                catalog_removed=removed,
            )
        )
        print(f"PASS: {args.model}: native {name} roots and catalog checks", flush=True)
    (output / "result.json").write_text(json.dumps(results, indent=2) + "\n")
