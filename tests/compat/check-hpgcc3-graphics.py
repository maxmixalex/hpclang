"""Build original HPGCC3 graphics documentation programs with an installed SDK."""

import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile

MEMBER = "libs_workspace/headers/hpgraphics.h"
MEMBER_SHA256 = "4eb01aa9bb84e199bb438bee60c0fb65fc5c59cf6c43b1fe87c654dd7b8f787d"
MODELS = ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g")
PROGRAMS = ("hello", "gray", "shapes")


def extract(archive):
    with tarfile.open(archive) as release:
        with release.extractfile(MEMBER) as stream:
            data = stream.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != MEMBER_SHA256:
        raise ValueError(f"Unreviewed {MEMBER}: SHA256 {digest}")
    text = data.decode("ascii").replace("\r\n", "\n")
    blocks = re.findall(r"\\code\n(.*?)\\endcode", text, re.S)
    if len(blocks) != 4:
        raise ValueError("Expected three self-contained programs and the external-XPM example")
    for name, block in zip(PROGRAMS, blocks[:3], strict=True):
        source = re.sub(r"(?m)^ \* ?", "", block).rstrip() + "\n"
        old = "#include <hpgcc3.h>"
        if source.count(old) != 1:
            raise ValueError(f"Unexpected umbrella include in {name}")
        yield name, source, source.replace(old, "#include <hpkeyb49.h>")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="Caller-supplied HPGCC3 R004.revA archive")
    parser.add_argument("--sdk", required=True, type=Path, help="Existing installed SDK prefix")
    parser.add_argument("--work", required=True, type=Path, help="New evidence/build directory")
    parser.add_argument(
        "--native", action="store_true", help="Also build observed native applications"
    )
    args = parser.parse_args()
    examples = list(extract(args.archive))
    sdk = args.sdk.resolve()
    toolchain = sdk / "share/hpclang/cmake/toolchains/hpclang-armv4t.cmake"
    if not toolchain.is_file():
        raise ValueError(f"Missing installed SDK toolchain: {toolchain}")
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    application = work / "application"
    application.mkdir()
    report = {
        "version": 1,
        "archive": args.archive.name,
        "archive_sha256": hashlib.sha256(args.archive.read_bytes()).hexdigest(),
        "member": MEMBER,
        "member_sha256": MEMBER_SHA256,
        "sdk": str(sdk),
        "programs": [],
        "limits": [
            "Compilation/linking/packaging only; no native execution or pixel acceptance",
            "The fourth documentation example requires an external monalisa.xpm asset",
            "Original geometry and busy ON-key waits are retained",
            "Original programs do not check display/font allocation errors",
        ],
    }
    cmake = [
        "cmake_minimum_required(VERSION 3.20)",
        "project(hpgcc3_graphics LANGUAGES C ASM)",
        "find_package(hpclang CONFIG REQUIRED)",
    ]
    for name, original, adapted in examples:
        (work / f"{name}-original.c").write_text(original, encoding="utf-8")
        (application / f"{name}.c").write_text(adapted, encoding="utf-8")
        (work / f"{name}.diff").write_text(
            "".join(
                difflib.unified_diff(
                    original.splitlines(True),
                    adapted.splitlines(True),
                    fromfile=f"original/{name}.c",
                    tofile=f"adapted/{name}.c",
                )
            ),
            encoding="utf-8",
        )
        report["programs"].append(
            {
                "name": name,
                "extracted_sha256": hashlib.sha256(original.encode()).hexdigest(),
                "adapted_sha256": hashlib.sha256(adapted.encode()).hexdigest(),
                "migration": "Replace hpgcc3.h with hpkeyb49.h; retain all application statements",
            }
        )
        cmake += [
            f"hpclang_add_application({name} {name}.c)",
            f"hpclang_target_launchers({name})",
        ]
        if args.native:
            (application / f"{name}-instrumented.c").write_text(
                f'#define main historical_main\n#include "{name}.c"\n', encoding="utf-8"
            )
            cmake += [
                f"hpclang_add_application({name}_native {name}-instrumented.c native.c)",
                f"target_link_options({name}_native PRIVATE LINKER:--wrap=keyb_isON)",
                f"hpclang_target_launchers({name}_native)",
            ]
    if args.native:
        shutil.copy2(Path(__file__).with_name("hpgcc3-graphics-native.c"), application / "native.c")
    (application / "CMakeLists.txt").write_text("\n".join(cmake) + "\n", encoding="utf-8")
    report_path = work / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    env = dict(os.environ)
    env["PATH"] = str(Path(env["ATFE"]) / "bin") + os.pathsep + env["PATH"]
    build = work / "build"
    with (work / "build.log").open("w", encoding="utf-8") as log:
        for command in (
            [
                "cmake",
                "-S",
                str(application),
                "-B",
                str(build),
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                f"-DCMAKE_PREFIX_PATH={sdk}",
                f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            ],
            ["cmake", "--build", str(build), "--parallel", "2"],
        ):
            subprocess.run(
                command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1200
            )
    for program in report["programs"]:
        name = program["name"]
        payload = (build / f"{name}.hpc").read_bytes()
        if payload[:4] != b"HPC1":
            raise ValueError(f"Invalid payload: {name}")
        launchers = build / f"{name}-launchers"
        embedded = (launchers / "objects/application.bin").read_bytes()
        for model in MODELS:
            binary = (launchers / f"{model}.bin").read_bytes()
            wrapped = (launchers / f"{model}.hp").read_bytes()
            if (
                not embedded
                or embedded not in binary
                or binary not in wrapped
                or wrapped[:8] != b"HPHP49-C"
            ):
                raise ValueError(f"Invalid launcher envelope: {name}/{model}")
        program["hpc_sha256"] = hashlib.sha256(payload).hexdigest()
        program["packaged_models"] = list(MODELS)
        if args.native:
            target = f"{name}_native"
            artifact = work / f"{name}-native-artifact"
            artifact.mkdir()
            for suffix in ("elf", "hpc"):
                shutil.copy2(build / f"{target}.{suffix}", artifact / f"e2e.{suffix}")
            shutil.copytree(build / f"{target}-launchers", artifact / "launchers")
            symbols = subprocess.check_output(
                [str(Path(env["ATFE"]) / "bin/llvm-nm"), str(artifact / "e2e.elf")], text=True
            )
            addresses = [
                line.split()[0]
                for line in symbols.splitlines()
                if line.split()[-1] == "hpclang_e2e_result"
            ]
            if len(addresses) != 1:
                raise ValueError(f"Missing native observation symbol: {name}")
            (artifact / "e2e.result-address").write_text(
                "0x" + addresses[0] + "\n", encoding="utf-8"
            )
            program["native_artifact"] = str(artifact)
    report["passed"] = True
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("PASS three original HPGCC3 documentation programs: installed SDK and six-model packages")


if __name__ == "__main__":
    main()
