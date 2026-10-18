#!/usr/bin/env python3
"""Execute ARM-side branch repair and verify subsequent native Stock execution."""

import argparse
import json
from pathlib import Path
import sys
from types import SimpleNamespace

from fixtures import RETURN, package


ROOT = Path(__file__).resolve().parents[3]


def verify(target, build, dist, state_root, output_root):
    # Reuse the established USB process/readiness/timeout handling and physical
    # Stock reader. This phase needs the standard suite's installed library 275.
    sys.path.insert(0, str(ROOT / "tests/e2e/port-recovery"))
    from run import transfer
    from verify import native_words

    source = state_root / f"{target}-state"
    output = output_root / f"{target}-code-relocation"
    output.mkdir()
    package_dir = output / "package"
    package_dir.mkdir()
    payload, cases = package((build / "launchers" / f"{target}.hp").read_bytes())
    (package_dir / "recovery.hp").write_bytes(payload)
    args = SimpleNamespace(
        model=target,
        profile=json.loads((ROOT / "tests/e2e/targets" / f"{target}.json").read_text()),
        dist=dist,
        flash_busy_reads=0,
    )
    transfer(
        args,
        output / "run",
        source,
        package_dir,
        "BRLIVE",
        remote_commands=["-95 CF HOME CLEAR 'BRLIVE' RCL"],
    )
    flash = (output / "run/state/flash").read_bytes()
    assert flash == (source / "flash").read_bytes(), "Branch probe changed Flash"
    read = native_words((output / "run/state/sram").read_bytes(), flash)
    stack = read(0x806F8)
    marker = read(stack)
    assert read(marker) == 0x2911 and read(marker + 5) == RETURN, "Missing final RPL marker"
    results = []
    for index, case in enumerate(reversed(cases), 1):
        address = read(stack + 5 * index)
        assert read(address) == 0x2A4E and read(address + 5) == 10, "Missing case report"
        value = read(address + 10)
        assert value == case["value"], (
            target,
            case["name"],
            case["repair"],
            hex(value),
        )
        results.append(
            dict(name=case["name"], repaired=case["repair"], moved=case["move"], value=value)
        )
    (output / "result.json").write_text(
        json.dumps(
            dict(target=target, cases=results, flash_unchanged=True, returned=True),
            indent=2,
        )
        + "\n"
    )
    print(
        f"PASS: {target}: ARM-side repair and {len(cases)} native branch/control cases",
        flush=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("stock",))
    parser.add_argument("family", choices=("hp4950",))
    parser.add_argument("--build", type=Path, default=ROOT / "build/e2e/code-relocation")
    parser.add_argument("--dist", type=Path, default=ROOT / "build/x50ng/dist")
    parser.add_argument("--state-root", type=Path, default=ROOT / "build")
    parser.add_argument("--output-root", type=Path, default=ROOT / "build")
    parser.add_argument("--targets-file", type=Path)
    args = parser.parse_args()
    targets_file = args.targets_file or ROOT / "build/stock-hp4950-targets.txt"
    targets = targets_file.read_text().splitlines()
    if (
        not targets
        or len(set(targets)) != len(targets)
        or not set(targets) <= {"hp49gplus", "hp50g"}
    ):
        raise ValueError("Invalid branch-probe target list")
    args.output_root.mkdir(parents=True, exist_ok=True)
    for target in targets:
        verify(
            target,
            args.build.resolve(),
            args.dist.resolve(),
            args.state_root.resolve(),
            args.output_root.resolve(),
        )


if __name__ == "__main__":
    main()
