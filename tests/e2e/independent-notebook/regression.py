"""Build and run the HP49g+/HP50g no-library Notebook save/relaunch fixture."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import struct
import subprocess
import sys
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
E2E = HERE.parent
ROOT = E2E.parents[1]
entry = runpy.run_path(str(E2E / "independent-entry/build.py"))
word = entry["word"]
sys.path.insert(0, str(E2E / "port-recovery"))
from run import emulator, completed, transfer  # noqa: E402
from verify import home_string, require_stock_pc  # noqa: E402

entry_verifier = runpy.run_path(str(E2E / "independent-entry/verify.py"))
no_toolbox = entry_verifier["no_toolbox"]
ui = runpy.run_path(str(E2E / "notebook/verify.py"))

# The low-memory run starts with the normal result already on the Stock stack.
# A distinct RPL result proves that the new program reached its cleanup check.
NORMAL_RESULT = 124275
LOW_MEMORY_RESULT = 124276
REFUSED_RESULT = 124277


def pack(nibbles):
    nibbles += "0" * (len(nibbles) % 2)
    return b"HPHP49-C" + bytes.fromhex(
        "".join(nibbles[i : i + 2][::-1] for i in range(0, len(nibbles), 2))
    )


def build(dest, toolchain, profile):
    env = dict(os.environ, PATH=str(toolchain) + os.pathsep + os.environ["PATH"])
    sdk = dest / "sdk"
    for cmd in [
        ["cmake", "-S", ROOT, "-B", dest / "install", "-DCMAKE_INSTALL_PREFIX=" + str(sdk)],
        ["cmake", "--install", dest / "install"],
        [
            "cmake",
            "-S",
            sdk / "share/hpclang/examples/notebook",
            "-B",
            dest / "app",
            "-DCMAKE_BUILD_TYPE=MinSizeRel",
            "-DCMAKE_PREFIX_PATH=" + str(sdk),
            "-DCMAKE_TOOLCHAIN_FILE="
            + str(sdk / "share/hpclang/cmake/toolchains/hpclang-armv4t.cmake"),
            "-DHPCLANG_BUILD_MODEL=" + profile["target"],
        ],
        ["cmake", "--build", dest / "app", "--target", "notebook_launchers", "-j4"],
    ]:
        subprocess.run(cmd, env=env, check=True)
    hpc = (dest / "app/notebook.hpc").read_bytes()
    image = hpc[16:]
    magic, size, offset, rw = struct.unpack("<4sIII", hpc[:16])
    if (
        magic != b"HPC1"
        or size != len(image)
        or (size + 4095) // 4096 != 20
        or rw + 8192 + 4096 > 20480
    ):
        raise ValueError("Notebook no longer fits the qualified fixture layout")
    checksum = 2166136261
    for b in image:
        checksum = ((checksum ^ b) * 16777619) & 0xFFFFFFFF
    for package, rw_bytes, expected_status in (
        ("RUN.HP", 32768, 0),
        ("LOWMEM.HP", 20480, 1),
        ("REFUSE.HP", 32768, None),
    ):
        out = dest / package.removesuffix(".HP").lower()
        out.mkdir()
        build_runner(
            out,
            dest / package,
            toolchain,
            profile,
            size,
            checksum,
            offset,
            rw_bytes,
            expected_status,
        )
    (dest / "COPY.HP").write_bytes(pack(word(0x2D9D) + word(0x6657) + word(0x312B)))
    return image


def build_runner(
    out, package, toolchain, profile, size, checksum, offset, rw_bytes, expected_status
):
    parts = []
    for phase in ("acquire", "check"):
        source = out / (phase + ".c")
        source.write_text(
            f"#define NOTEBOOK_MODEL {profile['model']}u\n"
            f"#define NOTEBOOK_RW_BYTES {rw_bytes}u\n"
            f"#define NOTEBOOK_EXPECTED_STATUS {expected_status or 0}u\n"
            f"#define REQUEST_PAGES {35 if expected_status is None else 'POOL_PAGES'}\n"
            f"#define IMAGE_BYTES {size}u\n"
            f"#define IMAGE_HASH {checksum}u\n"
            f"#define ENTRY_OFFSET {offset}u\n"
            f'#include "{(HERE / (phase + ".c")).as_posix()}"\n'
        )
        subprocess.run(
            [
                sys.executable,
                E2E / "independent-entry/build.py",
                "--model",
                profile["target"],
                "--toolchain",
                toolchain,
                "--stock-state",
                "--stock-frame",
                "--probe-source",
                source,
                "--output",
                out / phase,
            ],
            check=True,
        )
        digits = "".join(word(b, 2) for b in (out / phase / "program.HP").read_bytes()[8:])
        parts.append(digits[5 : 10 + int(digits[10:15][::-1], 16)])
    gc = entry["moving_gc_program"]()
    package.write_bytes(
        pack(
            word(0x2D9D)
            + parts[0]
            + gc
            + word(0x2614)
            + word(12)
            + str(REFUSED_RESULT)[::-1]
            + "0"
            + word(0x6657)  # NEWOB: private writable copy of the refusal result
            + parts[1]
            + word(0x312B)
        )
    )


def check_phase(phase, editor_text, flash, original, font, expected_result):
    ram = (phase / "state/sram").read_bytes()
    assert (phase / "state/flash").read_bytes() == flash
    _, read, _ = no_toolbox(ram, flash)
    assert bytes(home_string(read, b"HPCNOTE")) == b"hpclang-notebook/1\n20"
    assert original == ram[0x20000:0x35000]
    stack = read(0x806F8)
    result = read(stack)
    digits = sum(read(result + 10 + i, 1) * 10**i for i in range(6))
    assert read(result) == 0x2614 and read(result + 5) == 12 and digits == expected_result
    entry_verifier["moving_gc_result"](read, stack + 5)
    require_stock_pc(phase / "state")
    returned = (phase / "stock-returned.pgm").read_bytes()
    assert returned == (phase / "stock-checkpoint.pgm").read_bytes()
    if editor_text is None:
        return hashlib.sha256(returned).hexdigest()
    f = phase / "stock-editor.pgm"
    magic, dims, maximum, img = f.read_bytes().split(b"\n", 3)
    width, height = map(int, dims.split())
    assert (
        magic == b"P5"
        and maximum == b"255"
        and width in (131, 132)
        and height == 80
        and len(img) == width * height
    )
    pixels = b"".join(img[i * width : i * width + 131] for i in range(height))
    ui["check_line"](pixels, font, 0, "Notebook", f)
    ui["check_line"](pixels, font, 24, editor_text, f, focused=True)
    assert returned != f.read_bytes()
    return hashlib.sha256(returned).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--model", choices=("hp49gplus", "hp50g"), default="hp50g")
    parser.add_argument(
        "--baseline", type=Path, help="optional no-275 Stock snapshot for the selected model"
    )
    args = parser.parse_args()
    dest = args.output.resolve()
    dest.mkdir(parents=True)
    profile = json.loads((E2E / "targets" / f"{args.model}.json").read_text())
    image = build(dest, args.toolchain.resolve(), profile)
    spec = SimpleNamespace(
        model=args.model, profile=profile, dist=args.dist.resolve(), flash_busy_reads=0
    )
    baseline = args.baseline
    if baseline is None:
        fw = spec.dist / "firmware"
        with emulator(
            spec,
            dest / "clean",
            "2000 tap f6\n4000 tap f6\n7000 capture home\n9000 finish-install\n",
            [
                "--bootloader=" + str(fw / profile["bootloader"]),
                "--firmware=" + str(fw / "hp4950v215/2MB_FIX/2MB_215f.bin"),
                "--flash",
            ],
        ) as child:
            completed(child, 90)
        baseline = dest / "clean/state"
    ram = (baseline / "sram").read_bytes()
    flash = (baseline / "flash").read_bytes()
    _, read, _ = no_toolbox(ram, flash)
    bootloader = (spec.dist / "firmware" / profile["bootloader"]).read_bytes()
    expected_rom = entry_verifier["ROM_SHA256_BY_MODEL"][args.model]
    if (
        not flash.startswith(bootloader)
        or hashlib.sha256(flash[:0x140000]).hexdigest() != expected_rom
    ):
        raise ValueError(f"unexpected {args.model} ROM or bootloader")
    try:
        home_string(read, b"HPCNOTE")
    except ValueError as exc:
        if str(exc) != "HOME fixture variable is missing":
            raise
    else:
        raise ValueError("baseline contains HPCNOTE")

    # Stock appends NBLSE after the existing ERAM chain. Derive the
    # string payload alignment; reject an incompatible assumed mapping.
    def field(at, n):
        return sum(
            ((ram[0x20000 + (at + i) // 2] >> (4 * ((at + i) % 2))) & 15) << (4 * i)
            for i in range(n)
        )

    at = 0
    while field(at, 5):
        assert field(at, 5) in (0x2B62, 0x2B40)
        size = field(at + 5, 5) + 5
        assert 20 < size <= 0x40000 - at
        at += size
    offset = at + 34
    assert offset % 2 == 0 and offset // 2 < 4096
    payload = bytes(4096 - offset // 2) + image + bytes(20 * 4096 - len(image))
    (dest / "DATA.HP").write_bytes(
        pack(word(0x2A2C) + word(5 + 2 * len(payload)) + "".join(word(b, 2) for b in payload))
    )
    transfer(
        spec,
        dest / "setup",
        baseline,
        dest,
        "LEASRC",
        package_name="DATA.HP",
        execute=False,
        remote_commands=["-95 CF HOME LEASRC :1:NBLSE STO", "'LEASRC' PURGE"],
    )
    staged = (dest / "setup/state/sram").read_bytes()
    assert staged[0x21000 : 0x21000 + len(image)] == image
    font = ui["font_rows"](ROOT / "runtime/fonts/generated/newrpl_6a.S")
    hashes = {}
    card = dest / "notebook.img"
    media = runpy.run_path(str(ROOT / "tools/e2e-stock-media.py"))
    files = {
        "initial_files": {"KEEP.TXT": "Retained card content\n"},
        "expected_files": {"HPCNOTE.TXT": "202\n"},
    }
    media["prepare"](files, card)
    source = "setup"
    save_keys = "4300 tap two\n4900 tap zero\n5500 tap down\n6200 tap enter\n7600 capture saved\n"
    for name, program, package, editor in (
        ("save", "SPLITGC", "RUN.HP", ""),
        ("refused", "NBFULL", "REFUSE.HP", None),
        ("low-memory", "SPLITERR", "LOWMEM.HP", None),
        ("reload", "SPLITGC", None, "20"),
    ):
        actions = ""
        if editor is not None:
            actions = (
                "3500 capture editor\n" + (save_keys if name == "save" else "") + "8500 tap on\n"
            )
        if name == "reload":
            # Move past Save, Reload and Clock to export the edit, then back it up to Port 0.
            actions = (
                "3500 capture editor\n4300 tap two\n"
                + "".join(f"{5000 + 700 * i} tap down\n" for i in range(4))
                + "8000 tap enter\n9200 tap down\n10100 tap enter\n12300 tap on\n"
            )
        transfer(
            spec,
            dest / name,
            dest / source / "state",
            dest,
            program,
            installed=package is None,
            package_name=package,
            extra_files=[("COPY.HP", "CKCOPY")] if name == "save" else [],
            remote_commands=[
                f"HOME '{program}' RCL CKCOPY" + (" 'NBFULL' PURGE" if name == "refused" else "")
            ],
            actions=actions + "15000 capture returned\n",
            execution_ms=18000,
            sd_image=card if name == "reload" else None,
        )
        hashes[name] = {
            "editor": editor,
            "home_hex": b"hpclang-notebook/1\n20".hex(),
            "settled_sha256": check_phase(
                dest / name,
                editor,
                flash,
                staged[0x20000:0x35000],
                font,
                {"refused": REFUSED_RESULT, "low-memory": LOW_MEMORY_RESULT}.get(
                    name, NORMAL_RESULT
                ),
            ),
        }
        source = name
    media["verify"](files, card)
    hashes["reload"]["export"] = files["expected_files"]
    hashes["refused"].update(requested_pages=35, rpl_result=REFUSED_RESULT)
    hashes["low-memory"].update(exit_status=1, rw_bytes=20480)
    recall = dest / "port0-recall"
    transfer(
        spec,
        recall,
        dest / "reload/state",
        dest,
        "SPLITGC",
        installed=True,
        execute=False,
        remote_commands=["HOME :0:HPCNOTE RCL 'NBREAD' STO"],
        execution_ms=4000,
    )
    assert (recall / "state/flash").read_bytes() == flash
    ram = (recall / "state/sram").read_bytes()
    _, read, _ = no_toolbox(ram, flash)
    saved = home_string(read, b"HPCNOTE")
    backup = home_string(read, b"NBREAD")
    assert saved == b"hpclang-notebook/1\n20" and backup == b"hpclang-notebook/1\n202"
    assert ram[0x20000:0x35000] == staged[0x20000:0x35000]
    require_stock_pc(recall / "state")
    hashes["port0-recall"] = {"home_hex": saved.hex(), "backup_hex": backup.hex()}
    (dest / "report.json").write_text(json.dumps(hashes, indent=2))
    print(
        f"PASS independent {args.model} Notebook HOME bytes, editor reload, GC and settled return"
    )


if __name__ == "__main__":
    main()
