#!/usr/bin/env python3
"""Expose one emulator E2E target profile through GitHub Actions env."""

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} TARGET.json", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    data = json.loads(path.read_text(encoding="utf-8"))
    required = {
        "target", "family", "model", "archive", "member",
        "x50ng_model", "lcd_height",
    }
    missing = sorted(required - data.keys())
    if missing:
        print(f"{path}: missing keys: {', '.join(missing)}", file=sys.stderr)
        return 2

    if data["target"] != path.stem:
        print(
            f"{path}: target {data['target']!r} does not match filename",
            file=sys.stderr,
        )
        return 2

    for key in (
        "target", "family", "model", "archive", "member",
        "x50ng_model", "lcd_height",
    ):
        value = str(data[key])
        if "\n" in value or "\r" in value:
            print(f"{path}: newline in {key}", file=sys.stderr)
            return 2
        print(f"HPCLANG_E2E_{key.upper()}={value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
