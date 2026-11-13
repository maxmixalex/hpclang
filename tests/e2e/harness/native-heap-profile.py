"""Emit a fingerprint-checked native allocator observation profile for a ROM."""

import argparse
import json
from pathlib import Path
import runpy


def rom_image(data, update_image=False):
    if not update_image:
        return data
    # x50ng loads a Kinpo update image at NOR offset 0x4000, including
    # its 16-byte header. Allocator PCs must use those mapped coordinates.
    headers = (
        b"KINPOUPDATEIMAGE",
        b"KINPOHP39G+IMAGE",
        b"KINPOHP40G+IMAGE",
        b"KINPOHP48GIIMAGE",
    )
    if data[:16] not in headers or not 16 <= len(data) <= 0x1FC000:
        raise ValueError("invalid Kinpo update image")
    return bytes(0x4000) + data


def main():
    verifier = runpy.run_path(str(Path(__file__).with_name("verify-resource-snapshots.py")))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--model", choices=verifier["MODELS"], required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--update-image", action="store_true")
    parser.add_argument(
        "--shell-env",
        action="store_true",
        help="write only numeric observer environment exports",
    )
    args = parser.parse_args()
    profile = verifier["allocator_profile"](
        rom_image(args.rom.read_bytes(), args.update_image),
        verifier["MODELS"][args.model],
    )
    with args.output.open("x", encoding="utf-8") as output:
        if args.shell_env:
            for name, key in (
                ("MODEL", "model"),
                ("PC", "return_pc"),
                ("ROOTS", "free_list_roots"),
            ):
                output.write(f"export HPCLANG_X50NG_NATIVE_HEAP_{name}={int(profile[key])}\n")
        else:
            json.dump(profile, output, indent=2)
            output.write("\n")


if __name__ == "__main__":
    main()
