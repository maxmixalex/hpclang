"""Build external examples after relocating an installed SDK."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zlib


def run(*args, env):
    result = subprocess.run(args, env=env, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{args!r}\n{result.stdout}\n{result.stderr}")


def check_interworking(package, work, env):
    """Check emitted ARMv4T returns, which newer CPU emulators may accept wrongly."""
    sources = {
        "runtime/bootstrap/sync.S": (
            "hpclang_mmu_sync",
            "hpclang_cache_clean",
            "hpclang_irq_save_disable",
            "hpclang_irq_restore",
        ),
        "runtime/bootstrap/enter.S": ("hpclang_enter",),
        "runtime/libc/kos-call.S": ("hpclang_kos_swi3", "hpclang_kos_sync3"),
    }
    for index, (source, symbols) in enumerate(sources.items()):
        obj = work / f"interworking-{index}.o"
        run(
            "clang",
            "--target=arm-none-eabi",
            "-march=armv4t",
            "-c",
            str(package / source),
            "-o",
            str(obj),
            env=env,
        )
        listing = subprocess.check_output(
            ["llvm-objdump", "-d", "--no-show-raw-insn", str(obj)], env=env, text=True
        )
        functions = re.split(r"^[0-9a-f]+ <([^>]+)>:\s*$", listing, flags=re.MULTILINE)
        bodies = dict(zip(functions[1::2], functions[2::2]))
        for symbol in symbols:
            body = bodies[symbol]
            if not re.search(r"\bbx\s+lr\b", body):
                raise AssertionError(f"{symbol}: missing ARMv4T interworking return")
            if re.search(r"\b(?:pop|ldm\w*)\s+[^\n]*\bpc\b|\b(?:ldr|mov)\s+pc\b", body):
                raise AssertionError(f"{symbol}: non-interworking PC load")
    print("Installed SDK: ARMv4T public assembly return checks PASS")


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
        run(
            "cmake",
            "--install",
            str(work / "installer"),
            "--prefix",
            str(prefix),
            env=env,
        )
        prefix.rename(sdk)
        package = sdk / "share/hpclang"
        check_interworking(package, work, env)
        application = work / "external application"
        shutil.copytree(package / "examples/hello", application)
        shutil.copytree(package / "examples/declarative-ui", application / "devices")
        shutil.copytree(package / "examples/declarative-ui-cpp", application / "counter")
        counter_cmake = application / "counter/CMakeLists.txt"
        counter_source = counter_cmake.read_text()
        launcher_call = "hpclang_target_launchers(declarative_ui_cpp)"
        if counter_source.count(launcher_call) != 1:
            raise AssertionError("C++ counter launcher declaration changed")
        counter_cmake.write_text(
            counter_source.replace(
                launcher_call,
                "hpclang_target_launchers(declarative_ui_cpp MIN_RW_BYTES 36864 MAX_RW_BYTES 36864)",
            )
        )
        shutil.copytree(package / "examples/object-cpp", application / "objects")
        shutil.copytree(package / "examples/hpgcc-timer", application / "timer")
        shutil.copytree(package / "examples/hpgcc-console", application / "console")
        shutil.copytree(package / "examples/notebook", application / "notebook")
        shutil.copyfile(root / "tests/sdk/numeric-input.cpp", application / "numeric-input.cpp")
        for suffix in ("c", "cpp"):
            shutil.copyfile(root / "tests/sdk/sound.c", application / f"sound.{suffix}")
        shutil.copyfile(root / "tests/sdk/storage.cpp", application / "storage.cpp")
        with (application / "CMakeLists.txt").open("a") as source:
            source.write(
                "\nadd_subdirectory(counter)\nadd_subdirectory(objects)\nadd_subdirectory(timer)\n"
                "add_subdirectory(notebook)\nadd_subdirectory(console)\nadd_subdirectory(devices)\n"
                "hpclang_add_application(numeric_input numeric-input.cpp)\n"
                "target_compile_features(numeric_input PRIVATE cxx_std_17)\n"
                "hpclang_target_launchers(numeric_input)\n"
                "hpclang_add_application(sound_c sound.c)\n"
                "hpclang_add_application(sound_cpp sound.cpp)\n"
                "hpclang_target_launchers(sound_c)\n"
                "hpclang_target_launchers(sound_cpp)\n"
                "hpclang_add_application(storage_cpp storage.cpp)\n"
                "target_compile_features(storage_cpp PRIVATE cxx_std_17)\n"
                "hpclang_target_launchers(storage_cpp)\n"
            )
        if args.aplet_tool:
            with (application / "CMakeLists.txt").open("a") as source:
                source.write("""
foreach(application IN ITEMS hello_c hello_cpp numeric_input sound_c sound_cpp storage_cpp)
    foreach(model IN ITEMS hp39gplus hp39gs hp40gs)
        hpclang_target_aplet(${application} MODEL ${model} NAME "SDK example"
            AUTHOR hpclang TOOL "${APLET_TOOL}" ASSETS "${APLET_ASSETS}")
    endforeach()
endforeach()
""")
            for directory, target in (
                ("devices", "declarative_ui"),
                ("counter", "declarative_ui_cpp"),
                ("objects", "object_cpp"),
                ("timer", "hpgcc_timer"),
                ("console", "hpgcc_console"),
                ("notebook", "notebook"),
            ):
                with (application / directory / "CMakeLists.txt").open("a") as source:
                    source.write(
                        """
foreach(model IN ITEMS hp39gplus hp39gs hp40gs)
    hpclang_target_aplet(TARGET_NAME MODEL ${model} NAME "SDK example"
        AUTHOR hpclang TOOL "${APLET_TOOL}" ASSETS "${APLET_ASSETS}")
endforeach()
""".replace("TARGET_NAME", target)
                    )
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
        counter_args = (
            (build / "counter/declarative_ui_cpp-launchers.args").read_text().splitlines()
        )
        if counter_args[-4:] != ["--min-rw-bytes", "36864", "--max-rw-bytes", "36864"]:
            raise AssertionError("Relocated SDK lost the requested application RW limit")
        commands = json.loads((build / "compile_commands.json").read_text())
        for command in commands:
            serialized = json.dumps(command)
            for forbidden in (str(root), str(prefix)):
                if forbidden in serialized:
                    raise AssertionError(f"Nonrelocatable build dependency: {forbidden}")
        for name, output in (
            ("hello_c", build),
            ("hello_cpp", build),
            ("numeric_input", build),
            ("sound_c", build),
            ("sound_cpp", build),
            ("storage_cpp", build),
            ("declarative_ui", build / "devices"),
            ("declarative_ui_cpp", build / "counter"),
            ("object_cpp", build / "objects"),
            ("hpgcc_timer", build / "timer"),
            ("hpgcc_console", build / "console"),
            ("notebook", build / "notebook"),
        ):
            elf = (output / f"{name}.elf").read_bytes()
            hpc = (output / f"{name}.hpc").read_bytes()
            if elf[:4] != b"\x7fELF" or int.from_bytes(elf[18:20], "little") != 40:
                raise AssertionError(f"{name}: expected an ARM ELF executable")
            if hpc[:4] != b"HPC1" or len(hpc) <= 64:
                raise AssertionError(f"{name}: expected a packaged HPC1 application")
            memory = json.loads((output / f"{name}-launchers/memory.json").read_text())
            if (
                memory["version"] != 1
                or memory["page_bytes"] != 4096
                or memory["application"]["image_bytes"] != len(hpc) - 16
                or memory["application"]["static_bytes"] != int.from_bytes(hpc[12:16], "little")
            ):
                raise AssertionError(f"{name}: memory report does not describe the application")
            ceiling = 36864 if name in ("declarative_ui", "declarative_ui_cpp") else None
            if memory["application"]["rw_ceiling_bytes"] != ceiling:
                raise AssertionError(f"{name}: memory report lost its configured RW ceiling")
            if memory["application"]["required_rw_bytes"] != ceiling:
                raise AssertionError(f"{name}: memory report lost its required RW size")
            if len(memory["models"]) != 6 or not memory["excludes"]:
                raise AssertionError(f"{name}: incomplete memory report scope")
            embedded = (output / f"{name}-launchers/objects/application.bin").read_bytes()
            restored = embedded
            if embedded[:4] == b"HPCZ":
                restored = b"HPC1" + embedded[4:16] + zlib.decompress(embedded[16:], wbits=-15)
            if restored != hpc:
                raise AssertionError(f"{name}: launcher image differs from public HPC1")
            if memory["serialized"]["hpc1_file_bytes"] != len(hpc) or memory["serialized"][
                "embedded_payload_bytes"
            ] != len(embedded):
                raise AssertionError(f"{name}: serialized application accounting mismatch")
            for model in (
                "hp39gplus",
                "hp39gs",
                "hp40gs",
                "hp48gii",
                "hp49gplus",
                "hp50g",
            ):
                launcher = output / f"{name}-launchers" / model
                binary = launcher.with_suffix(".bin").read_bytes()
                wrapped = launcher.with_suffix(".hp").read_bytes()
                sizes = memory["serialized"]["models"][model]
                if sizes != {
                    "launcher_bytes": len(binary),
                    "transfer_file_bytes": len(wrapped),
                    "native_object_nibbles": 2 * (len(wrapped) - 8),
                }:
                    raise AssertionError(f"{name}/{model}: serialized launcher accounting mismatch")
                if embedded not in binary or binary not in wrapped or wrapped[:8] != b"HPHP49-C":
                    raise AssertionError(
                        f"{name}/{model}: application missing from native launcher"
                    )
                if args.aplet_tool and model in ("hp39gplus", "hp39gs", "hp40gs"):
                    bundle = output / f"{name}-aplet" / model
                    aplet = (bundle / "HPGCC000.000").read_bytes()
                    if not aplet.startswith(b"HP39BinB\x0bSDK example") or embedded not in aplet:
                        raise AssertionError(f"{name}/{model}: invalid installable Aplet")
                    for asset in (
                        "HPCRESET.000",
                        "LIB275L.000",
                        "HP39DIR.000",
                        "HP39DIR.CUR",
                    ):
                        if not (bundle / asset).stat().st_size:
                            raise AssertionError(f"missing Aplet companion: {asset}")
        # A missing declared output must be regenerated without touching source.
        missing = [build / "hello_c-launchers/hp50g.hp", build / "hello_cpp-launchers/memory.json"]
        if args.aplet_tool:
            missing.append(build / "hello_cpp-aplet/hp39gs/HPCRESET.000")
        for path in missing:
            path.unlink()
        run("cmake", "--build", str(build), "--parallel", "2", env=env)
        for path in missing:
            if not path.is_file():
                raise AssertionError(f"incremental build did not restore {path.name}")
        # Exercise all model selectors from the relocated SDK. Each build keeps
        # the same notebook source and packages only its matching launcher.
        for model in ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g"):
            selected = work / f"model-{model}"
            run(
                "cmake",
                "-S",
                str(package / "examples/notebook"),
                "-B",
                str(selected),
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                f"-DHPCLANG_BUILD_MODEL={model}",
                f"-DCMAKE_PREFIX_PATH={sdk}",
                f"-DCMAKE_TOOLCHAIN_FILE={package / 'cmake/toolchains/hpclang-armv4t.cmake'}",
                env=env,
            )
            run("cmake", "--build", str(selected), "--parallel", "2", env=env)
            launchers = selected / "notebook-launchers"
            if {p.stem for p in launchers.glob("*.hp")} != {model}:
                raise AssertionError(f"{model}: model build packaged other calculators")
            if (
                model == "hp39gplus"
                and (selected / "notebook.hpc").stat().st_size
                >= (build / "notebook/notebook.hpc").stat().st_size
            ):
                raise AssertionError("HP39 specialization did not reduce notebook code size")
        print("Installed SDK: all six model-specific notebook builds PASS")
        rejected = work / "rejected model application"
        shutil.copytree(package / "examples/notebook", rejected)
        cmake_file = rejected / "CMakeLists.txt"
        cmake_file.write_text(
            cmake_file.read_text().replace(
                "hpclang_target_launchers(notebook)",
                "hpclang_target_launchers(notebook MODELS hp50g)",
            )
        )
        for model, diagnostic in (
            ("unknown", "Unsupported HPCLANG_BUILD_MODEL"),
            ("hp39gplus", "does not match HPCLANG_BUILD_MODEL"),
        ):
            result = subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(rejected),
                    "-B",
                    str(work / f"reject-{model}"),
                    f"-DHPCLANG_BUILD_MODEL={model}",
                    f"-DCMAKE_PREFIX_PATH={sdk}",
                    f"-DCMAKE_TOOLCHAIN_FILE={package / 'cmake/toolchains/hpclang-armv4t.cmake'}",
                ],
                env=env,
                capture_output=True,
                text=True,
            )
            if result.returncode == 0 or diagnostic not in result.stdout + result.stderr:
                raise AssertionError(f"Model configuration did not reject {model}: {result}")
        print("Installed SDK: C/C++, fonts, HPC1, six-model launchers and relocation PASS")
        if args.aplet_tool:
            print("Installed SDK: HP39g+/HP39gs/HP40gs C/C++ Aplet packaging PASS")


if __name__ == "__main__":
    main()
