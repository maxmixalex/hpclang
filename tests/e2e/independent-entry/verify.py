"""Check HP50g independent-entry results in an emulator snapshot."""

import argparse
import hashlib
import json
import struct
from pathlib import Path

ROM_SHA256 = "be21f48a44feb9a899a1e9c31799a55e8d1cbdd7e3774e868d64062e41f5105c"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def mapping(ram, flash):
    require(len(ram) == 0x80000, "expected HP50g 512 KiB SRAM")
    words = struct.unpack("<131072I", ram)
    matches = []
    for table, value in enumerate(words[:-128]):
        if not 0x08000000 <= value < 0x08000000 + len(ram) or value % 2048:
            continue
        if any(words[table + j] != value + j * 2048 for j in (1, 2, 3, 16, 64, 127)):
            continue

        def nib(addr, table=table):
            require(0 <= addr < 0x100000, "Saturn address out of range")
            index = table + (addr >> 12) - 128
            require(0 <= index < len(words), "mapping table out of bounds")
            physical = words[index] + (addr & 4095) // 2
            data, offset = (
                (ram, physical - 0x08000000) if physical >= 0x08000000 else (flash, physical)
            )
            require(0 <= offset < len(data), "mapped byte out of bounds")
            return (data[offset] >> (4 * (addr & 1))) & 15

        def word(at, count=5, nib=nib):
            return sum(nib(at + i) << (4 * i) for i in range(count))

        try:
            home = word(0x80711)
            if home >= 0x80000 and word(home) == 0x2A96:
                matches.append((nib, word, words[table : table + 128]))
        except ValueError:
            continue
    require(bool(matches), "native mapping not found")
    require(all(m[2] == matches[0][2] for m in matches), "ambiguous SRAM mapping")
    return matches[0][:2]


def no_toolbox(ram, flash):
    nib, word = mapping(ram, flash)
    count = word(0x8611D, 3)
    require(count < 128, "invalid library catalog count")
    require(all(word(0x86120 + 16 * i, 3) != 275 for i in range(count)), "library 275 present")
    return nib, word, count


def verify(artifact, root, baseline):
    flash = (root / "flash").read_bytes()
    require(hashlib.sha256(flash[:0x140000]).hexdigest() == ROM_SHA256, "unqualified HP50g ROM")
    no_toolbox((baseline / "sram").read_bytes(), (baseline / "flash").read_bytes())
    ram = (root / "sram").read_bytes()
    nib, word, _ = no_toolbox(ram, flash)
    manifest = json.loads((artifact / "build.json").read_text())
    for name, digest in manifest["outputs"].items():
        require(Path(name).name == name, "invalid manifest filename")
        require(
            hashlib.sha256((artifact / name).read_bytes()).hexdigest() == digest,
            "artifact changed: " + name,
        )

    obj = word(word(0x806F8))
    require(
        word(obj) == 0x2614 and word(obj + 5) == 12 and (nib(obj + 16) == 0),
        "word(obj) == 0x2614 and word(obj + 5) == 12 and (nib(obj + 16) == 0)",
    )
    require(
        sum((nib(obj + 10 + i) * 10**i for i in range(6))) == 124275,
        "sum((nib(obj + 10 + i) * 10 ** i for i in range(6))) == 124275",
    )
    meta = json.loads((artifact / "metadata.json").read_text())
    symbols = meta["symbols"]
    marker = struct.pack("<I", 0x48504343)
    hits = [i for i in range(len(ram) - 3) if ram.startswith(marker, i)]
    candidates = []
    expected = bytes.fromhex(meta["blob"])
    for hit in hits:
        start = hit - symbols["workspace"]
        if start >= 0 and ram[start : start + 32] == expected[:32]:
            candidates.append(start)
    require(len(candidates) == 1, "len(candidates) == 1")
    start = candidates[0]
    require(start % 4 == 0, "start % 4 == 0")
    physical = 0x08000000 + start
    result = struct.unpack_from("<134I", ram, start + symbols["workspace"])
    checksum = 0x12345678
    for i in range(128):
        value = ((i % 32) * 0x10203 + 7) ^ (i * 0x01010101)
        require(result[6 + i] == value, "result[6 + i] == value")
        checksum = (checksum << 3 | checksum >> 29) & 0xFFFFFFFF ^ value
    require(result[1] == checksum, "result[1] == checksum")
    low, high = (physical + symbols["stack_start"], physical + symbols["stack_end"])
    require(
        low <= result[2] and result[2] + 128 <= high, "low <= result[2] and result[2] + 128 <= high"
    )
    require(
        low <= result[3] <= result[2] and result[3] % 8 == 0,
        "low <= result[3] <= result[2] and result[3] % 8 == 0",
    )
    require(
        result[4] == physical + symbols["workspace"] and result[5] == result[4] + 536,
        "result[4] == physical + symbols['workspace'] and result[5] == result[4] + 536",
    )
    actual = ram[start : start + meta["size"]]
    require(len(actual) == len(expected) == meta["size"], "truncated payload")
    for index, (before, after) in enumerate(zip(expected, actual)):
        if symbols["workspace"] <= index < symbols["workspace"] + 536:
            continue
        if symbols["stack_start"] <= index < symbols["stack_end"]:
            continue
        require(before == after, "before == after")

    def nibbles(data):
        return "".join((f"{b & 15:x}{b >> 4:x}" for b in data))

    program = nibbles((artifact / "program.HP").read_bytes()[8:])
    prefix = program[: meta["prefix"]]
    packed = nibbles(ram)
    owners = [
        i
        for i in range(len(packed))
        if packed.startswith(prefix, i)
        and i + len(prefix) <= start * 2
        and (start * 2 + meta["size"] * 2 <= i + len(prefix) + meta["reserve"])
    ]
    require(len(owners) == 1, "len(owners) == 1")
    require(
        flash == (baseline / "flash").read_bytes(), "flash == (baseline / 'flash').read_bytes()"
    )
    return {
        "complete": True,
        "scope": "HP50g entry/return snapshot",
        "payload_physical": physical,
        "owned_stack_bytes": high - low,
        "program_sha256": hashlib.sha256((artifact / "program.HP").read_bytes()).hexdigest(),
        "state_sha256": {
            name: hashlib.sha256((root / name).read_bytes()).hexdigest()
            for name in ("flash", "sram")
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", type=Path, required=True)
    parser.add_argument("--state", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.artifact, args.state, args.baseline)
    args.report.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
