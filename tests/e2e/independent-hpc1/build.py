"""Build a bounded HP50g HPC1 experiment without library 275 or ROM inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess

FIXTURE = Path("tests/e2e/independent-hpc1")
INPUTS = (
    "tests/e2e/independent-hpc1/build.py",
    "tests/e2e/independent-hpc1/app.c",
    "tests/e2e/independent-hpc1/loader.c",
    "tests/e2e/independent-hpc1/link.ld",
    "tests/e2e/independent-entry/build.py",
    "tests/e2e/independent-entry/entry.S",
    "include/hpclang/abi.h",
    "include/hpclang/bootstrap.h",
    "runtime/bootstrap/mmu.c",
    "runtime/bootstrap/run.c",
    "runtime/bootstrap/enter.S",
    "runtime/bootstrap/sync.S",
    "platform/armtoolbox/stage-image.c",
    "platform/armtoolbox/stage-image.h",
    "third_party/tinf/tinf.h",
)
OUTPUTS = ("app.hpc", "app.elf", "probe.bin", "probe.elf", "metadata.json", "program.HP")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(source, tool, output, model="hp50g"):
    model_id = {"hp49gplus": 5, "hp50g": 6}[model]
    output.mkdir(parents=True, exist_ok=False)
    inputs = {name: digest(source / name) for name in INPUTS}
    common = [
        str(tool / "clang"),
        "--target=arm-none-eabi",
        "-march=armv4t",
        "-ffreestanding",
        f"-DHPCLANG_PROBE_MODEL={model_id}",
        "-fno-builtin",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
        "-ffunction-sections",
        "-fdata-sections",
        "-Oz",
        "-I" + str(source / "include"),
    ]

    def run(args):
        subprocess.run(args, cwd=output, check=True)

    def compile_file(path, name, thumb=False):
        run(common + ["-mthumb" if thumb else "-marm", "-c", str(path), "-o", name])

    def symbols(name):
        result = {}
        for line in subprocess.check_output(
            [str(tool / "llvm-nm"), str(output / name)], text=True
        ).splitlines():
            fields = line.split()
            if len(fields) == 3:
                result[fields[2]] = int(fields[0], 16)
        return result

    compile_file(source / FIXTURE / "app.c", "app.o", True)
    compile_file(source / "runtime/bootstrap/sync.S", "sync.o")
    run(
        common
        + [
            "-mthumb",
            "-nostdlib",
            "-Wl,--gc-sections",
            "-Wl,-Ttext,0x01220000",
            "-Wl,-e,app",
            "app.o",
            "sync.o",
            "-o",
            "app.elf",
        ]
    )
    run([str(tool / "llvm-objcopy"), "-O", "binary", "-j", ".text", "app.elf", "app.bin"])
    payload = (output / "app.bin").read_bytes()
    entry = (symbols("app.elf")["app"] - 0x01220000) | 1
    if not 0 < entry < len(payload) <= 4096:
        raise ValueError("fixture app exceeds the single-page HPC1 contract")
    (output / "app.hpc").write_bytes(
        struct.pack("<4sIII", b"HPC1", len(payload), entry, 12) + payload
    )

    assembly = (source / "tests/e2e/independent-entry/entry.S").read_text()
    replacements = {
        " bl exercise": " ldr r2, arena_offset\n add r1, r6, r2\n ldr r2, image_offset\n add r2, r6, r2\n ldr r3, image_bytes\n bl exercise",
        "workspace_offset: .word workspace-entry": "workspace_offset: .word workspace-entry\narena_offset: .word arena-entry\nimage_offset: .word hpc-entry\nimage_bytes: .word hpc_end-hpc",
    }
    for old, new in replacements.items():
        if assembly.count(old) != 1:
            raise ValueError("shared entry no longer has the expected call/offset structure")
        assembly = assembly.replace(old, new)
    assembly += '\n.balign 8\n.global arena\narena: .space 28672\n.global arena_end\narena_end:\n.balign 4\n.global hpc\nhpc: .incbin "app.hpc"\nhpc_end:\n'
    (output / "entry.S").write_text(assembly)
    compile_file(output / "entry.S", "entry.o")
    run(
        common
        + [
            "-marm",
            "-I" + str(source / "platform/armtoolbox"),
            "-c",
            str(source / FIXTURE / "loader.c"),
            "-o",
            "probe.o",
        ]
    )
    for name in ("mmu.c", "run.c", "enter.S"):
        compile_file(source / "runtime/bootstrap" / name, name + ".o")
    compile_file(source / "platform/armtoolbox/stage-image.c", "stage-image.o")
    run(
        common
        + [
            "-marm",
            "-nostdlib",
            "-Wl,--gc-sections",
            "-Wl,-T," + str(source / FIXTURE / "link.ld"),
            "entry.o",
            "probe.o",
            "mmu.c.o",
            "run.c.o",
            "enter.S.o",
            "sync.o",
            "stage-image.o",
            "-o",
            "probe.elf",
        ]
    )
    run([str(tool / "llvm-objcopy"), "-O", "binary", "-j", ".text", "probe.elf", "probe.bin"])
    blob = (output / "probe.bin").read_bytes()
    blob += bytes(-len(blob) % 8)
    encoder = runpy.run_path(str(source / "tests/e2e/independent-entry/build.py"))
    word, code_for = encoder["word"], encoder["code_for"]
    code = code_for(len(blob), len(code_for(len(blob), 0)) - 13)
    packed = "".join(f"{b & 15:x}{b >> 4:x}" for b in blob)
    reserve = len(packed) + 7
    body = code + packed + "0" * reserve
    program = word(0x2D9D) + word(0x2DCC) + word(5 + len(body)) + body
    program += word(0x2614) + word(12) + "5724210" + word(0x312B)
    if len(program) & 1:
        program += "0"
    (output / "program.HP").write_bytes(
        b"HPHP49-C"
        + bytes(
            int(program[i], 16) | int(program[i + 1], 16) << 4 for i in range(0, len(program), 2)
        )
    )
    metadata = dict(
        symbols=symbols("probe.elf"),
        size=len(blob),
        prefix=15 + len(code) + len(packed),
        reserve=reserve,
        blob=blob.hex(),
    )
    (output / "metadata.json").write_text(json.dumps(metadata) + "\n")
    if any(digest(source / name) != expected for name, expected in inputs.items()):
        raise ValueError("build inputs changed")
    manifest = dict(
        scope="HP49g+/HP50g bounded embedded HPC1 experiment; no native-service context",
        model=model,
        inputs=inputs,
        outputs={name: digest(output / name) for name in OUTPUTS},
        compiler=subprocess.check_output([str(tool / "clang"), "--version"], text=True),
    )
    (output / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Built {len(blob)}-byte payload in {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--model", choices=("hp49gplus", "hp50g"), default="hp50g")
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    build(args.source_root.resolve(), args.toolchain.resolve(), args.output.resolve(), args.model)


if __name__ == "__main__":
    main()
