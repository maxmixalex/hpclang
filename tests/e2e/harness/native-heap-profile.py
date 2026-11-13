"""Emit a fingerprint-checked native allocator observation profile for a ROM."""

import argparse
import json
from pathlib import Path
import runpy


def main():
    verifier = runpy.run_path(str(Path(__file__).with_name("verify-resource-snapshots.py")))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--model", choices=verifier["MODELS"], required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    profile = verifier["allocator_profile"](args.rom.read_bytes(), verifier["MODELS"][args.model])
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(profile, output, indent=2)
        output.write("\n")


if __name__ == "__main__":
    main()
