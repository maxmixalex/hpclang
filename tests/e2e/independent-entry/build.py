"""Build the model-specific Stock-to-ARM entry fixture without ARMToolbox.

This is an entry/return experiment, not an HPC1 application launcher.
"""

import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path


def bitmap_image_info(path):
    image = path.read_bytes()
    if len(image) < 16:
        raise ValueError("truncated HPC1 header")
    magic, size, entry, static = struct.unpack_from("<4sIII", image)
    if (magic, size, entry) != (b"HPC1", len(image) - 16, 8) or not 0 < size <= 12 * 4096:
        raise ValueError("image does not fit the D1.55 editor fixture")
    if static > 4096:
        raise ValueError("unexpected static data requirement for the editor fixture")
    checksum = 2166136261
    for byte in image[16:]:
        checksum = ((checksum ^ byte) * 16777619) & 0xFFFFFFFF
    return size, checksum


def word(value, width=5):
    if not 0 <= value < 1 << width * 4:
        raise ValueError("Saturn immediate overflow")
    return "".join((f"{value >> 4 * i & 15:x}" for i in range(width)))


def moving_gc_program():
    # Make an unreachable temporary and capture the tagged object's address
    # before the original GARBAGECOL, so actual movement is observable.
    dead = word(0x2A4E) + word(1029) + "0" * 1024
    target = word(0x2A4E) + word(37) + "12345678" * 4
    seed = (
        "8f"
        + word(0x679B)
        + "20"
        + "143"
        + "1b"
        + word(0x8078E)
        + "146136"
        + "16f" * 6
        + "163"
        + "140"
        + "8f"
        + word(0x67D2)
        + "142164808c"
    )
    return (
        dead
        + word(0x6657)
        + word(0x3244)
        + target
        + word(0x6657)
        + word(0x2DCC)
        + word(5 + len(seed))
        + seed
        + word(0x5F42)
    )


def code_for(size, offset, model="hp50g"):
    if size < 8 or size % 8 or size > 65536:
        raise ValueError("payload must contain 1..8192 complete eight-byte blocks")
    # Save Stock pointers, capture PC, and copy into program-owned aligned RAM.
    save, restore = (0x6F73, 0xF30C) if model == "hp39gplus" else (0x679B, 0x67D2)
    code = "8f" + word(save) + "20" + "81b5"
    code += "da" + "34" + word(offset) + "c2" + "134"
    code += "da" + "34" + word(size * 2 + 7) + "c2"
    code += "80880" + "80881" + "80882" + "135" + "d5"
    code += "34" + word(size // 8 - 1)
    code += "1527" + "1517" + "16f" + "17f" + "ce" + "5fe"
    code += "af2" + "d9" + "81e"
    if model in ("hp48gii", "hp39gplus"):
        # Translate the main-RAM nibble address before converting it to bytes.
        code += "da" + "34" + word(0xC8000) + "c2"
    code += "25" + "32" + "080" + "20"
    # ARMSYS followed by Stock RESTORE and RPL return; no library call.
    code += "80bfe" + "8f" + word(restore) + "142164808c"
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True, help="LLVM bin directory")
    parser.add_argument("--model", choices=("hp48gii", "hp49gplus", "hp50g"), default="hp50g")
    parser.add_argument("--output", type=Path, required=True, help="new output directory")
    parser.add_argument("--bootstrap-sync", type=Path, help="shared runtime/bootstrap/sync.S")
    probe = parser.add_mutually_exclusive_group()
    probe.add_argument("--probe-source", type=Path, help="alternative bounded experiment body")
    probe.add_argument(
        "--hp39-notebook-stage", action="store_true", help="stage the HP39g+ Notebook in a bitmap"
    )
    probe.add_argument(
        "--hp39-notebook-editor", action="store_true", help="enter the staged HP39g+ Notebook"
    )
    parser.add_argument(
        "--stock-state", action="store_true", help="pass original Saturn state to probe"
    )
    parser.add_argument("--stock-frame", action="store_true", help="also pass saved register frame")
    parser.add_argument("--bitmap-image", type=Path, help="compile against this Notebook HPC1")
    parser.add_argument(
        "--aplet-writeback", action="store_true", help="enable owned Aplet data in the editor"
    )
    args = parser.parse_args()
    if args.aplet_writeback and not args.hp39_notebook_editor:
        parser.error("--aplet-writeback requires --hp39-notebook-editor")
    bitmap_image = args.hp39_notebook_stage or args.hp39_notebook_editor
    if args.bitmap_image and not bitmap_image:
        parser.error("--bitmap-image requires a Notebook bitmap probe")
    if bitmap_image:
        args.stock_state = True
    if args.stock_frame and not args.stock_state:
        parser.error("--stock-frame requires --stock-state")
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    tool = args.toolchain.resolve()
    source = Path(__file__).resolve().parent
    sync = args.bootstrap_sync or Path(__file__).resolve().parents[3] / "runtime/bootstrap/sync.S"
    inputs = {name: source / name for name in ("entry.S", "probe.c", "link.ld", "build.py")}
    inputs["bootstrap-sync.S"] = sync
    if args.probe_source:
        inputs["probe.c"] = args.probe_source.resolve()
    if bitmap_image:
        inputs["probe.c"] = source.parent / "independent-pool/hp39-bitmap-probe.c"
    common = [
        str(tool / "clang"),
        "--target=arm-none-eabi",
        "-march=armv4t",
        "-marm",
        "-ffreestanding",
        "-fno-builtin",
        "-Oz",
    ]
    if args.stock_state:
        common.append("-DHPCLANG_PROBE_STOCK_STATE")
    if bitmap_image:
        common.append("-DHPCLANG_PROBE_NOTEBOOK_STAGE")
        if args.bitmap_image:
            size, checksum = bitmap_image_info(args.bitmap_image)
            common += [f"-DNOTEBOOK_BYTES={size}", f"-DNOTEBOOK_FNV1A={checksum}u"]
            inputs["notebook.hpc"] = args.bitmap_image
    if args.stock_frame:
        common.append("-DHPCLANG_PROBE_STOCK_FRAME")
    sources = ["entry.S", "probe.c", "bootstrap-sync.S"]
    if args.hp39_notebook_editor:
        repo = source.parents[2]
        if args.aplet_writeback:
            common.append("-DHPCLANG_PROBE_APLET_WRITEBACK")
        common += ["-DHPCLANG_PROBE_NOTEBOOK_EDITOR", "-I" + str(repo / "include")]
        bootstrap = ["mmu.c", "run.c", "enter.S"]
        inputs.update({name: repo / "runtime/bootstrap" / name for name in bootstrap})
        sources = bootstrap + sources
    for name in sources:
        subprocess.run(
            common + ["-c", str(inputs[name]), "-o", str(root / (name + ".o"))],
            check=True,
        )
    subprocess.run(
        common
        + [
            "-nostdlib",
            "-Wl,-T," + str(source / "link.ld"),
            *(str(root / (name + ".o")) for name in sources),
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
    offset = len(code_for(len(blob), 0, args.model)) - 13
    code = code_for(len(blob), offset, args.model)
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
        "scope": "Independent entry and return only",
        "model": args.model,
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
