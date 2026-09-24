#!/usr/bin/env python3
"""Expose one emulator E2E target profile through GitHub Actions env."""

import json
import sys
from pathlib import Path


COMMON_KEYS = ("target", "family", "model", "lcd_height")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} TARGET.json", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    data = json.loads(path.read_text(encoding="utf-8"))

    missing = [key for key in COMMON_KEYS if key not in data]
    if missing:
        missing_text = ", ".join(missing)
        print(f"{path}: missing keys: {missing_text}", file=sys.stderr)
        return 2

    if data["target"] != path.stem:
        print(
            f"{path}: target {data['target']!r} does not match filename",
            file=sys.stderr,
        )
        return 2

    for key in COMMON_KEYS:
        value = str(data[key])
        if "\n" in value or "\r" in value:
            print(f"{path}: newline in {key}", file=sys.stderr)
            return 2
        print(f"HPCLANG_E2E_{key.upper()}={value}")

    stock_transport = data.get("stock_transport")
    if stock_transport is not None:
        value = str(stock_transport)
        if value not in ("uart", "usb", "sd"):
            print(f"{path}: unsupported stock_transport {value!r}", file=sys.stderr)
            return 2
        print(f"HPCLANG_E2E_STOCK_TRANSPORT={value}")

    bootloader = data.get("bootloader")
    if bootloader is not None:
        value = str(bootloader)
        if not value:
            print(f"{path}: bootloader must not be empty", file=sys.stderr)
            return 2
        if "\n" in value or "\r" in value:
            print(f"{path}: newline in bootloader", file=sys.stderr)
            return 2
        print(f"HPCLANG_E2E_BOOTLOADER={value}")

    firmware = data.get("firmware")
    if firmware is not None:
        if not isinstance(firmware, dict):
            print(f"{path}: firmware must be an object", file=sys.stderr)
            return 2
        for key in ("archive", "member"):
            value = str(firmware.get(key, ""))
            if not value:
                print(f"{path}: firmware.{key} is required", file=sys.stderr)
                return 2
            if "\n" in value or "\r" in value:
                print(f"{path}: newline in firmware.{key}", file=sys.stderr)
                return 2
            print(f"HPCLANG_E2E_{key.upper()}={value}")

    nor = data.get("nor")
    if nor is not None:
        if not isinstance(nor, dict):
            print(f"{path}: nor must be an object", file=sys.stderr)
            return 2
        for key in ("path", "compression", "bytes", "sha256"):
            value = str(nor.get(key, ""))
            if not value:
                print(f"{path}: nor.{key} is required", file=sys.stderr)
                return 2
            if "\n" in value or "\r" in value:
                print(f"{path}: newline in nor.{key}", file=sys.stderr)
                return 2
            print(f"HPCLANG_E2E_NOR_{key.upper()}={value}")
        if "state_bytes" in nor:
            value = str(nor["state_bytes"])
            if not value or "\n" in value or "\r" in value:
                print(f"{path}: invalid nor.state_bytes", file=sys.stderr)
                return 2
            print(f"HPCLANG_E2E_NOR_STATE_BYTES={value}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
