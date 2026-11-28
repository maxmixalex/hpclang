"""Build the independent HP49g+/HP50g Notebook transfer files."""

import argparse
from pathlib import Path
import runpy
import struct
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
E2E = HERE.parent
word = runpy.run_path(str(E2E / "independent-entry/build.py"))["word"]
MODELS = {"hp49gplus": 5, "hp50g": 6}


def pack(nibbles):
    nibbles += "0" * (len(nibbles) % 2)
    return b"HPHP49-C" + bytes.fromhex(
        "".join(nibbles[i : i + 2][::-1] for i in range(0, len(nibbles), 2))
    )


def read_image(hpc):
    if len(hpc) < 16:
        raise ValueError("Notebook HPC1 header is truncated")
    image = hpc[16:]
    magic, size, offset, rw = struct.unpack("<4sIII", hpc[:16])
    if (
        magic != b"HPC1"
        or size != len(image)
        or not 0 <= offset < size
        or (size + 4095) // 4096 != 20
        or rw + 8192 + 4096 > 20480
    ):
        raise ValueError("Notebook no longer fits the qualified fixture layout")
    checksum = 2166136261
    for b in image:
        checksum = ((checksum ^ b) * 16777619) & 0xFFFFFFFF
    return image, checksum, offset


def build_phase(
    out,
    toolchain,
    model,
    size,
    checksum,
    offset,
    rw_bytes,
    expected_status,
    phase,
    user_return=False,
    enter_after_gc=False,
):
    source = out / (phase + ".c")
    source.write_text(
        ("#define NOTEBOOK_USER_RETURN\n" if user_return else "")
        + f"#define NOTEBOOK_MAPPED_GC {int(not user_return and expected_status == 0)}\n"
        + f"#define NOTEBOOK_ENTER_AFTER_GC {int(enter_after_gc)}\n"
        + f"#define NOTEBOOK_MODEL {MODELS[model]}u\n"
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
            model,
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
    return digits[5 : 10 + int(digits[10:15][::-1], 16)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", choices=MODELS, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--notebook", type=Path, required=True)
    parser.add_argument("--port1-offset", type=int, default=0)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    at = args.port1_offset
    # Stock addresses Port records in nibbles. The NBLSE backup and string
    # headers precede the payload. Its trailer and the next terminator follow.
    payload_start = at + 34
    page = ((payload_start + 8191) // 8192) * 8192
    image_capacity = 20 * 4096
    if at < 0 or at % 2 or page + 2 * image_capacity + 10 + 6 > 0x40000:
        parser.error("Port 1 needs an even append offset in nibbles and space for 20 aligned pages")
    image, checksum, entry = read_image(args.notebook.read_bytes())
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=out) as tmp:
        code = build_phase(
            Path(tmp),
            args.toolchain,
            args.model,
            len(image),
            checksum,
            entry,
            32768,
            0,
            "acquire",
            user_return=True,
        )
    (out / "USER.HP").write_bytes(pack(word(0x2D9D) + code + word(0x312B)))
    (out / "COPY.HP").write_bytes(pack(word(0x2D9D) + word(0x6657) + word(0x312B)))
    payload = bytes((page - payload_start) // 2) + image + bytes(image_capacity - len(image))
    (out / "DATA.HP").write_bytes(
        pack(word(0x2A2C) + word(5 + 2 * len(payload)) + "".join(word(b, 2) for b in payload))
    )


if __name__ == "__main__":
    main()
