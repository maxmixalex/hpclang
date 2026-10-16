#!/usr/bin/env python3
"""Analyze owned translation units using their real CMake compile commands."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import shlex
import re
from pathlib import Path
import subprocess
import tempfile


def compile_key(arguments, directory):
    """Deduplicate only understood invocations; unknown flags retain their cwd."""
    normalized = [arguments[0]]
    contextual = False
    index = 1
    independent = {
        "-c",
        "-pthread",
        "-ffreestanding",
        "-ffunction-sections",
        "-fdata-sections",
        "-fPIC",
        "-fPIE",
        "-fpic",
        "-fpie",
        "-fexceptions",
        "-frtti",
        "-nostdinc",
        "-nostdinc++",
        "-nostdlib",
    }
    while index < len(arguments):
        argument = arguments[index]
        index += 1
        if argument == "-o" and index < len(arguments):
            index += 1
            continue
        if argument in ("-I", "-isystem", "-iquote", "-include", "-imacros"):
            if index < len(arguments):
                operand = Path(arguments[index])
                normalized.extend((argument, str(operand)))
                contextual |= not operand.is_absolute()
                index += 1
                continue
        if argument.startswith("-I") and len(argument) > 2:
            operand = argument[2:]
            normalized.extend(("-I", operand))
            contextual |= not Path(operand).is_absolute()
            continue
        normalized.append(argument)
        if argument in independent or re.fullmatch(
            r"-(?:[DUW].+|O[0-3gsz]|g[0-3]?|std=.+|-target=.+|"
            r"m(?:arch|cpu|fpu|float-abi|abi)=.+|mthumb|marm|fno-[a-z-]+)",
            argument,
        ):
            continue
        if not Path(argument).is_absolute():
            contextual = True
    return tuple(normalized), str(directory) if contextual else None


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
    entries = {}
    configurations = 0
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
            arguments = entry.get("arguments") or shlex.split(entry["command"])
            if host_target:
                arguments = [*arguments, f"--target={host_target}"]
            directory = Path(entry["directory"]).resolve()
            key = (str(source), compile_key(arguments, directory))
            entries.setdefault(
                key,
                {
                    "directory": str(directory),
                    "file": str(source),
                    "arguments": arguments,
                },
            )
            configurations += 1
        if not sources:
            raise SystemExit(f"{build}: no owned C/C++ translation units")
        print(f"Clang analysis: {build}: {len(sources)} translation units", flush=True)
    print(
        f"Clang analysis: {configurations} compile entries, {len(entries)} unique configurations",
        flush=True,
    )
    with tempfile.TemporaryDirectory(prefix="hpclang-analysis-") as temporary:
        database = Path(temporary)
        (database / "compile_commands.json").write_text(json.dumps(list(entries.values())))
        tasks = []
        for source in sorted({entry["file"] for entry in entries.values()}):
            command = [
                args.clang_check,
                "--analyze",
                "-p",
                str(database),
                str(source),
                "--extra-arg=-Xanalyzer",
                "--extra-arg=-analyzer-werror",
            ]
            # Runtime ROM/MMIO code is also compiled by host unit-test targets.
            # Fixed-address access is intentional in this embedded SDK.
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

        failed = False
        with ThreadPoolExecutor(max_workers=2) as workers:
            futures = [workers.submit(run, command) for command in tasks]
            for completed, future in enumerate(as_completed(futures), 1):
                code, output = future.result()
                failed |= code != 0
                if output:
                    print(output, end="", flush=True)
                if completed % 25 == 0 or completed == len(tasks):
                    print(f"Clang analysis: {completed}/{len(tasks)} sources completed", flush=True)
        return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
