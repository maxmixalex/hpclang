"""Build the HP50g Stock-to-ARM entry fixture without ARMToolbox.

This is an entry/return experiment, not an HPC1 application launcher.
"""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def word(value, width=5):
    if not 0 <= value < 1 << width * 4:
        raise ValueError("Saturn immediate overflow")
    return "".join((f"{value >> 4 * i & 15:x}" for i in range(width)))


def code_for(size, offset):
    if size < 8 or size % 8 or size > 32768:
        raise ValueError("payload must contain 1..4096 complete eight-byte blocks")
    # HP50g SAVE, capture PC, then copy into the program object's reserved RAM.
    code = "8f" + word(0x679B) + "20" + "81b5"
    code += "da" + "34" + word(offset) + "c2" + "134"
    code += "da" + "34" + word(size * 2 + 7) + "c2"
    code += "80880" + "80881" + "80882" + "135" + "d5"
    code += "34" + word(size // 8 - 1)
    code += "1527" + "1517" + "16f" + "17f" + "ce" + "5fe"
    code += "af2" + "d9" + "81e" + "25" + "32" + "080" + "20"
    # ARMSYS followed by Stock RESTORE and RPL return; no library call.
    code += "80bfe" + "8f" + word(0x67D2) + "142164808c"
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True, help="LLVM bin directory")
    parser.add_argument("--output", type=Path, required=True, help="new output directory")
    parser.add_argument("--bootstrap-sync", type=Path, help="shared runtime/bootstrap/sync.S")
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    tool = args.toolchain.resolve()
    source = Path(__file__).resolve().parent
    sync = args.bootstrap_sync or Path(__file__).resolve().parents[3] / "runtime/bootstrap/sync.S"
    inputs = {name: source / name for name in ("entry.S", "probe.c", "link.ld", "build.py")}
    inputs["bootstrap-sync.S"] = sync
    common = [
        str(tool / "clang"),
        "--target=arm-none-eabi",
        "-march=armv4t",
        "-marm",
        "-ffreestanding",
        "-fno-builtin",
        "-Oz",
    ]
    for name in ("entry.S", "probe.c", "bootstrap-sync.S"):
        subprocess.run(
            common + ["-c", str(inputs[name]), "-o", str(root / (name + ".o"))],
            check=True,
        )
    subprocess.run(
        common
        + [
            "-nostdlib",
            "-Wl,-T," + str(source / "link.ld"),
            str(root / "entry.S.o"),
            str(root / "probe.c.o"),
            str(root / "bootstrap-sync.S.o"),
            "-o",
            str(root / "probe.elf"),
        ],
        check=True,
    )
    subprocess.run(
        [
            str(tool / "llvm-objcopy"),
            "-O",
            "binary",
            "-j",
            ".text",
            str(root / "probe.elf"),
            str(root / "probe.bin"),
        ],
        check=True,
    )
    symbols = {}
    for line in subprocess.check_output(
        [str(tool / "llvm-nm"), str(root / "probe.elf")], text=True
    ).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)

    blob = (root / "probe.bin").read_bytes()
    blob += bytes((-len(blob)) % 8)
    code = code_for(len(blob), len(code_for(len(blob), 0)) - 13)
    payload = "".join(f"{b & 15:x}{b >> 4:x}" for b in blob)
    reserve = len(payload) + 7
    body = code + payload + "0" * reserve
    program = (
        word(0x2D9D)
        + word(0x2DCC)
        + word(5 + len(body))
        + body
        + word(0x2614)
        + word(12)
        + "5724210"
        + word(0x312B)
    )
    if len(program) & 1:
        program += "0"
    encoded = bytes(
        int(program[i], 16) | (int(program[i + 1], 16) << 4) for i in range(0, len(program), 2)
    )
    (root / "program.HP").write_bytes(b"HPHP49-C" + encoded)
    (root / "metadata.json").write_text(
        json.dumps(
            dict(
                symbols=symbols,
                size=len(blob),
                prefix=15 + len(code) + len(payload),
                reserve=reserve,
                blob=blob.hex(),
            )
        )
    )
    manifest = {
        "scope": "HP50g independent entry and return only",
        "inputs": {
            name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in inputs.items()
        },
        "compiler": subprocess.check_output([str(tool / "clang"), "--version"], text=True),
        "outputs": {
            name: hashlib.sha256((root / name).read_bytes()).hexdigest()
            for name in ("program.HP", "probe.bin", "probe.elf", "metadata.json")
        },
    }
    (root / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Built {len(blob)}-byte independent entry payload in {root}")


if __name__ == "__main__":
    main()
