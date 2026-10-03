#!/usr/bin/env python3
"""Verify payload-defined phases across independent Stock emulator launches."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def load_contract(path: Path, sequences: Path) -> dict:
    contract = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(contract, dict) or set(contract) != {
        "version",
        "family",
        "initial_phase",
        "relaunches",
    }:
        raise ValueError("invalid Stock lifecycle fields")
    if type(contract["version"]) is not int or contract["version"] != 1:
        raise ValueError("unsupported Stock lifecycle version")
    if contract["family"] != "hp39":
        raise ValueError("Stock lifecycle currently supports the HP39 Aplet return path")

    def phase(value):
        if not isinstance(value, str) or not re.fullmatch(r"0x4f53[0-9a-f]{4}", value):
            raise ValueError("lifecycle phase must be an object-service phase word")

    phase(contract["initial_phase"])
    relaunches = contract["relaunches"]
    if not isinstance(relaunches, list) or not 1 <= len(relaunches) <= 16:
        raise ValueError("lifecycle requires 1..16 relaunches")
    for step in relaunches:
        if not isinstance(step, dict) or set(step) != {"sequence", "phase"}:
            raise ValueError("invalid lifecycle relaunch fields")
        phase(step["phase"])
        name = step["sequence"]
        if not isinstance(name, str) or not re.fullmatch(r"[a-z0-9-]+\.seq", name):
            raise ValueError("invalid lifecycle sequence name")
        if not (sequences / name).is_file():
            raise ValueError(f"missing lifecycle sequence: {name}")
    return contract


def verify_result(path: Path, expected: str) -> None:
    result = json.loads(path.read_text(encoding="utf-8"))
    if (
        result.get("status") != "pass"
        or result.get("saw_pass") is not True
        or result.get("returned") is not True
        or result.get("return_settled") is not True
        or result.get("object_store_phase") != int(expected, 16)
    ):
        raise ValueError(f"{path}: expected settled PASS with phase {expected}; got {result}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contract", type=Path)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    sequences = root / "tests/e2e/sequences"
    contract = load_contract(args.contract, sequences)
    if args.validate_only:
        return
    if os.environ["HPCLANG_E2E_FAMILY"] != contract["family"]:
        raise ValueError("lifecycle family differs from the current Stock target")
    target = os.environ["TARGET"]
    state = root / "build" / f"{target}-state"
    verify_result(Path(os.environ["HPCLANG_X50NG_E2E_JSON"]), contract["initial_phase"])
    for index, step in enumerate(contract["relaunches"], 2):
        prefix = root / "build" / f"{target}-lifecycle-{index}"
        result = Path(f"{prefix}.json")
        result.unlink(missing_ok=True)
        env = dict(os.environ)
        for key in ("HPCLANG_X50NG_USB0", "HPCLANG_X50NG_UART0", "HPCLANG_E2E_HOST_GATE_FILE"):
            env.pop(key, None)
        env.update(
            {
                "HPCLANG_E2E_SEQUENCE": str(sequences / step["sequence"]),
                "HPCLANG_STOCK_PREFIX": str(prefix),
                "HPCLANG_STOCK_JSON": f"{prefix}-stock.json",
                "HPCLANG_X50NG_E2E_JSON": str(result),
                "HPCLANG_X50NG_BEFORE_PGM": f"{prefix}-before.pgm",
                "HPCLANG_X50NG_PASS_PGM": f"{prefix}-pass.pgm",
                "HPCLANG_X50NG_RETURN_PGM": f"{prefix}-return.pgm",
            }
        )
        # Reuse Stock's saved state; never reinstall or reset the Aplet here.
        # The sequence's 90s monitor has 30s of startup/shutdown headroom.
        command = [
            str(root / "build/x50ng/dist/x50ng"),
            f"--datadir={state}",
            "--overwrite-config",
            f"--style={root / 'build/x50ng/dist/style-40g.css'}",
        ]
        with Path(f"{prefix}.log").open("w", encoding="utf-8") as log:
            subprocess.run(
                command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120, check=True
            )
        verify_result(result, step["phase"])
        print(f"PASS: {target} Stock lifecycle run {index}: {step['phase']}", flush=True)


if __name__ == "__main__":
    main()
