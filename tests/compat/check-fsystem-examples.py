"""Build pinned original HPGCC 2.0 filesystem examples through an installed SDK."""

import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

MEMBERS = {
    "example2": "5a8f551f5dff28fc76997379139e8099370743696fc424e459591d968082a630",
    "example3": "0dd7a9db707d3fc6255c81aac5ce88810f43941a265db28bb5e79b5b9ced8aae",
}
MODELS = ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g")
SHUTDOWN = """error=FSShutdown();
if(error==FS_OK) printf("Shutdown OK\\n");
else printf("Shutdown error: %s", FSGetErrorMsg(error));"""


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"Expected exactly one migration site: {old}")
    return source.replace(old, new)


def extract(archive):
    with tarfile.open(archive) as release:
        for name, expected in MEMBERS.items():
            member = f"hpgcc_20/sources/fsystem/examples/{name}.c"
            with release.extractfile(member) as stream:
                data = stream.read()
            actual = hashlib.sha256(data).hexdigest()
            if actual != expected:
                raise ValueError(f"Unreviewed {member}: SHA256 {actual}")
            original = data.decode("ascii").replace("\r\n", "\n")
            source = replace_once(
                original,
                "#include <hpgcc49.h>",
                "#include <hpconio.h>\n#include <hpkeyb49.h>\n#include <string.h>",
            )
            source = replace_once(source, "clear_screen();", "_clear_screen();")
            source = replace_once(
                source,
                SHUTDOWN,
                "/* hpclang closes the process-owned filesystem at application exit. */",
            )
            if name == "example3":
                source = replace_once(
                    source,
                    'FSWrite("Appended string",17,file)!=17',
                    'FSWrite("Appended string",sizeof("Appended string")-1,file)'
                    '!=sizeof("Appended string")-1',
                )
            yield name, member, original, source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument(
        "--native", action="store_true", help="Also build observed native applications"
    )
    args = parser.parse_args()
    examples = list(extract(args.archive))
    sdk, work = args.sdk.resolve(), args.work.resolve()
    toolchain = sdk / "share/hpclang/cmake/toolchains/hpclang-armv4t.cmake"
    if not toolchain.is_file():
        parser.error(f"Missing installed SDK toolchain: {toolchain}")
    work.mkdir(parents=True, exist_ok=False)
    app = work / "application"
    app.mkdir()
    report = {
        "passed": False,
        "archive_sha256": hashlib.sha256(args.archive.read_bytes()).hexdigest(),
        "sdk": str(sdk),
        "programs": [],
        "limits": [
            "Compile/link/package only; no native file/input/output/return acceptance",
            "SD runtime is available only on HP49g+/HP50g; packaging is not availability",
            "Explicit FSShutdown and its success/error output are removed; exit owns shutdown",
            "Original example3 rename needs a preexisting My own text document.txt fixture",
        ],
    }
    cmake = [
        "cmake_minimum_required(VERSION 3.20)",
        "project(hpgcc_fsystem LANGUAGES C ASM)",
        "find_package(hpclang CONFIG REQUIRED)",
    ]
    for name, member, original, source in examples:
        (work / f"{name}-original.c").write_text(original, encoding="utf-8")
        (app / f"{name}.c").write_text(source, encoding="utf-8")
        (work / f"{name}.diff").write_text(
            "".join(
                difflib.unified_diff(
                    original.splitlines(True),
                    source.splitlines(True),
                    fromfile=f"original/{name}.c",
                    tofile=f"adapted/{name}.c",
                )
            ),
            encoding="utf-8",
        )
        report["programs"].append(
            {
                "name": name,
                "member": member,
                "member_sha256": MEMBERS[name],
                "adapted_sha256": hashlib.sha256(source.encode()).hexdigest(),
            }
        )
        cmake += [f"hpclang_add_application({name} {name}.c)", f"hpclang_target_launchers({name})"]
        if args.native:
            entry = "historical_enum_main" if name == "example2" else "historical_write_main"
            (app / f"{name}-instrumented.c").write_text(
                f'#define main {entry}\n#include "{name}.c"\n', encoding="utf-8"
            )
    if args.native:
        shutil.copy2(Path(__file__).with_name("fsystem-examples-native.c"), app / "native.c")
        cmake += [
            "hpclang_add_application(native example2-instrumented.c example3-instrumented.c native.c)",
            "hpclang_target_launchers(native)",
        ]
        for symbol in (
            "keyb_getkey",
            "keyb_getkeyM",
            "FSWrite",
            "FSClose",
            "FSCloseAndDelete",
            "FSRename",
            "FSGetNextEntry",
        ):
            cmake.append(f"target_link_options(native PRIVATE LINKER:--wrap={symbol})")
    (app / "CMakeLists.txt").write_text("\n".join(cmake) + "\n", encoding="utf-8")
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
                str(app),
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
                raise ValueError(f"Invalid envelope: {name}/{model}")
        program["hpc_sha256"] = hashlib.sha256(payload).hexdigest()
        program["packaged_models"] = list(MODELS)
    if args.native:
        artifact = work / "native-artifact"
        artifact.mkdir()
        for suffix in ("elf", "hpc"):
            shutil.copy2(build / f"native.{suffix}", artifact / f"e2e.{suffix}")
        shutil.copytree(build / "native-launchers", artifact / "launchers")
        symbols = subprocess.check_output(
            [str(Path(env["ATFE"]) / "bin/llvm-nm"), str(artifact / "e2e.elf")], text=True
        )
        addresses = [
            line.split()[0]
            for line in symbols.splitlines()
            if line.split()[-1] == "hpclang_e2e_result"
        ]
        if len(addresses) != 1:
            raise ValueError("Missing observation symbol")
        (artifact / "e2e.result-address").write_text("0x" + addresses[0] + "\n", encoding="utf-8")
        (artifact / "e2e.stock-media.json").write_text(
            json.dumps(
                {
                    "version": 1,
                    "initial_files": {"KEEP.TXT": "RETAINED\n"},
                    "expected_files": {
                        "RESULT.TXT": "ORIGINAL-FSYSTEM-PASS\n",
                        "CAPTURE.BIN": "Hello HP49G+\nAppended string",
                    },
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        report["native_artifact"] = str(artifact)
    report["passed"] = True
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("PASS original HPGCC2 filesystem examples: installed SDK build and six-model packages")


if __name__ == "__main__":
    main()
