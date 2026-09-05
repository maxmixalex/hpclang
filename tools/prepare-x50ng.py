#!/usr/bin/env python3
"""Prepare a pinned x50ng checkout for one hpclang headless E2E frontend."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def replace_once(path: Path, old: str, new: str, what: str) -> None:
    text = path.read_text()
    if old not in text:
        raise SystemExit(f"{path}: {what} anchor changed")
    path.write_text(text.replace(old, new, 1))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--frontend", type=Path, required=True)
    parser.add_argument("--headless-header", type=Path, required=True)
    parser.add_argument("--little-apple", action="store_true")
    parser.add_argument("--reserve-ram", action="store_true")
    args = parser.parse_args()

    root = args.x50ng
    shutil.copy2(args.frontend, root / "src" / "main.c")
    shutil.copy2(args.headless_header, root / "src" / "headless-ui-init.h")

    if args.little_apple:
        replace_once(
            root / "src" / "flash.c",
            'memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0',
            '''memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP39G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP40G+IMAGE", 16 ) != 0''',
            "Little-Apple firmware header",
        )

    if args.reserve_ram:
        replace_once(
            root / "src" / "module.c",
            "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
            "    /* hpclang E2E reserved RAM. */\\n"
            "    phys_ram_size += 0x00100000;\\n\\n"
            "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
            "reserved RAM",
        )


if __name__ == "__main__":
    main()
