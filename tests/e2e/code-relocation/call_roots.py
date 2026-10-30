"""Run controlled direct-call continuations through the standard Stock launcher."""

import json
import struct

from verify import home_string, home_object, require_stock_pc

from fixtures import word


CASES = {
    "native": b"",
    "native-new-message": b"",
    "native-missing-message": b"\0",
    "native-remove": b"\0",
    "native-detach": b"\1",
    "native-reattach": b"\2",
    "native-cache": b"\3",
    "native-cache-lookup": b"\4",
    "native-cache-message": b"\4",
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


def attachment_records(read):
    home = read(0x80711)
    if not 0x80000 <= home <= 0xFFFF8 or read(home) != 0x2A96:
        raise ValueError("Invalid native HOME")
    count = read(home + 5, 3)
    if count > (0x100000 - home - 8) // 13:
        raise ValueError("Invalid native attachment extent")
    return [
        tuple(read(home + 8 + 13 * i + offset, width) for offset, width in ((0, 3), (3, 5), (8, 5)))
        for i in range(count)
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


def verify_cache_lookup_program(read):
    lookup = home_object(read, b"CACHELOOK")
    expected = (
        word(0x2D9D)
        + word(0x389B9)
        + word(0x2E92)
        + word(275, 3)
        + word(0, 3)
        + word(0x389D4)
        + word(0x312B)
    )
    actual = "".join(format(read(lookup + i, 1), "x") for i in range(len(expected)))
    if actual != expected:
        raise ValueError("ROM did not resolve PrRUN to the original Stock program encoding")
    return actual


def verify_cache_message(read):
    message = home_string(read, b"CACHEMSG")
    if message != b"Invalid user RAM block":
        raise ValueError("ROM did not resolve library 275 message one")
    return message


def verify_calls(args, build, source, output, *, transfer, native_words):
    original_flash = (source / "flash").read_bytes()
    before = native_words((source / "sram").read_bytes(), original_flash)
    original_ids = [before(0x86120 + 16 * i, 3) for i in range(before(0x8611D, 3))]
    original_records = catalog_records(before)
    original_attachments = attachment_records(before)
    if sum(record[0] == 275 for record in original_attachments) != 1:
        raise ValueError("Call-root fixture requires one genuine library-275 attachment")
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
        # Check the producing run before restarting it for message/cache probes.
        # A saved ARM exception otherwise appears as a later USB/key failure.
        require_stock_pc(state)
        if name in ("native-new-message", "native-missing-message"):
            transfer(
                args,
                case / "message-run",
                state,
                package_dir,
                "CALLROOT",
                installed=True,
                execute=False,
                remote_commands=["IFERR #7B01h DOERR THEN ERRM 'NEWMSG' STO END"],
            )
            state = case / "message-run/state"
            require_stock_pc(state)
        if name in ("native-cache-lookup", "native-cache-message"):
            transfer(
                args,
                case / "lookup-run",
                state,
                package_dir,
                "CALLROOT",
                installed=True,
                execute=False,
                remote_commands=[
                    r"\{171} PrRUN \{187} 'CACHELOOK' STO"
                    if name == "native-cache-lookup"
                    else "IFERR #11301h DOERR THEN ERRM 'CACHEMSG' STO END"
                ],
            )
            state = case / "lookup-run/state"
            require_stock_pc(state)
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
        if name in (
            "native",
            "native-new-message",
            "native-missing-message",
            "native-remove",
            "native-detach",
            "native-reattach",
            "native-cache",
            "native-cache-lookup",
            "native-cache-message",
            "gosub-rtnnc",
        ):
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
        removed = name in (
            "native-remove",
            "native-missing-message",
            "native-detach",
            "native-reattach",
            "native-cache",
            "native-cache-lookup",
            "native-cache-message",
        )
        expected_ids = original_ids if removed else sorted(original_ids + [123])
        if ids != expected_ids or len(ids) != len(set(ids)):
            raise ValueError("Native call did not produce the expected catalog")
        verify_bindings(original_records, read, removed=removed)
        expected_attachments = [
            record
            for record in original_attachments
            if name
            not in (
                "native-detach",
                "native-reattach",
                "native-cache",
                "native-cache-lookup",
                "native-cache-message",
            )
            or record[0] != 275
        ]
        if not removed:
            cache_start = 0x86120 + 16 * len(original_ids)
            cache_end = before(0x806E9)

            def relocated(field):
                if not field or field < 0x80000:
                    return field
                if not cache_start <= field <= cache_end - 15:
                    raise ValueError("Fixture HOME field is outside reviewed cache storage")
                return field + 16

            expected_attachments = [
                (number, relocated(first), relocated(second))
                for number, first, second in expected_attachments
            ]
        if name in (
            "native-reattach",
            "native-cache",
            "native-cache-lookup",
            "native-cache-message",
        ):
            expected_attachments += [record for record in original_attachments if record[0] == 275]
        if name in ("native-cache-lookup", "native-cache-message"):
            original = next(r for r in original_attachments if r[0] == 275)
            current = next(r for r in attachment_records(read) if r[0] == 275)
            bottom = read(0x806E9)
            if current[2] != current[1] + 15:
                raise ValueError("New wrappers must remain adjacent")
            for old, new in zip(original[1:], current[1:]):
                if new == old or not 0x80000 <= new <= bottom - 15:
                    raise ValueError("Missing newly owned wrapper")
                if (
                    read(new) != 0x2BAA
                    or read(new + 5) != before(old + 5)
                    or read(new + 10) != before(old + 10)
                ):
                    raise ValueError("New wrapper differs from original table/switch identity")
            expected_attachments[-1] = current
            if name == "native-cache-lookup":
                verify_cache_lookup_program(read)
            else:
                verify_cache_message(read)
        if attachment_records(read) != expected_attachments:
            raise ValueError("Native call did not preserve the expected HOME attachments")
        if name in ("native-new-message", "native-missing-message"):
            message = home_string(read, b"NEWMSG")
            if name == "native-new-message" and message != b"hpclang library message":
                raise ValueError("Newly registered library message was not resolved")
            if name == "native-missing-message" and message == b"hpclang library message":
                raise ValueError("Unregistered library message unexpectedly resolved")
        results.append(
            dict(
                case=name,
                returned=True,
                flash_unchanged=True,
                rewritten_return=name
                not in (
                    "native",
                    "native-new-message",
                    "native-missing-message",
                    "native-remove",
                    "native-detach",
                    "native-reattach",
                    "native-cache",
                    "native-cache-lookup",
                    "native-cache-message",
                ),
                catalog_removed=removed,
                new_library_message=name == "native-new-message",
                missing_library_message=name == "native-missing-message",
                home_detached=name == "native-detach",
                cache_roundtrip=name == "native-cache",
                cache_lookup=name == "native-cache-lookup",
                cache_message=name == "native-cache-message",
                home_reattached=name
                in (
                    "native-reattach",
                    "native-cache",
                    "native-cache-lookup",
                    "native-cache-message",
                ),
            )
        )
        print(f"PASS: {args.model}: native {name} roots and catalog checks", flush=True)
    (output / "result.json").write_text(json.dumps(results, indent=2) + "\n")
