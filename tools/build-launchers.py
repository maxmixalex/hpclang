#!/usr/bin/env python3
"""Build calculator launchers from an HPC1 application using the installed SDK."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import zlib


# Stable model numbers from the public hpclang target ABI.
MODELS = {"hp39gplus": 1, "hp39gs": 2, "hp40gs": 3, "hp48gii": 4, "hp49gplus": 5, "hp50g": 6}
# HPC1 execution ABI; test_launcher_memory checks these against the SDK headers.
PAGE_BYTES = 4096
STACK_BYTES = 8192
MAX_RW_PAGES = 256


def validate_rw_limit(payload: bytes, limit: int | None) -> None:
    if len(payload) < 16 or payload[:4] != b"HPC1":
        raise ValueError("expected an HPC1 application header")
    if limit is None:
        return
    if (
        type(limit) is not int
        or limit <= 0
        or limit % PAGE_BYTES
        or limit > MAX_RW_PAGES * PAGE_BYTES
    ):
        raise ValueError("maximum RW bytes must be a positive page-aligned size up to 1 MiB")
    static = int.from_bytes(payload[12:16], "little")
    minimum = PAGE_BYTES + ((static + PAGE_BYTES - 1) // PAGE_BYTES) * PAGE_BYTES + STACK_BYTES
    if limit < minimum:
        raise ValueError("maximum RW bytes cannot contain the boot page, static data and stack")


def validate_rw_bounds(payload: bytes, minimum: int | None, maximum: int | None) -> None:
    validate_rw_limit(payload, maximum)
    validate_rw_limit(payload, minimum)
    if minimum is not None and maximum is not None and minimum > maximum:
        raise ValueError("minimum RW bytes exceeds maximum RW bytes")


def run(*args: str | Path) -> str:
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{args!r}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def memory_report(
    payload: bytes,
    stage_size: int,
    stage_minimum_pages: int,
    models: list[str],
    limit: int | None,
    minimum: int | None = None,
) -> dict:
    """Account for pool pages without claiming to size the native allocation."""
    validate_rw_bounds(payload, minimum, limit)
    if type(stage_size) is not int or stage_size <= 0:
        raise ValueError("invalid linked stage extent")
    if type(stage_minimum_pages) is not int or not 1 < stage_minimum_pages <= MAX_RW_PAGES:
        raise ValueError("invalid linked stage pool minimum")
    image = int.from_bytes(payload[4:8], "little")
    static = int.from_bytes(payload[12:16], "little")
    if not image or len(payload) != 16 + image:
        raise ValueError("HPC1 image size does not match its payload")
    code_pages = (image + PAGE_BYTES - 1) // PAGE_BYTES
    rw_minimum_pages = 1 + (static + PAGE_BYTES - 1) // PAGE_BYTES + STACK_BYTES // PAGE_BYTES
    admitted_rw_pages = max(rw_minimum_pages, (minimum or 0) // PAGE_BYTES)
    # The linked stage minimum already includes the live stage stack page.
    # stage1 then adds a distinct L2 workspace, application code and RW pages.
    fixed_pages = stage_minimum_pages + 1 + code_pages
    return {
        "version": 1,
        "page_bytes": PAGE_BYTES,
        "pool_capacity_pages": MAX_RW_PAGES,
        "application": {
            "image_bytes": image,
            "code_pages": code_pages,
            "static_bytes": static,
            "boot_pages": 1,
            "stack_bytes": STACK_BYTES,
            "minimum_rw_pages": rw_minimum_pages,
            "required_rw_bytes": minimum,
            "rw_ceiling_bytes": limit,
        },
        "launcher": {
            "stage_extent_bytes": stage_size,
            "stage_and_prefix_pages": stage_minimum_pages - 1,
            "stage_stack_pages": 1,
            "mmu_workspace_pages": 1,
        },
        "models": {
            model: {
                "stock_native_workspace_pages": 2 if MODELS[model] <= 3 else 0,
                "minimum_direct_pool_pages": fixed_pages + admitted_rw_pages,
                "minimum_stock_pool_pages": fixed_pages
                + admitted_rw_pages
                + (2 if MODELS[model] <= 3 else 0),
                "direct_pool_pages_for_rw_ceiling": None
                if limit is None
                else fixed_pages + limit // PAGE_BYTES,
                "stock_pool_pages_for_rw_ceiling": None
                if limit is None
                else fixed_pages + limit // PAGE_BYTES + (2 if MODELS[model] <= 3 else 0),
            }
            for model in models
        },
        "excludes": [
            "native launcher allocation policy and allocator overhead",
            "stored launcher/application objects and transfer staging",
        ],
    }


def build(
    root: Path,
    tools: Path,
    payload: Path,
    output: Path,
    models: list[str],
    *,
    wrap: bool = True,
    hp39_armsys_probe: bool = False,
    max_rw_bytes: int | None = None,
    min_rw_bytes: int | None = None,
) -> None:
    validate_rw_bounds(payload.read_bytes(), min_rw_bytes, max_rw_bytes)
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
        "-ffreestanding",
        "-ffunction-sections",
        "-fdata-sections",
        "-Os",
        "-DNDEBUG",
        "-I" + str(root / "include"),
    ]

    def compile_source(source: Path, name: str, *extra: str) -> Path:
        obj = scratch / (name + ".o")
        language = ["-mthumb", "-Oz"] if source.suffix == ".c" else []
        run(cc, *flags, *language, *extra, "-c", source, "-o", obj)
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
        compile_source(
            platform / source,
            name,
            *(
                [
                    f"-DHPCLANG_{name}_RW_BYTES={value}"
                    for name, value in (("MAX", max_rw_bytes), ("MIN", min_rw_bytes))
                    if value is not None
                ]
                if source == "stage1.c"
                else []
            ),
        )
        for source, name in (
            ("stage1-entry.S", "stage1-entry"),
            ("stage1.c", "stage1"),
            ("stage-image.c", "stage-image"),
            ("stage-stack.c", "stage-stack"),
            ("hp39-portal.S", "stage1-hp39-portal"),
        )
    ] + objects
    objects.append(compile_source(root / "third_party/tinf/tinflate.c", "tinflate"))
    objects.append(compile_source(root / "runtime/core/armv4t-eabi-div.S", "stage-div"))
    script = scratch / "stage1.ld"
    run(cc, "-E", "-P", "-x", "c", platform / "stage1.ld", "-o", script)
    stage1_elf = scratch / "stage1.elf"
    builtins = run(
        cc, "--target=arm-none-eabi", "-march=armv4t", "--print-libgcc-file-name"
    ).strip()
    run(ld, "-m", "armelf", "--gc-sections", "-T", script, *objects, builtins, "-o", stage1_elf)
    if run(nm, "-u", stage1_elf).strip():
        raise ValueError("unresolved stage1 symbols")
    symbols = {
        parts[-1]: int(parts[0], 16)
        for line in run(nm, stage1_elf).splitlines()
        if len(parts := line.split()) == 3
    }
    stage_size = symbols.get("__stage1_size")
    minimum_pool_pages = symbols.get("__stage1_min_pool_pages")
    if stage_size is None or minimum_pool_pages is None:
        raise ValueError("missing stage1 memory extent")
    stage1 = scratch / "stage1.bin"
    run(tools / "llvm-objcopy", "-O", "binary", stage1_elf, stage1)
    if stage1.stat().st_size > stage_size:
        raise ValueError("stage1 file exceeds its reserved memory extent")

    def assembly_path(path: Path) -> str:
        return str(path.resolve()).replace("\\", "/").replace('"', '\\"')

    bundle = scratch / "bundle.S"
    original = payload.read_bytes()
    report = memory_report(
        original, stage_size, minimum_pool_pages, models, max_rw_bytes, min_rw_bytes
    )
    early_pool_pages = minimum_pool_pages
    if min_rw_bytes is not None:
        early_pool_pages = next(iter(report["models"].values()))["minimum_direct_pool_pages"]
        if any(
            item["minimum_stock_pool_pages"] > MAX_RW_PAGES for item in report["models"].values()
        ):
            raise ValueError("minimum RW requirement cannot fit the launcher page pool")
    compressor = zlib.compressobj(level=9, wbits=-15)
    compressed = compressor.compress(original[16:]) + compressor.flush()
    packed_payload = scratch / "application.bin"
    packed_payload.write_bytes(
        b"HPCZ" + original[4:16] + compressed if len(compressed) < len(original) - 16 else original
    )
    bundle.write_text(
        f'''\
.section .rodata.hpclang.bundle,"a",%progbits
.align 2
.global __stage1_blob_start, __stage1_blob_end, __stage1_blob_size, __stage1_memory_size
.global __stage1_min_pool_pages
.global __hpc1_blob_start, __hpc1_blob_end, __hpc1_blob_size
.set __stage1_memory_size, {stage_size}
.set __stage1_min_pool_pages, {early_pool_pages}
__stage1_blob_start:
.incbin "{assembly_path(stage1)}"
__stage1_blob_end:
.set __stage1_blob_size, __stage1_blob_end - __stage1_blob_start
.align 2
__hpc1_blob_start:
.incbin "{assembly_path(packed_payload)}"
__hpc1_blob_end:
.set __hpc1_blob_size, __hpc1_blob_end - __hpc1_blob_start
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
    (output / "memory.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, fromfile_prefix_chars="@")
    parser.add_argument("payload", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--tool-bin", type=Path, required=True)
    parser.add_argument("--models", choices=MODELS, nargs="+", default=list(MODELS))
    parser.add_argument("--raw-only", action="store_true")
    parser.add_argument(
        "--max-rw-bytes",
        type=lambda value: int(value, 0),
        help="cap application-visible RW bytes (page aligned); does not reduce the Stock reservation",
    )
    parser.add_argument("--hp39-armsys-probe", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument(
        "--min-rw-bytes",
        type=lambda value: int(value, 0),
        help="require this page-aligned application RW size before launching",
    )
    args = parser.parse_args()
    build(
        Path(__file__).resolve().parents[1],
        args.tool_bin,
        args.payload,
        args.output,
        args.models,
        wrap=not args.raw_only,
        hp39_armsys_probe=args.hp39_armsys_probe,
        max_rw_bytes=args.max_rw_bytes,
        min_rw_bytes=args.min_rw_bytes,
    )


if __name__ == "__main__":
    main()
