"""Run controlled direct-call continuations through the standard Stock launcher."""

import json

from fixtures import word


CASES = {
    "gosub": bytes([0x27, 0, 0x10, 0x10]),
    "gosub-rtnsxm": bytes([0x27, 0, 0x10, 0x00]),
    "gosub-rtnnc": bytes([0x37, 0, 0x05, 0, 0x03]),
    "gosubl": bytes([0xE8, 2, 0, 0x10, 0x10]),
    "gosbvl": bytes([0xF8, 0, 0, 0, 1, 1]),
}
MARKER = 0xC173A


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


def verify_calls(args, build, source, output, *, transfer, native_words):
    original_flash = (source / "flash").read_bytes()
    before = native_words((source / "sram").read_bytes(), original_flash)
    original_ids = [before(0x86120 + 16 * i, 3) for i in range(before(0x8611D, 3))]
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
            remote_commands=["-95 CF HOME CLEAR 'CALLROOT' RCL"],
        )
        state = case / "run/state"
        flash = (state / "flash").read_bytes()
        if flash != original_flash:
            raise ValueError("Call-root fixture changed Flash")
        read = native_words((state / "sram").read_bytes(), flash)
        stack = read(0x806F8)
        marker = read(stack)
        if read(marker) != 0x2911 or read(marker + 5) != MARKER:
            raise ValueError("Missing post-call native marker")
        obj = read(stack + 5)
        expected = b"HPCSCALL" + encoding
        if read(obj) != 0x2A2C or read(obj + 5) != 5 + 2 * len(expected):
            raise ValueError("Call fixture object changed")
        actual = bytes(read(obj + 10 + 2 * i, 2) for i in range(len(expected)))
        if name == "gosub-rtnnc":
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
        if ids != sorted(original_ids + [123]) or len(ids) != len(set(ids)):
            raise ValueError("Native call did not preserve the catalog and add library 123")
        results.append(dict(case=name, returned=True, flash_unchanged=True))
        print(f"PASS: {args.model}: native {name} roots and catalog growth", flush=True)
    (output / "result.json").write_text(json.dumps(results, indent=2) + "\n")
