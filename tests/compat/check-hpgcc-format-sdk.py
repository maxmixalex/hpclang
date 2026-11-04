"""Build the mixed C/C++ legacy-format native probe using an existing installed SDK."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    args = parser.parse_args()
    sdk, work = args.sdk.resolve(), args.work.resolve()
    root = Path(__file__).resolve().parents[2]
    work.mkdir(parents=True, exist_ok=False)
    application = work / "application"
    application.mkdir()
    sources = ("main.c", "legacy.c", "modern.cpp", "legacy.cpp")
    hashes = {}
    for name in sources:
        source = root / "tests/e2e/hpgcc-format" / name
        shutil.copyfile(source, application / name)
        hashes[name] = hashlib.sha256(source.read_bytes()).hexdigest()
    (application / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(format_probe LANGUAGES C CXX ASM)\n"
        "find_package(hpclang CONFIG REQUIRED)\n"
        "hpclang_add_application(format main.c legacy.c modern.cpp legacy.cpp)\n"
        "target_compile_features(format PRIVATE c_std_11 cxx_std_17)\n"
        "hpclang_target_launchers(format)\n"
    )
    env = dict(os.environ)
    env["PATH"] = str(Path(env["ATFE"]) / "bin") + os.pathsep + env["PATH"]
    build = work / "build"
    with (work / "build.log").open("w") as log:
        for command in (
            [
                "cmake",
                "-S",
                str(application),
                "-B",
                str(build),
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                f"-DCMAKE_PREFIX_PATH={sdk}",
                f"-DCMAKE_TOOLCHAIN_FILE={sdk}/share/hpclang/cmake/toolchains/hpclang-armv4t.cmake",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            ],
            ["cmake", "--build", str(build), "--parallel", "2"],
        ):
            subprocess.run(command, check=True, env=env, stdout=log, stderr=subprocess.STDOUT)
    artifact = work / "artifact"
    artifact.mkdir()
    for suffix in ("elf", "hpc"):
        shutil.copyfile(build / f"format.{suffix}", artifact / f"e2e.{suffix}")
    shutil.copytree(build / "format-launchers", artifact / "launchers")
    symbols = subprocess.check_output(
        [str(Path(env["ATFE"]) / "bin/llvm-nm"), str(artifact / "e2e.elf")], text=True
    )
    addresses = [
        line.split()[0] for line in symbols.splitlines() if line.split()[-1] == "hpclang_e2e_result"
    ]
    if len(addresses) != 1:
        raise AssertionError("Missing unique native result address")
    (artifact / "e2e.result-address").write_text("0x" + addresses[0] + "\n")
    models = ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g")
    embedded = (artifact / "launchers/objects/application.bin").read_bytes()
    assert (artifact / "e2e.hpc").read_bytes()[:4] == b"HPC1" and embedded
    for model in models:
        binary = (artifact / "launchers" / f"{model}.bin").read_bytes()
        wrapped = (artifact / "launchers" / f"{model}.hp").read_bytes()
        if embedded not in binary or binary not in wrapped or wrapped[:8] != b"HPHP49-C":
            raise AssertionError(f"Invalid launcher envelope: {model}")
    commands = json.loads((build / "compile_commands.json").read_text())
    if any(str(root) in row["command"] for row in commands):
        raise AssertionError("Repository source/include path leaked into SDK compilation")
    (work / "report.json").write_text(
        json.dumps(
            {
                "passed": True,
                "scope": "installed SDK build/package, not native execution",
                "sdk": str(sdk),
                "source_sha256": hashes,
                "models": models,
                "elf_sha256": hashlib.sha256((artifact / "e2e.elf").read_bytes()).hexdigest(),
            },
            indent=2,
        )
        + "\n"
    )
    print("PASS mixed C/C++ installed-SDK build and six-model packages")


if __name__ == "__main__":
    main()
