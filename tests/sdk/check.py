"""Build external examples after relocating an installed SDK."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(*args, env):
    result = subprocess.run(args, env=env, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{args!r}\n{result.stdout}\n{result.stderr}")


def main():
    root = Path(__file__).resolve().parents[2]
    toolchain = Path(os.environ["ATFE"])
    env = dict(os.environ, PATH=str(toolchain / "bin") + os.pathsep + os.environ["PATH"])
    with tempfile.TemporaryDirectory(prefix="hpclang sdk ") as directory:
        work = Path(directory)
        prefix = work / "original prefix"
        sdk = work / "relocated sdk"
        run("cmake", "-S", str(root), "-B", str(work / "installer"), env=env)
        run("cmake", "--install", str(work / "installer"), "--prefix", str(prefix), env=env)
        prefix.rename(sdk)
        package = sdk / "share/hpclang"
        application = work / "external application"
        shutil.copytree(package / "examples/hello", application)
        build = work / "build"
        run(
            "cmake",
            "-S",
            str(application),
            "-B",
            str(build),
            "-DCMAKE_BUILD_TYPE=MinSizeRel",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            f"-DCMAKE_PREFIX_PATH={sdk}",
            f"-DCMAKE_TOOLCHAIN_FILE={package / 'cmake/toolchains/hpclang-armv4t.cmake'}",
            env=env,
        )
        run("cmake", "--build", str(build), "--parallel", "2", env=env)
        commands = json.loads((build / "compile_commands.json").read_text())
        for command in commands:
            serialized = json.dumps(command)
            for forbidden in (str(root), str(prefix)):
                if forbidden in serialized:
                    raise AssertionError(f"Nonrelocatable build dependency: {forbidden}")
        for name in ("hello_c", "hello_cpp"):
            elf = (build / f"{name}.elf").read_bytes()
            hpc = (build / f"{name}.hpc").read_bytes()
            if elf[:4] != b"\x7fELF" or int.from_bytes(elf[18:20], "little") != 40:
                raise AssertionError(f"{name}: expected an ARM ELF executable")
            if hpc[:4] != b"HPC1" or len(hpc) <= 64:
                raise AssertionError(f"{name}: expected a packaged HPC1 application")
        print("Installed SDK: external C/C++ builds, fonts, HPC1 packaging and relocation PASS")


if __name__ == "__main__":
    main()
