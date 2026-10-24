"""Compare original soft menu controls over the same production window foundation."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import tarfile
import tempfile
import zipfile

RELEASES = (
    ("hpgcc1.1-with-39g-support-windows.zip", "sources/win/softmenu/"),
    ("hpgcc-2.0-SP2-Linux.tar.bz2", "hpgcc_20/sources/win/softmenu/"),
)
FILES = (
    "win_softmenu_new.c",
    "win_softmenu_set_item.c",
    "win_softmenu_add_item.c",
    "win_softmenu_insert_item.c",
    "win_softmenu_delete_item.c",
    "win_softmenu_set_colors.c",
    "win_softmenu_set_disable_colors.c",
    "win_softmenu_set_enabled.c",
)
FOUNDATION = (
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path)
    parser.add_argument("--compiler", default="cc")
    parser.add_argument("--candidate", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    common = [
        args.compiler,
        "-std=c11",
        "-funsigned-char",
        "-O1",
        "-g",
        "-fsanitize=address,undefined",
        "-fno-omit-frame-pointer",
        "-Wno-pointer-to-int-cast",
        "-I",
        str(root / "include"),
        "-I",
        str(root / "runtime/core"),
        str(root / "tests/compat/win-softmenu.c"),
        *(str(root / f"runtime/core/{name}.c") for name in FOUNDATION),
    ]
    with tempfile.TemporaryDirectory(prefix="hpclang-menu-reference-") as temporary:
        work = Path(temporary)
        executable = work / "candidate"
        subprocess.run(
            [
                *common,
                str(args.candidate or root / "runtime/core/hpgcc-softmenu.c"),
                "-o",
                str(executable),
            ],
            check=True,
        )
        actual = subprocess.check_output([str(executable)])
        for name, prefix in RELEASES:
            reference = work / name
            reference.mkdir()
            for filename, standard in (
                ("hpstdlib.h", "stdlib.h"),
                ("hpstdio.h", "stdio.h"),
                ("hpstring.h", "string.h"),
            ):
                (reference / filename).write_text(f"#include <{standard}>\n")
            (reference / "win.h").write_text(
                "#include <wincore.h>\n#include <winkeys.h>\n#include <wintimer.h>\n"
                "#include <winsoftmenu.h>\n"
            )
            path = args.archives / name
            with zipfile.ZipFile(path) if name.endswith(".zip") else tarfile.open(path) as archive:
                for filename in (*FILES, "softmenupriv.h"):
                    if name.endswith(".zip"):
                        data = archive.read(prefix + filename)
                    else:
                        with archive.extractfile(prefix + filename) as source:
                            data = source.read()
                    # The historical disabled-color TU accidentally exports the
                    # ordinary-color name. Rename only that symbol to link both
                    # functions; preserve its executable implementation.
                    if filename == "win_softmenu_set_disable_colors.c":
                        assert data.count(b"void win_softmenu_set_colors(") == 1
                        data = data.replace(
                            b"void win_softmenu_set_colors(",
                            b"void win_softmenu_set_disable_colors(",
                        )
                    if filename == "win_softmenu_new.c":
                        # Match the original stack-allocation lifetime using the
                        # host compiler intrinsic instead of an external symbol.
                        assert data.count(b"extern void *alloca(int size);") == 1
                        data = data.replace(
                            b"extern void *alloca(int size);", b"#include <alloca.h>"
                        )
                    (reference / filename).write_bytes(data)
            executable = reference / "reference"
            # Put host libc shims before the installed historical aliases.
            subprocess.run(
                [
                    args.compiler,
                    "-I",
                    str(reference),
                    *common[1:],
                    *(str(reference / filename) for filename in FILES),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            expected = subprocess.check_output([str(executable)])
            if actual != expected:
                for i, (a, b) in enumerate(zip(actual.splitlines(), expected.splitlines())):
                    if a != b:
                        raise AssertionError(f"{name}: menu trace line {i + 1}: {a!r} != {b!r}")
                raise AssertionError(f"{name}: menu trace lengths differ")
            print(
                f"PASS {name}: {len(actual.splitlines())} lines, "
                f"sha256={hashlib.sha256(actual).hexdigest()}"
            )


if __name__ == "__main__":
    main()
