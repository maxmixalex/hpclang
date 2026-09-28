#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path


def expand(value: str) -> str:
    return os.path.expandvars(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("spec", type=Path)
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    failures: list[str] = []

    for raw in spec.get("required_files", []):
        path = Path(expand(raw))
        if not path.is_file():
            failures.append(f"missing required file: {path}")

    for entry in spec.get("files", []):
        path = Path(expand(entry["path"]))
        if not path.is_file():
            failures.append(f"missing file: {path}")
            continue
        if "size" in entry:
            size = path.stat().st_size
            expected = int(entry["size"])
            if size != expected:
                failures.append(f"{path}: size {size}, expected {expected}")

    for entry in spec.get("logs", []):
        path = Path(expand(entry["path"]))
        if not path.is_file():
            failures.append(f"missing log: {path}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for expected in entry.get("contains", []):
            expected = expand(str(expected))
            if expected not in text:
                failures.append(f"{path}: missing text: {expected}")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1

    print(f"PASS: {args.spec}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
