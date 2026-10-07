#!/usr/bin/env python3
"""Select automated model jobs from the canonical E2E target profiles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def targets(profiles: Path, mode: str, family: str | None = None, target: str | None = None):
    result = []
    for path in sorted(profiles.glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        if not data.get("automated_e2e", True):
            continue
        if mode == "stock" and not data.get("automated_stock_e2e", True):
            continue
        if family is not None and data["family"] != family:
            continue
        if target is not None and data["target"] != target:
            continue
        result.append({"family": data["family"], "target": data["target"]})
    if not result:
        raise ValueError(f"No automated {mode} targets match family={family!r}, target={target!r}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("--family")
    parser.add_argument("--target")
    parser.add_argument("--names", action="store_true")
    args = parser.parse_args()
    profiles = Path(__file__).resolve().parents[1] / "tests/e2e/targets"
    selected = targets(profiles, args.mode, args.family, args.target)
    if args.names:
        print("\n".join(item["target"] for item in selected))
    else:
        print(json.dumps(selected))


if __name__ == "__main__":
    main()
