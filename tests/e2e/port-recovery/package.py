"""Build the independent recovery witness after checking its linked PIC image."""

import argparse
import importlib.util
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "package_recovery", ROOT / "tools/package-recovery.py"
)
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)

# This fixture uses PC-relative calls/data and local-exec TLS. LLD may retain
# the relaxed TLS_IE32 relocation in --emit-relocs output. Absolute/GOT address
# relocations and any newly introduced relocation kind require investigation.
PIC_RELOCATIONS = {
    "R_ARM_ALU_PC_G0",
    "R_ARM_CALL",
    "R_ARM_JUMP24",
    "R_ARM_REL32",
    "R_ARM_THM_CALL",
    "R_ARM_TLS_IE32",
    "R_ARM_TLS_LE32",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for tool in ("nm", "readelf", "objcopy"):
        parser.add_argument("--" + tool, required=True)
    parser.add_argument("elf", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    listing = subprocess.check_output([args.readelf, "-r", args.elf], text=True)
    relocations = {word for word in listing.split() if word.startswith("R_ARM_")}
    if not relocations or relocations - PIC_RELOCATIONS:
        raise ValueError(
            f"Unverified recovery relocations: {sorted(relocations - PIC_RELOCATIONS)}"
        )
    symbols = {}
    for line in subprocess.check_output([args.nm, args.elf], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    if symbols["entry"] != 0:
        raise ValueError("Recovery ARM entry must be at image offset zero")
    binary = args.output.with_suffix(".bin")
    subprocess.run(
        [args.objcopy, "-O", "binary", "-j", ".text", "-j", ".tdata", args.elf, binary], check=True
    )
    initialized = binary.read_bytes()
    zero, end = symbols["__zero_start"], symbols["__image_end"]
    if not len(initialized) <= zero <= end or zero % 4 or end % 8:
        raise ValueError(
            "Recovery BSS clearing must preserve initialized data and stay in the image"
        )
    args.output.write_bytes(PACK.package(initialized, end, model="hp50g"))


if __name__ == "__main__":
    main()
