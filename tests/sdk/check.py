"""Build external examples after relocating an installed SDK."""

import argparse
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aplet-tool", type=Path)
    parser.add_argument("--aplet-assets", type=Path)
    args = parser.parse_args()
    if bool(args.aplet_tool) != bool(args.aplet_assets):
        parser.error("--aplet-tool and --aplet-assets must be provided together")
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
        shutil.copytree(package / "examples/declarative-ui-cpp", application / "counter")
        with (application / "CMakeLists.txt").open("a") as source:
            source.write("\nadd_subdirectory(counter)\n")
        if args.aplet_tool:
            with (application / "CMakeLists.txt").open("a") as source:
                source.write("""
foreach(application IN ITEMS hello_c hello_cpp)
    foreach(model IN ITEMS hp39gplus hp39gs hp40gs)
        hpclang_target_aplet(${application} MODEL ${model} NAME "SDK example"
            AUTHOR hpclang TOOL "${APLET_TOOL}" ASSETS "${APLET_ASSETS}")
    endforeach()
endforeach()
""")
            with (application / "counter/CMakeLists.txt").open("a") as source:
                source.write("""
foreach(model IN ITEMS hp39gplus hp39gs hp40gs)
    hpclang_target_aplet(declarative_ui_cpp MODEL ${model} NAME "SDK example"
        AUTHOR hpclang TOOL "${APLET_TOOL}" ASSETS "${APLET_ASSETS}")
endforeach()
""")
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
            *(
                [
                    f"-DAPLET_TOOL={args.aplet_tool.resolve()}",
                    f"-DAPLET_ASSETS={args.aplet_assets.resolve()}",
                ]
                if args.aplet_tool
                else []
            ),
            env=env,
        )
        run("cmake", "--build", str(build), "--parallel", "2", env=env)
        commands = json.loads((build / "compile_commands.json").read_text())
        for command in commands:
            serialized = json.dumps(command)
            for forbidden in (str(root), str(prefix)):
                if forbidden in serialized:
                    raise AssertionError(f"Nonrelocatable build dependency: {forbidden}")
        for name, output in (
            ("hello_c", build),
            ("hello_cpp", build),
            ("declarative_ui_cpp", build / "counter"),
        ):
            elf = (output / f"{name}.elf").read_bytes()
            hpc = (output / f"{name}.hpc").read_bytes()
            if elf[:4] != b"\x7fELF" or int.from_bytes(elf[18:20], "little") != 40:
                raise AssertionError(f"{name}: expected an ARM ELF executable")
            if hpc[:4] != b"HPC1" or len(hpc) <= 64:
                raise AssertionError(f"{name}: expected a packaged HPC1 application")
            for model in ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g"):
                launcher = output / f"{name}-launchers" / model
                binary = launcher.with_suffix(".bin").read_bytes()
                wrapped = launcher.with_suffix(".hp").read_bytes()
                if hpc not in binary or binary not in wrapped or wrapped[:8] != b"HPHP49-C":
                    raise AssertionError(
                        f"{name}/{model}: application missing from native launcher"
                    )
                if args.aplet_tool and model in ("hp39gplus", "hp39gs", "hp40gs"):
                    bundle = output / f"{name}-aplet" / model
                    aplet = (bundle / "HPGCC000.000").read_bytes()
                    if not aplet.startswith(b"HP39BinB\x0bSDK example") or hpc not in aplet:
                        raise AssertionError(f"{name}/{model}: invalid installable Aplet")
                    for asset in ("HPCRESET.000", "LIB275L.000", "HP39DIR.000", "HP39DIR.CUR"):
                        if not (bundle / asset).stat().st_size:
                            raise AssertionError(f"missing Aplet companion: {asset}")
        # A missing declared output must be regenerated without touching source.
        missing = [build / "hello_c-launchers/hp50g.hp"]
        if args.aplet_tool:
            missing.append(build / "hello_cpp-aplet/hp39gs/HPCRESET.000")
        for path in missing:
            path.unlink()
        run("cmake", "--build", str(build), "--parallel", "2", env=env)
        for path in missing:
            if not path.is_file():
                raise AssertionError(f"incremental build did not restore {path.name}")
        print("Installed SDK: C/C++, fonts, HPC1, six-model launchers and relocation PASS")
        if args.aplet_tool:
            print("Installed SDK: HP39g+/HP39gs/HP40gs C/C++ Aplet packaging PASS")


if __name__ == "__main__":
    main()
