#!/usr/bin/env python3
"""Compute the content key for a generated HP39/40 installed-state fixture."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

FILES = (
    "HP39DIR.CUR",
    "HP39DIR.000",
    "HPGCC000.000",
    "LIB275L.000",
)
BUILDER_VERSION = b"hpclang-hp39-fixture-v1\0"


def add_file(digest: "hashlib._Hash", label: str, path: Path) -> None:
    data = path.read_bytes()
    digest.update(label.encode("utf-8"))
    digest.update(b"\0")
    digest.update(len(data).to_bytes(8, "little"))
    digest.update(data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("target")
    parser.add_argument("firmware", type=Path)
    parser.add_argument("transfer_dir", type=Path)
    args = parser.parse_args()

    digest = hashlib.sha256()
    digest.update(BUILDER_VERSION)
    digest.update(args.target.encode("ascii"))
    digest.update(b"\0")
    add_file(digest, "firmware", args.firmware)
    for name in FILES:
        add_file(digest, name, args.transfer_dir / name)

    print(digest.hexdigest())


if __name__ == "__main__":
    main()
