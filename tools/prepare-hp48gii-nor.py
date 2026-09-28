#!/usr/bin/env python3
"""Prepare a full HP48gII NOR dump as an x50ng persistent flash state."""

from __future__ import annotations

import argparse
from pathlib import Path

FLASH_SIZE = 0x200000
BOOT_SIZE = 0x4000
SERIAL_OFFSET = 0x3FF0


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=Path)
    parser.add_argument("state_dir", type=Path)
    args = parser.parse_args()

    data = args.raw.read_bytes()
    if len(data) != FLASH_SIZE:
        raise SystemExit(f"{args.raw}: expected a 2 MiB full NOR dump, got {len(data)} bytes")
    if data[:BOOT_SIZE] == b"\xff" * BOOT_SIZE:
        raise SystemExit(f"{args.raw}: bootloader area is blank")

    args.state_dir.mkdir(parents=True, exist_ok=True)
    (args.state_dir / "flash").write_bytes(data)

    serial = data[SERIAL_OFFSET : SERIAL_OFFSET + 16].split(b"\0", 1)[0]
    marker = data[BOOT_SIZE : BOOT_SIZE + 16]
    print(f"flash={args.state_dir / 'flash'}")
    print(f"serial={serial.decode('ascii', errors='replace')}")
    print(f"firmware_marker={marker.decode('ascii', errors='replace')}")


if __name__ == "__main__":
    main()
