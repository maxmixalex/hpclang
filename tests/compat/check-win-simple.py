"""Exercise the original 1.1 window application and build it with a relocated SDK."""

import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile

FOUNDATION = (
    "hpgcc-text",
    "hpgcc-progress",
    "hpgcc-checkbox",
    "hpgcc-softmenu",
    "hpgcc-widget",
    "win-focus",
    "hpgcc-window",
    "win-key-state",
    "win-drivers",
    "win-keyboard",
    "win-repaint",
    "win-timers",
    "win-loop",
    "win-events",
)
MODELS = ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--compiler", default="cc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ)
    env["PATH"] = str(Path(env["ATFE"]) / "bin") + os.pathsep + env["PATH"]

    def run(*command):
        subprocess.run(command, check=True, env=env, timeout=600)

    archive = args.archives / "hpgcc1.1-with-39g-support-windows.zip"
    with zipfile.ZipFile(archive) as release:
        original = release.read("examples/win/simple.c")
    source = original.decode("ascii").replace("\r\n", "\n")
    header = "#include <hpgcc49.h>\n"
    assert source.count(header) == 1
    adapted = source.replace(header, "")
    application = work / "application"
    application.mkdir()
    (application / "main.c").write_text(adapted, encoding="utf-8")
    (work / "migration.diff").write_text(
        "".join(
            difflib.unified_diff(
                source.splitlines(True),
                adapted.splitlines(True),
                fromfile="original/simple.c",
                tofile="adapted/main.c",
            )
        ),
        encoding="utf-8",
    )
    report = {
        "archive": archive.name,
        "member": "examples/win/simple.c",
        "original_sha256": hashlib.sha256(original).hexdigest(),
        "adapted_sha256": hashlib.sha256(adapted.encode()).hexdigest(),
        "migration": "Remove unused hpgcc49.h include; normalize CRLF to LF",
        "limits": [
            "Host renderer is stubbed; no native application acceptance",
            "Original fourth slider remains at y=65 on 64-line screens",
            "Harness cleans original process-exit-only widget ownership",
        ],
    }
    flags = (
        "-std=c11",
        "-funsigned-char",
        "-O1",
        "-g",
        "-fsanitize=address,undefined",
        "-fno-omit-frame-pointer",
        "-I",
        str(root / "include"),
        "-I",
        str(root / "runtime/core"),
    )
    run(
        args.compiler,
        *flags,
        "-Dmain=historical_main",
        "-c",
        str(application / "main.c"),
        "-o",
        str(work / "original.o"),
    )
    run(
        args.compiler,
        *flags,
        str(root / "tests/compat/win-simple.c"),
        *(str(root / f"runtime/core/{name}.c") for name in FOUNDATION),
        str(work / "original.o"),
        "-Wl,--wrap=win_event_loop",
        "-lm",
        "-o",
        str(work / "interaction"),
    )
    run(str(work / "interaction"))
    prefix, sdk = work / "original prefix", work / "relocated sdk"
    run("cmake", "-S", str(root), "-B", str(work / "installer"))
    run("cmake", "--install", str(work / "installer"), "--prefix", str(prefix))
    prefix.rename(sdk)
    (application / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(historical_window LANGUAGES C ASM)\n"
        "find_package(hpclang CONFIG REQUIRED)\n"
        "hpclang_add_application(historical_window main.c)\n"
        "hpclang_target_launchers(historical_window)\n",
        encoding="utf-8",
    )
    build = work / "build"
    run(
        "cmake",
        "-S",
        str(application),
        "-B",
        str(build),
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
        f"-DCMAKE_PREFIX_PATH={sdk}",
        f"-DCMAKE_TOOLCHAIN_FILE={sdk}/share/hpclang/cmake/toolchains/hpclang-armv4t.cmake",
    )
    run("cmake", "--build", str(build), "--parallel", "2")
    assert (build / "historical_window.hpc").read_bytes()[:4] == b"HPC1"
    launchers = build / "historical_window-launchers"
    embedded = (launchers / "objects/application.bin").read_bytes()
    for model in MODELS:
        binary = (launchers / f"{model}.bin").read_bytes()
        wrapped = (launchers / f"{model}.hp").read_bytes()
        assert embedded in binary and binary in wrapped and wrapped[:8] == b"HPHP49-C"
    report["passed"] = [
        "Production event-loop host interaction under ASan/UBSan",
        "Relocated SDK ARM compilation and six launcher envelopes",
    ]
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("PASS original 1.1 simple.c: interaction and relocated SDK six-model packaging")


if __name__ == "__main__":
    main()
