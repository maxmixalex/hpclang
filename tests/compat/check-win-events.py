"""Compare dispatch traces with caller-supplied HPGCC 1.1/2.0 sources on a POSIX host."""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


FILES = ("win_add_event_handler.c", "win_remove_event_handler.c", "win_post_event.c")
TIMER_FILES = (
    "win_add_timer.c",
    "win_remove_timer.c",
    "_win_push_timer.c",
    "_win_timer_generator.c",
    "win_add_timer_event.c",
    "win_remove_timer_event.c",
    "_win_timer_event.c",
)
RELEASES = (
    ("hpgcc1.1-with-39g-support-windows.zip", "sources/win/common/"),
    ("hpgcc-2.0-SP2-Linux.tar.bz2", "hpgcc_20/sources/win/common/"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path)
    parser.add_argument("--compiler", default="cc")
    parser.add_argument("--candidate", type=Path, help="alternate dispatcher for negative controls")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    harness = root / "tests/compat/win-events.c"
    candidate = args.candidate or root / "runtime/core/win-events.c"
    flags = ["-std=c11", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    with tempfile.TemporaryDirectory(prefix="hpclang-win-reference-") as temporary:
        work = Path(temporary)
        executable = work / "candidate"
        subprocess.run(
            [
                args.compiler,
                *flags,
                "-I",
                str(root / "runtime/core"),
                str(harness),
                str(candidate),
                "-o",
                str(executable),
            ],
            check=True,
        )
        actual = subprocess.check_output([str(executable)])
        for name, prefix in RELEASES:
            reference = work / name
            reference.mkdir()
            shutil.copyfile(root / "tests/compat/win-event-reference.h", reference / "winpriv.h")
            (reference / "hpstdlib.h").write_text("#include <stdlib.h>\n")
            (reference / "hpkeyb49.h").write_text(
                "/* No keyboard services used by this probe. */\n"
            )
            (reference / "hpsys.h").write_text(
                "#ifndef HOST_HPSYS_H\n#define HOST_HPSYS_H\n#include <stdint.h>\n"
                "typedef uint64_t ULONGLONG;\n"
                "typedef struct { ULONGLONG current; } sys_timer_t;\n"
                "#define SYS_TIMER_INITIALIZER {0}\n"
                "void sys_updateTimer(sys_timer_t *);\n#endif\n"
            )
            path = args.archives / name
            with zipfile.ZipFile(path) if name.endswith(".zip") else tarfile.open(path) as archive:
                for filename in (*FILES, *TIMER_FILES):
                    member = prefix + filename
                    if name.endswith(".zip"):
                        data = archive.read(member)
                    else:
                        with archive.extractfile(member) as source:
                            data = source.read()
                    text = data.decode().replace("\r\n", "\n")
                    if filename == "win_post_event.c":
                        declaration = "extern void *alloca(int size);"
                        if text.count(declaration) != 1:
                            raise AssertionError("Historical alloca declaration changed")
                        text = text.replace(declaration, "#include <alloca.h>")
                    (reference / filename).write_text(text)
            executable = reference / "reference"
            subprocess.run(
                [
                    args.compiler,
                    *flags,
                    "-DREFERENCE",
                    "-I",
                    str(reference),
                    str(harness),
                    *(str(reference / filename) for filename in FILES),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            expected = subprocess.check_output([str(executable)])
            if actual != expected:
                raise AssertionError(f"{name}: dispatch trace differs from candidate")
            print(
                f"PASS {name}: {len(actual.splitlines())} trace lines, "
                f"sha256={hashlib.sha256(actual).hexdigest()}"
            )
            timer_executable = reference / "timers"
            subprocess.run(
                [
                    args.compiler,
                    *flags,
                    "-I",
                    str(reference),
                    str(root / "tests/compat/win-timers.c"),
                    *(str(reference / filename) for filename in (*FILES, *TIMER_FILES)),
                    "-o",
                    str(timer_executable),
                ],
                check=True,
            )
            timer_trace = subprocess.check_output([str(timer_executable)])
            print(
                f"PASS {name}: eight original timer scenarios, "
                f"sha256={hashlib.sha256(timer_trace).hexdigest()}"
            )


if __name__ == "__main__":
    main()
