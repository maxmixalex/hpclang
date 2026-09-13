#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def expand(value: str) -> str:
    return os.path.expandvars(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("spec", type=Path)
    parser.add_argument("--image-out")
    parser.add_argument("--image-url")
    parser.add_argument("--image-only", action="store_true")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    command = [
        sys.executable,
        "tools/pgm-to-summary.py",
        "--heading",
        expand(spec.get("heading", "Calculator E2E")),
    ]

    summary = spec.get("summary", {})
    if "scale" in summary:
        command += ["--scale", str(summary["scale"])]
    if "columns" in summary:
        command += ["--columns", str(summary["columns"])]

    if args.image_out:
        command += ["--image-out", args.image_out]
    if args.image_url:
        command += ["--image-url", args.image_url]
    if args.image_only:
        if not args.image_out:
            raise SystemExit("--image-only requires --image-out")
        command.append("--image-only")

    for raw in spec.get("json", []):
        command += ["--json", expand(raw)]
    command += [expand(raw) for raw in spec.get("captures", [])]

    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
