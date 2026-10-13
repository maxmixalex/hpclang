#!/usr/bin/env python3
"""Build calculator launchers from an HPC1 application using the installed SDK."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


# Stable model numbers from the public hpclang target ABI.
MODELS = {"hp39gplus": 1, "hp39gs": 2, "hp40gs": 3, "hp48gii": 4, "hp49gplus": 5, "hp50g": 6}


def run(*args: str | Path) -> str:
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{args!r}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def build(
    root: Path,
    tools: Path,
    payload: Path,
    output: Path,
    models: list[str],
    *,
    wrap: bool = True,
    hp39_armsys_probe: bool = False,
) -> None:
    if payload.read_bytes()[:4] != b"HPC1":
        raise ValueError("expected an HPC1 application")
    output.mkdir(parents=True, exist_ok=True)
    scratch = output / "objects"
    scratch.mkdir(exist_ok=True)
    cc, ld, nm = (tools / name for name in ("clang", "ld.lld", "llvm-nm"))
    flags = [
        "--target=arm-none-eabi",
        "-march=armv4t",
        "-mfpu=none",
        "-mfloat-abi=soft",
        "-mabi=aapcs",
        "-marm",
        "-fno-stack-protector",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
        "-fno-pic",
        "-fno-pie",
        "-ffunction-sections",
        "-fdata-sections",
        "-Os",
        "-I" + str(root / "include"),
    ]

    def compile_source(source: Path, name: str, *extra: str) -> Path:
        obj = scratch / (name + ".o")
        run(cc, *flags, *extra, "-c", source, "-o", obj)
        return obj

    objects = [
        compile_source(root / "runtime/bootstrap" / source, "bootstrap-" + name)
        for source, name in (
            ("mmu.c", "mmu"),
            ("run.c", "run"),
            ("sync.S", "sync"),
            ("enter.S", "enter"),
        )
    ]
    platform = root / "platform/armtoolbox"
    objects = [
        compile_source(platform / source, name)
        for source, name in (
            ("stage1-entry.S", "stage1-entry"),
            ("stage1.c", "stage1"),
            ("hp39-portal.S", "stage1-hp39-portal"),
        )
    ] + objects
    script = scratch / "stage1.ld"
    run(cc, "-E", "-P", "-x", "c", platform / "stage1.ld", "-o", script)
    stage1_elf = scratch / "stage1.elf"
    run(ld, "-m", "armelf", "-T", script, *objects, "-o", stage1_elf)
    if run(nm, "-u", stage1_elf).strip():
        raise ValueError("unresolved stage1 symbols")
    sizes = [
        int(line.split()[0], 16)
        for line in run(nm, stage1_elf).splitlines()
        if line.split()[-1] == "__stage1_size"
    ]
    if len(sizes) != 1:
        raise ValueError("missing stage1 memory extent")
    stage1 = scratch / "stage1.bin"
    run(tools / "llvm-objcopy", "-O", "binary", stage1_elf, stage1)
    if stage1.stat().st_size > sizes[0]:
        raise ValueError("stage1 file exceeds its reserved memory extent")

    def assembly_path(path: Path) -> str:
        return str(path.resolve()).replace("\\", "/").replace('"', '\\"')

    bundle = scratch / "bundle.S"
    bundle.write_text(
        f'''\
.section .rodata.hpclang.bundle,"a",%progbits
.align 2
.global __stage1_blob_start, __stage1_blob_end, __stage1_blob_size, __stage1_memory_size
.global __hpc1_blob_start, __hpc1_blob_end
.set __stage1_memory_size, {sizes[0]}
__stage1_blob_start:
.incbin "{assembly_path(stage1)}"
__stage1_blob_end:
.set __stage1_blob_size, __stage1_blob_end - __stage1_blob_start
.align 2
__hpc1_blob_start:
.incbin "{assembly_path(payload)}"
__hpc1_blob_end:
''',
        encoding="utf-8",
    )
    bundle_object = compile_source(bundle, "bundle")
    for model in models:
        definitions = [f"-DHPCLANG_STAGE0_MODEL={MODELS[model]}"]
        if hp39_armsys_probe and MODELS[model] <= 3:
            definitions.append("-DHPCLANG_STAGE0_ARMSYS_PROBE=1")
        entry = compile_source(platform / "stage0.S", model, *definitions)
        elf = output / (model + ".elf")
        run(ld, "-m", "armelf", "-T", platform / "stage0.ld", entry, bundle_object, "-o", elf)
        binary = output / (model + ".bin")
        run(tools / "llvm-objcopy", "-O", "binary", elf, binary)
        if wrap:
            # Use the same native object envelope as repository execution tests.
            run(sys.executable, root / "tools/armtoolbox-wrap.py", binary, output / (model + ".hp"))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--tool-bin", type=Path, required=True)
    parser.add_argument("--models", choices=MODELS, nargs="+", default=list(MODELS))
    parser.add_argument("--raw-only", action="store_true")
    parser.add_argument("--hp39-armsys-probe", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    build(
        Path(__file__).resolve().parents[1],
        args.tool_bin,
        args.payload,
        args.output,
        args.models,
        wrap=not args.raw_only,
        hp39_armsys_probe=args.hp39_armsys_probe,
    )


if __name__ == "__main__":
    main()
