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
    if (
        isinstance(contract, dict)
        and type(contract.get("version")) is int
        and contract["version"] == 2
    ):
        if set(contract) != {"version", "families"} or not isinstance(contract["families"], dict):
            raise ValueError("invalid Stock lifecycle family fields")
        if not contract["families"]:
            raise ValueError("Stock lifecycle requires at least one family")
        for family, profile in contract["families"].items():
            validate_profile(family, profile, sequences)
        return contract["families"]
    if not isinstance(contract, dict) or set(contract) != {
        "version",
        "family",
        "initial_phase",
        "relaunches",
    }:
        raise ValueError("invalid Stock lifecycle fields")
    if type(contract["version"]) is not int or contract["version"] != 1:
        raise ValueError("unsupported Stock lifecycle version")
    profile = {key: contract[key] for key in ("initial_phase", "relaunches")}
    validate_profile(contract["family"], profile, sequences)
    return {contract["family"]: profile}


def validate_profile(family: str, contract: dict, sequences: Path) -> None:
    if family not in ("hp39", "hp48", "hp4950"):
        raise ValueError("unsupported Stock lifecycle family")
    if not isinstance(contract, dict) or set(contract) != {"initial_phase", "relaunches"}:
        raise ValueError("invalid Stock lifecycle profile fields")

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
    parser.add_argument("--has-family", choices=("hp39", "hp48", "hp4950"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    sequences = root / "tests/e2e/sequences"
    contracts = load_contract(args.contract, sequences)
    if args.has_family:
        print(int(args.has_family in contracts))
        return
    if args.validate_only:
        return
    family = os.environ["HPCLANG_E2E_FAMILY"]
    if family not in contracts:
        raise ValueError("lifecycle does not define the current Stock family")
    contract = contracts[family]
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
                "HPCLANG_STOCK_REQUIRE_OBJECT_PHASE": "1",
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
            f"--style={root / 'build/x50ng/dist' / ('style-50g.css' if family == 'hp4950' else 'style-40g.css')}",
        ]
        with Path(f"{prefix}.log").open("w", encoding="utf-8") as log:
            subprocess.run(
                command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120, check=True
            )
        verify_result(result, step["phase"])
        print(f"PASS: {target} Stock lifecycle run {index}: {step['phase']}", flush=True)


if __name__ == "__main__":
    main()
