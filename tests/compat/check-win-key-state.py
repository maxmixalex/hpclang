"""Compare typed-key/modifier traces with caller-supplied HPGCC releases."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import tarfile
import tempfile
import zipfile

RELEASES = (
    ("hpgcc1.1-with-39g-support-windows.zip", "sources/win/"),
    ("hpgcc-2.0-SP2-Linux.tar.bz2", "hpgcc_20/sources/win/"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path)
    parser.add_argument("--compiler", default="cc")
    parser.add_argument("--candidate", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    harness = root / "tests/compat/win-key-state.c"
    flags = ["-std=c11", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    with tempfile.TemporaryDirectory(prefix="hpclang-key-reference-") as temporary:
        work = Path(temporary)
        candidate = work / "candidate"
        subprocess.run(
            [
                args.compiler,
                *flags,
                "-I",
                str(root / "include"),
                "-I",
                str(root / "runtime/core"),
                str(harness),
                str(args.candidate or root / "runtime/core/win-key-state.c"),
                "-o",
                str(candidate),
            ],
            check=True,
        )
        actual = subprocess.check_output([str(candidate)])
        for name, prefix in RELEASES:
            reference = work / name
            reference.mkdir()
            path = args.archives / name
            with zipfile.ZipFile(path) if name.endswith(".zip") else tarfile.open(path) as archive:
                for member in ("common/_win_default_key_handler.c", "winkeys.h"):
                    if name.endswith(".zip"):
                        data = archive.read(prefix + member)
                    else:
                        with archive.extractfile(prefix + member) as stream:
                            data = stream.read()
                    source = data.decode().replace("\r\n", "\n")
                    if member.endswith(".c"):
                        declaration = "extern void *alloca(int size);"
                        if source.count(declaration) != 1:
                            raise AssertionError("Historical alloca declaration changed")
                        source = source.replace(declaration, "#include <alloca.h>")
                    (reference / Path(member).name).write_text(source)
            (reference / "hpstdlib.h").write_text("#include <stdlib.h>\n")
            (reference / "hpgraphics.h").write_text(
                "/* No graphics calls in the state machine. */\n"
            )
            (reference / "winpriv.h").write_text(
                '#include "winkeys.h"\n#define WIN_REPAINT_EVENT 7\n'
                "int win_post_event(int, void *);\n"
            )
            executable = reference / "reference"
            subprocess.run(
                [
                    args.compiler,
                    *flags,
                    "-DREFERENCE",
                    "-Wno-pointer-to-int-cast",
                    "-Wno-int-to-pointer-cast",
                    "-I",
                    str(reference),
                    "-I",
                    str(root / "include"),
                    str(harness),
                    str(reference / "_win_default_key_handler.c"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            expected = subprocess.check_output([str(executable)])
            if actual != expected:
                first = next(
                    (
                        i
                        for i, (a, b) in enumerate(zip(actual.splitlines(), expected.splitlines()))
                        if a != b
                    ),
                    None,
                )
                raise AssertionError(f"{name}: key trace mismatch at line {first}")
            print(
                f"PASS {name}: {len(actual.splitlines())} trace lines, sha256={hashlib.sha256(actual).hexdigest()}"
            )


if __name__ == "__main__":
    main()
