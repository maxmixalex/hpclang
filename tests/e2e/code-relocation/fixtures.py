"""Build controlled Saturn branch programs, with repaired/unrepaired pairs."""

import argparse
from pathlib import Path

PASS = 0xC173A
FAIL = 0xFA173
RETURN = 0xA173C


def word(value, width=5):
    return "".join(format((value >> (4 * i)) & 15, "x") for i in range(width))


def load_marker(value):
    # LC starts at P. Reset P after tests which deliberately change it.
    return "2034" + word(value)


def jump(address, target):
    return "6" + word(target - address - 1, 3)


def build_cases():
    # Stock 2.15 SAVE/GETPTR and ordinary Saturn instructions. SAVE retains
    # the caller's RPL pointers; the epilogue writes the top HEXSTR via A/D0
    # without changing C, restores the pointers, and resumes the runstream.
    save = "8f" + word(0x679B)
    finish = "201b" + word(0x806F8) + "142130142130169144"
    finish += "8f" + word(0x67D2) + "142164808c"
    templates = [
        ("goto", "", "6", 3, False),
        ("gosub", "", "7", 3, True),
        ("golong", "", "8c", 4, False),
        ("gosubl", "", "8e", 4, True),
        ("carry-set", "af2ce", "4", 2, False),
        ("carry-clear", "af2e6", "5", 2, False),
        ("hst", "82f", "831", 2, False),
        ("st-clear", "840", "860", 2, False),
        ("st-set", "850", "870", 2, False),
        ("p-not-equal", "21", "880", 2, False),
        ("p-equal", "20", "890", 2, False),
        ("equal-a", "af0af1", "8a0", 2, False),
        ("order-a", "af0af1", "8b8", 2, False),
        ("equal-w", "af0af1", "970", 2, False),
        ("order-w", "af0af1", "9f8", 2, False),
        ("a-bit-clear", "80842", "80862", 2, False),
        ("a-bit-set", "80852", "80872", 2, False),
        ("c-bit-clear", "80882", "808a2", 2, False),
        ("c-bit-set", "80892", "808b2", 2, False),
    ]
    objects, cases = [], []
    for name, setup, prefix, width, call in templates:
        for repair in (False, True):
            body = save + setup
            at, length = len(body), len(prefix) + width
            old = at + length + 16
            new, epilogue = old + 16, old + 32
            base = at + (length if call else len(prefix))
            body += prefix + word(old - base, width)
            if not call:
                body += load_marker(FAIL)
            body += jump(len(body), epilogue)
            assert len(body) <= old
            body += "0" * (old - len(body)) + load_marker(FAIL)
            body += "01" if call else jump(len(body), epilogue)
            assert len(body) <= new
            body += "0" * (new - len(body)) + load_marker(PASS)
            body += "01" if call else jump(len(body), epilogue)
            assert len(body) <= epilogue
            body += "0" * (epilogue - len(body)) + finish
            cases.append(
                dict(
                    name=name,
                    at=at,
                    length=length,
                    old=old,
                    body=len(body),
                    repair=repair,
                    move=False,
                    value=PASS if repair else FAIL,
                )
            )
            objects.append(word(0x2A4E) + word(10) + "00000")
            objects.append(word(0x2DCC) + word(len(body) + 5) + body)
        for repair in (False, True):
            # Enter the destination only after ARM has copied the instruction.
            # Its target remains fixed. Without repair the moved instruction
            # reaches the failure stub, exactly 32 nibbles beyond the target.
            body = save + setup
            at = len(body) + 4
            length = len(prefix) + width
            destination, target, trap, epilogue = at + 32, at + 64, at + 96, at + 128
            body += jump(len(body), destination)
            base = at + (length if call else len(prefix))
            body += prefix + word(target - base, width)
            body += "0" * (destination - len(body) + length)
            if not call:
                body += load_marker(FAIL)
            body += jump(len(body), epilogue)
            body += "0" * (target - len(body)) + load_marker(PASS)
            body += "01" if call else jump(len(body), epilogue)
            body += "0" * (trap - len(body)) + load_marker(FAIL)
            body += "01" if call else jump(len(body), epilogue)
            body += "0" * (epilogue - len(body)) + finish
            cases.append(
                dict(
                    name=name,
                    at=at,
                    length=length,
                    old=at,
                    body=len(body),
                    repair=repair,
                    move=True,
                    value=PASS if repair else FAIL,
                )
            )
            objects.append(word(0x2A4E) + word(10) + "00000")
            objects.append(word(0x2DCC) + word(len(body) + 5) + body)
    for register, prefix, read in ((0, "1b", "146"), (1, "1f", "147")):
        for repair in (False, True):
            at = len(save)
            body = save + prefix + word(0) + read + finish
            old = len(body)
            body += word(PASS) + "0" * 11 + word(0)
            cases.append(
                dict(
                    name=f"data-d{register}",
                    at=at,
                    length=7,
                    old=old,
                    body=len(body),
                    repair=repair,
                    move=False,
                    pointer=True,
                    value=PASS if repair else FAIL,
                )
            )
            objects.append(word(0x2A4E) + word(10) + "00000")
            objects.append(word(0x2DCC) + word(len(body) + 5) + body)
    return "".join(objects), cases


def package(launcher):
    if launcher[:8] != b"HPHP49-C":
        raise ValueError("Expected wrapped HP49/50 launcher")
    encoded = "".join(format(v & 15, "x") + format(v >> 4, "x") for v in launcher[8:])
    if len(encoded) < 10 or int(encoded[:5][::-1], 16) != 0x2A2C:
        raise ValueError("Expected launcher string")
    size = int(encoded[5:10][::-1], 16) + 5
    if size > len(encoded) or len(encoded) - size > 1:
        raise ValueError("Invalid launcher extent")
    tail, cases = build_cases()
    program = word(0x2D9D) + encoded[:size] + word(0x2E92) + word(275, 3) + word(0, 3)
    program += tail + word(0x2911) + word(RETURN) + word(0x312B)
    program += "0" * (len(program) % 2)
    return b"HPHP49-C" + bytes(
        int(program[i], 16) | (int(program[i + 1], 16) << 4) for i in range(0, len(program), 2)
    ), cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("header", type=Path)
    args = parser.parse_args()
    _, cases = build_cases()
    rows = []
    for case in cases:
        values = [
            case.get(key, False)
            for key in ("at", "length", "old", "body", "repair", "move", "pointer")
        ]
        rows.append("    {" + ", ".join(str(int(value)) for value in values) + "}")
    args.header.write_text(
        "/* Generated by fixtures.py; offsets are relative to each CODE body. */\n"
        "static const struct { uint32_t at, length, old, body, repair, move, pointer; } cases[] = {\n"
        + ",\n".join(rows)
        + "\n};\n"
    )


if __name__ == "__main__":
    main()
