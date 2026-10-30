#!/usr/bin/env python3
"""Reject a Stock HP39 Aplet containing a different application's launcher."""

import argparse
import hashlib
from pathlib import Path


def verify(launcher: bytes, aplet: bytes) -> None:
    """Check application identity, not the complete historical Aplet format."""
    if not launcher.startswith(b"HPHP49-C") or len(launcher) <= 8:
        raise ValueError("expected a nonempty wrapped ARMToolbox launcher")
    if not aplet.startswith(b"HP39BinB"):
        raise ValueError("expected a binary HP39 Aplet")
    if aplet.count(launcher[8:]) != 1:
        raise ValueError("Stock Aplet does not contain exactly one matching application launcher")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launcher", type=Path)
    parser.add_argument("aplet", type=Path)
    args = parser.parse_args()
    launcher, aplet = args.launcher.read_bytes(), args.aplet.read_bytes()
    try:
        verify(launcher, aplet)
    except ValueError as error:
        parser.exit(1, f"Stock Aplet identity failure: {error}\n")
    print(
        "Stock Aplet identity verified: "
        f"launcher={hashlib.sha256(launcher).hexdigest()} "
        f"aplet={hashlib.sha256(aplet).hexdigest()}"
    )


if __name__ == "__main__":
    main()
