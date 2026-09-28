#!/usr/bin/env python3
"""Analyze owned translation units using their real CMake compile commands."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import shlex
from pathlib import Path
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("clang_check")
    parser.add_argument("--host-build", action="append", default=[], type=Path)
    parser.add_argument("--arm-build", action="append", default=[], type=Path)
    args = parser.parse_args()
    if not args.host_build and not args.arm_build:
        parser.error("at least one host or ARM build is required")
    root = Path(__file__).resolve().parent.parent
    owned = set(subprocess.check_output(["git", "ls-files"], cwd=root, text=True).splitlines())
    tasks = []
    for build, embedded in [(p, False) for p in args.host_build] + [
        (p, True) for p in args.arm_build
    ]:
        database = json.loads((build / "compile_commands.json").read_text())
        sources = set()
        host_target = None
        if not embedded:
            first = database[0]
            compiler = (first.get("arguments") or shlex.split(first["command"]))[0]
            host_target = subprocess.check_output([compiler, "-dumpmachine"], text=True).strip()
        for entry in database:
            source = Path(entry["file"]).resolve()
            if not source.is_relative_to(root) or source.suffix not in (
                ".c",
                ".cc",
                ".cpp",
                ".cxx",
            ):
                continue
            relative = source.relative_to(root)
            if relative.as_posix() not in owned:
                continue
            if relative.parts[0] == "third_party" or relative.parts[:3] == (
                "runtime",
                "fonts",
                "generated",
            ):
                continue
            sources.add(source)
        if not sources:
            raise SystemExit(f"{build}: no owned C/C++ translation units")
        print(f"Clang analysis: {build}: {len(sources)} translation units", flush=True)
        for source in sorted(sources):
            command = [
                args.clang_check,
                "--analyze",
                "-p",
                str(build),
                str(source),
                "--extra-arg=-Xanalyzer",
                "--extra-arg=-analyzer-werror",
            ]
            # Runtime ROM/MMIO code is also compiled by host unit-test targets.
            # Fixed-address access is intentional in this embedded SDK.
            if host_target:
                command.append(f"--extra-arg=--target={host_target}")
            command += [
                "--extra-arg=-Xanalyzer",
                "--extra-arg=-analyzer-disable-checker=core.FixedAddressDereference",
            ]
            tasks.append(command)

    def run(command):
        result = subprocess.run(
            command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        return result.returncode, result.stdout

    with ThreadPoolExecutor(max_workers=2) as workers:
        results = list(workers.map(run, tasks))
    for _, output in results:
        if output:
            print(output, end="")
    return int(any(code != 0 for code, _ in results))


if __name__ == "__main__":
    raise SystemExit(main())
