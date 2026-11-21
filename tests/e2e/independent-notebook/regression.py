"""Build and run the HP50g no-library Notebook save/relaunch fixture."""

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

no_toolbox = runpy.run_path(str(E2E / "independent-entry/verify.py"))["no_toolbox"]
ui = runpy.run_path(str(E2E / "notebook/verify.py"))


def pack(nibbles):
    nibbles += "0" * (len(nibbles) % 2)
    return b"HPHP49-C" + bytes.fromhex(
        "".join(nibbles[i : i + 2][::-1] for i in range(0, len(nibbles), 2))
    )


def build(dest, toolchain):
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
            "-DHPCLANG_BUILD_MODEL=hp50g",
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
        or rw + 8192 + 4096 > 32768
    ):
        raise ValueError("Notebook no longer fits the qualified fixture layout")
    checksum = 2166136261
    for b in image:
        checksum = ((checksum ^ b) * 16777619) & 0xFFFFFFFF
    parts = []
    for phase in ("acquire", "check"):
        source = dest / (phase + ".c")
        source.write_text(
            f'#define IMAGE_BYTES {size}u\n#define IMAGE_HASH {checksum}u\n#define ENTRY_OFFSET {offset}u\n#include "{(HERE / (phase + ".c")).as_posix()}"\n'
        )
        subprocess.run(
            [
                sys.executable,
                E2E / "independent-entry/build.py",
                "--toolchain",
                toolchain,
                "--stock-state",
                "--stock-frame",
                "--probe-source",
                source,
                "--output",
                dest / phase,
            ],
            check=True,
        )
        digits = "".join(word(b, 2) for b in (dest / phase / "program.HP").read_bytes()[8:])
        parts.append(digits[5 : 10 + int(digits[10:15][::-1], 16)])
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
    gc = (
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
    (dest / "RUN.HP").write_bytes(
        pack(
            word(0x2D9D)
            + parts[0]
            + gc
            + parts[1]
            + word(0x2614)
            + word(12)
            + "5724210"
            + word(0x312B)
        )
    )
    (dest / "COPY.HP").write_bytes(pack(word(0x2D9D) + word(0x6657) + word(0x312B)))
    return image


def check_phase(phase, first, flash, original, font):
    ram = (phase / "state/sram").read_bytes()
    assert (phase / "state/flash").read_bytes() == flash
    _, read, _ = no_toolbox(ram, flash)
    assert bytes(home_string(read, b"HPCNOTE")) == b"hpclang-notebook/1\n20"
    assert original == ram[0x20000:0x35000]
    stack = read(0x806F8)
    result = read(stack)
    assert (
        read(result) == 0x2614 and sum(read(result + 10 + i, 1) * 10**i for i in range(6)) == 124275
    )
    moved = read(stack + 5)
    assert moved != read(read(0x8078E) + 100) and read(moved) == 0x2A4E and read(moved + 5) == 37
    assert [read(moved + 10 + i, 1) for i in range(32)] == [int(x, 16) for x in "12345678" * 4]
    require_stock_pc(phase / "state")
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
    ui["check_line"](pixels, font, 24, "" if first else "20", f, focused=True)
    returned = (phase / "stock-returned.pgm").read_bytes()
    assert returned == (phase / "stock-checkpoint.pgm").read_bytes() and returned != f.read_bytes()
    return hashlib.sha256(returned).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, help="optional local no-275 HP50g Stock snapshot")
    args = parser.parse_args()
    dest = args.output.resolve()
    dest.mkdir(parents=True)
    image = build(dest, args.toolchain.resolve())
    profile = json.loads((E2E / "targets/hp50g.json").read_text())
    spec = SimpleNamespace(
        model="hp50g", profile=profile, dist=args.dist.resolve(), flash_busy_reads=0
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
    if (
        hashlib.sha256(flash[:0x140000]).hexdigest()
        != runpy.run_path(str(E2E / "independent-entry/verify.py"))["ROM_SHA256"]
    ):
        raise ValueError("unexpected HP50g ROM")
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
    for first, name, source in [(True, "save", "setup"), (False, "reload", "save")]:
        keys = (
            "4300 tap two\n4900 tap zero\n5500 tap down\n6200 tap enter\n7600 capture saved\n"
            if first
            else ""
        )
        transfer(
            spec,
            dest / name,
            dest / source / "state",
            dest,
            "SPLITGC",
            installed=not first,
            package_name="RUN.HP",
            extra_files=[("COPY.HP", "CKCOPY")] if first else [],
            remote_commands=["HOME 'SPLITGC' RCL CKCOPY"],
            actions="3500 capture editor\n" + keys + "8500 tap on\n15000 capture returned\n",
            execution_ms=18000,
        )
        hashes[name] = {
            "editor": "" if first else "20",
            "home_hex": b"hpclang-notebook/1\n20".hex(),
            "settled_sha256": check_phase(dest / name, first, flash, staged[0x20000:0x35000], font),
        }
    (dest / "report.json").write_text(json.dumps(hashes, indent=2))
    print("PASS independent HP50g Notebook HOME bytes, editor reload, GC and settled return")


if __name__ == "__main__":
    main()
