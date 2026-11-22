"""Build the model-specific HPC1 experiment without library 275 or ROM inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess

# Repo and installed builders both live three directories below the SDK root.
_script = Path(__file__).resolve()
FIXTURE = _script.parent.relative_to(_script.parents[3])
ENTRY = FIXTURE.parent / "independent-entry"
INPUTS = (
    *((FIXTURE / name).as_posix() for name in ("build.py", "app.c", "loader.c", "link.ld")),
    *((ENTRY / name).as_posix() for name in ("build.py", "entry.S")),
    "include/hpclang/abi.h",
    "include/hpclang/image-capabilities.h",
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


def build(source, tool, output, model="hp50g", notebook=None, port0=False, preserve_stack=False):
    if preserve_stack and not notebook:
        raise ValueError("stack-preserving return requires the HP48gII Notebook")
    model_id = {"hp48gii": 4, "hp49gplus": 5, "hp50g": 6}[model]
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

    compile_file(source / "runtime/bootstrap/sync.S", "sync.o")
    if port0:
        if not notebook or model != "hp48gii":
            raise ValueError("Port 0 execution requires the HP48gII Notebook")
        common.append("-DHPCLANG_NOTEBOOK_PORT0")
    # The independent Notebook maps 16 code pages and one MMU page.
    # Keep the broader 29-page HP48gII reservation for the general pool probe.
    pool_pages = 29 if model == "hp48gii" else 6
    if notebook:
        application = notebook.read_bytes()
        if model != "hp48gii" or len(application) < 16:
            raise ValueError("expected an HP48gII Notebook image")
        magic, size, entry, rw = struct.unpack_from("<4sIII", application)
        if (
            magic != b"HPC1"
            or size != len(application) - 16
            or not 0 < entry < size <= 65536
            or rw + 8192 + 4096 > 49152
        ):
            raise ValueError("Notebook exceeds the HP48gII layout or has an invalid HPC1 header")
        # Budget 12 KiB for the editor's measured 9,924-byte heap, plus its
        # stack and boot page. Keep the previous 48 KiB ceiling for large images.
        rw_pages = min(12, max(8, (rw + 8192 + 4096 + 12288 + 4095) // 4096))
        pool_pages = 1 + 16 + rw_pages
        common.append(f"-DNOTEBOOK_RW_PAGES={rw_pages}")
        (output / "app.hpc").write_bytes(application)
        common += ["-DHPCLANG_NOTEBOOK", f"-DNOTEBOOK_BYTES={size}", f"-DNOTEBOOK_ENTRY={entry}"]
    else:
        compile_file(source / FIXTURE / "app.c", "app.o", True)
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

    assembly = (source / ENTRY / "entry.S").read_text()
    replacements = {
        " bl exercise": " ldr r2, arena_offset\n add r1, r6, r2\n ldr r2, image_offset\n add r2, r6, r2\n ldr r3, image_bytes\n bl exercise",
        "workspace_offset: .word workspace-entry": "workspace_offset: .word workspace-entry\narena_offset: .word arena-entry\nimage_offset: .word hpc-entry\nimage_bytes: .word hpc_end-hpc",
    }
    if notebook:
        replacements[" bl exercise"] = (
            " ldr r2, arena_offset\n add r1, r6, r2\n ldr r2, [r4, #4]\n bl exercise"
        )
        replacements["workspace_offset: .word workspace-entry"] = (
            "workspace_offset: .word workspace-entry\narena_offset: .word arena-entry"
        )
    if port0:
        # Use the original ARM stack after the last workspace write. All record
        # bytes must be stable while the CRC is calculated, including stack data.
        replacements[" bl exercise"] = replacements[" bl exercise"].replace(
            " bl exercise", " mov r3, r4\n bl exercise"
        )
        replacements[" mov sp, r4"] = (
            " bic sp, r4, #7\n ldr r0, [r4, #4]\n mov r1, r6\n bl finish_port0_backup\n mov sp, r4"
        )
    for old, new in replacements.items():
        if assembly.count(old) != 1:
            raise ValueError("shared entry no longer has the expected call/offset structure")
        assembly = assembly.replace(old, new)
    arena_bytes = (pool_pages + 1) * 4096
    arena = (
        f".balign 8\n.global arena\narena: .space {arena_bytes}\n.global arena_end\narena_end:\n"
    )
    image = '.balign 4\n.global hpc\nhpc: .incbin "app.hpc"\nhpc_end:\n'
    if model == "hp48gii":
        # Only initialized bytes need a transfer copy beside the program-owned destination.
        assembly += "\n" + ("" if notebook else image) + '.section .bss,"aw",%nobits\n' + arena
    else:
        assembly += "\n" + arena + image
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
    encoder = runpy.run_path(str(source / ENTRY / "build.py"))
    word, code_for = encoder["word"], encoder["code_for"]
    code = code_for(len(blob), len(code_for(len(blob), 0, model)) - 13, model)
    packed = "".join(f"{b & 15:x}{b >> 4:x}" for b in blob)
    layout = symbols("probe.elf")
    reserve = 2 * max(len(blob), layout["arena_end"]) + 7
    destination = "0" * reserve
    if notebook:
        # The header stays beyond the writable arena. Its backwards distance to the
        # code is in Saturn nibbles, so a moved HOME object can execute again.
        pos = 2 * layout["arena"] + 64
        payload = "".join(f"{b & 15:x}{b >> 4:x}" for b in application[16:])
        trailer = application[:16] + struct.pack("<I", reserve - pos)
        destination = destination[:pos] + payload + destination[pos + len(payload) :]
        destination += "".join(f"{b & 15:x}{b >> 4:x}" for b in trailer)
        reserve = len(destination)
    body = code + packed + destination
    program = word(0x2D9D) + word(0x2DCC) + word(5 + len(body)) + body
    if not preserve_stack:
        if notebook:
            program += encoder["moving_gc_program"]()
        program += word(0x2614) + word(12) + "5724210"
    program += word(0x312B)
    if len(program) & 1:
        program += "0"
    (output / "program.HP").write_bytes(
        b"HPHP49-C"
        + bytes(
            int(program[i], 16) | int(program[i + 1], 16) << 4 for i in range(0, len(program), 2)
        )
    )
    metadata = dict(
        symbols=layout,
        size=len(blob),
        prefix=15 + len(code) + len(packed),
        reserve=reserve,
        blob=blob.hex(),
    )
    (output / "metadata.json").write_text(json.dumps(metadata) + "\n")
    if any(digest(source / name) != expected for name, expected in inputs.items()):
        raise ValueError("build inputs changed")
    manifest = dict(
        scope=f"{model} bounded embedded HPC1 experiment; no native-service context",
        model=model,
        inputs=inputs,
        outputs={
            name: digest(output / name) for name in OUTPUTS if not notebook or name != "app.elf"
        },
        compiler=subprocess.check_output([str(tool / "clang"), "--version"], text=True),
    )
    if notebook:
        manifest["application"] = "notebook"
        manifest["scope"] = "HP48gII resident Notebook editor and Stock return"
        manifest["resident_image"] = True
        manifest["post_exit_gc"] = not preserve_stack
        if preserve_stack:
            manifest["preserve_stack"] = True
    if port0:
        manifest["port0_backup"] = True
    if model == "hp48gii":
        manifest["pool_pages"] = pool_pages
    (output / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Built {len(blob)}-byte payload in {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--model", choices=("hp48gii", "hp49gplus", "hp50g"), default="hp50g")
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--notebook", type=Path, help="installed-SDK HP48gII notebook.hpc")
    parser.add_argument("--port0", action="store_true", help="run in the ENTRY48 Port 0 backup")
    parser.add_argument(
        "--preserve-stack", action="store_true", help="return without diagnostic stack objects"
    )
    args = parser.parse_args()
    build(
        args.source_root.resolve(),
        args.toolchain.resolve(),
        args.output.resolve(),
        args.model,
        args.notebook,
        args.port0,
        args.preserve_stack,
    )


if __name__ == "__main__":
    main()
