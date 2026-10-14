#!/usr/bin/env python3
"""Prepare and independently verify a fixture's raw 16 MiB FAT16 card."""

import argparse
import json
from pathlib import Path
import re
import subprocess


def manifest(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) != {"version", "initial_files", "expected_files"}:
        raise ValueError("Expected version, initial_files and expected_files")
    if type(data["version"]) is not int or data["version"] != 1:
        raise ValueError("Unsupported media manifest version")
    for field in ("initial_files", "expected_files"):
        if not isinstance(data[field], dict):
            raise ValueError(f"{field} must map FAT 8.3 filenames to UTF-8 text")
        for name, text in data[field].items():
            if not re.fullmatch(r"[A-Z0-9_]{1,8}(?:\.[A-Z0-9_]{1,3})?", name):
                raise ValueError(f"Invalid fixture filename: {name}")
            if not isinstance(text, str) or len(text.encode("utf-8")) > 4096:
                raise ValueError(f"Invalid fixture text: {name}")
    if not data["expected_files"]:
        raise ValueError("Media witness must declare an expected output")
    return data


def prepare(data, image):
    with image.open("wb") as output:
        output.truncate(16 * 1024 * 1024)
    subprocess.run(["mkfs.fat", "-F", "16", str(image)], check=True, capture_output=True)
    for name, text in data["initial_files"].items():
        subprocess.run(
            ["mcopy", "-i", str(image), "-", f"::/{name}"],
            input=text.encode("utf-8"),
            check=True,
            capture_output=True,
        )


def verify(data, image):
    for name, text in (data["initial_files"] | data["expected_files"]).items():
        actual = subprocess.check_output(["mtype", "-i", str(image), f"::/{name}"])
        if actual != text.encode("utf-8"):
            raise ValueError(f"{name}: raw-card content differs from fixture expectation")
    print("PASS: raw FAT card output and retained fixture files")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("prepare", "verify"))
    parser.add_argument("manifest", type=Path)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    data = manifest(args.manifest)
    (prepare if args.operation == "prepare" else verify)(data, args.image)


if __name__ == "__main__":
    main()
