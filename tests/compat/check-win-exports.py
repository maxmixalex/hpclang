"""Link every original window function through an installed SDK in C and C++."""

import argparse
import json
import os
from pathlib import Path
import runpy
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    audit = runpy.run_path(str(root / "tools/audit-hpgcc-windows.py"))
    report = audit["audit"](args.archives)
    left, right = (report["releases"][name]["window_functions"] for name in audit["ARCHIVES"][1:3])
    if left != right:
        raise SystemExit("Original 1.1/2.0 function inventories differ; review before probing")
    # The original checkbox header contains a nonexistent text getter. Use its
    # real implementation names. The declared disabled-color setter is intended
    # API despite the original source defining the ordinary setter twice.
    if left["header_only"] != ["win_softmenu_set_disable_colors", "win_text_is_selected"]:
        raise SystemExit("Original header discrepancies changed; review required")
    symbols = sorted(set(left["definitions"]) | {"win_softmenu_set_disable_colors"})
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    sdk = args.sdk.resolve()
    source = "#include <win.h>\n#include <wincore.h>\n"
    source += "".join(
        f"__typeof__(&{symbol}) volatile probe_{symbol} = &{symbol};\n" for symbol in symbols
    )
    source += "int main(void) {\n    int missing = 0;\n"
    source += "".join(f"    missing += probe_{symbol} == 0;\n" for symbol in symbols)
    source += "    return missing;\n}\n"
    for suffix in ("c", "cpp"):
        (work / f"probe.{suffix}").write_text(source, encoding="utf-8")
    (work / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(window_exports LANGUAGES C CXX ASM)\n"
        "find_package(hpclang CONFIG REQUIRED)\n"
        "hpclang_add_application(window_c probe.c)\n"
        "hpclang_add_application(window_cpp probe.cpp)\n",
        encoding="utf-8",
    )
    env = dict(os.environ)
    env["PATH"] = str(Path(env["ATFE"]) / "bin") + os.pathsep + env["PATH"]
    build = work / "build"
    subprocess.run(
        [
            "cmake",
            "-S",
            str(work),
            "-B",
            str(build),
            "-DCMAKE_BUILD_TYPE=MinSizeRel",
            f"-DCMAKE_PREFIX_PATH={sdk}",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk}/share/hpclang/cmake/toolchains/hpclang-armv4t.cmake",
        ],
        check=True,
        env=env,
        timeout=600,
    )
    subprocess.run(
        ["cmake", "--build", str(build), "--parallel", "2"],
        check=True,
        env=env,
        timeout=600,
    )
    for target in ("window_c", "window_cpp"):
        output = subprocess.check_output(
            [
                str(Path(env["ATFE"]) / "bin/llvm-nm"),
                "--defined-only",
                str(build / f"{target}.elf"),
            ],
            text=True,
        )
        linked = {line.split()[-1] for line in output.splitlines() if line.split()}
        missing = set(symbols) - linked
        if missing:
            raise SystemExit(f"{target}: missing linked functions: {sorted(missing)}")
    report["sdk_export_probe"] = {
        "symbols": symbols,
        "passed": ["C declarations and ARM link", "C++ declarations and ARM link"],
        "scope": "Name availability/linkage only; no behavioral or binary ABI proof",
    }
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS installed SDK: {len(symbols)} original/repaired window names in C and C++")


if __name__ == "__main__":
    main()
