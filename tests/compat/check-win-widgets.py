"""Compare custom-widget traces with caller-supplied HPGCC 1.1/2.0 archives."""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

RELEASES = (
    ("hpgcc1.1-with-39g-support-windows.zip", "sources/win/common/"),
    ("hpgcc-2.0-SP2-Linux.tar.bz2", "hpgcc_20/sources/win/common/"),
)
FILES = (
    "win_add_widget.c",
    "win_remove_widget.c",
    "win_focus_next.c",
    "win_focus_prev.c",
    "win_wcore_alloc.c",
    "win_wcore_get_data.c",
    "win_wcore_notify_focus.c",
    "win_wcore_pref_size.c",
    "win_wcore_paint_base.c",
    "win_widget_destroy.c",
    "win_widget_has_focus.c",
    "win_widget_pack.c",
    "win_widget_get_colors.c",
    "win_widget_set_colors.c",
    "win_widget_get_font.c",
    "win_widget_set_font.c",
    "win_widget_get_location.c",
    "win_widget_set_location.c",
    "win_widget_get_size.c",
    "win_widget_set_size.c",
    "win_widget_set_transparent.c",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path)
    parser.add_argument("--compiler", default="cc")
    parser.add_argument(
        "--candidate", type=Path, help="alternate widget source for negative control"
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    harness = root / "tests/compat/win-widgets.c"
    flags = ["-std=c11", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    with tempfile.TemporaryDirectory(prefix="hpclang-widget-reference-") as temporary:
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
                str(args.candidate or root / "runtime/core/hpgcc-widget.c"),
                str(root / "runtime/core/win-focus.c"),
                "-o",
                str(candidate),
            ],
            check=True,
        )
        actual = subprocess.check_output([str(candidate)])
        for name, prefix in RELEASES:
            reference = work / name
            reference.mkdir()
            shutil.copyfile(root / "tests/compat/win-widget-reference.h", reference / "winpriv.h")
            (reference / "hpstdlib.h").write_text("#include <stdlib.h>\n#include <string.h>\n")
            path = args.archives / name
            with zipfile.ZipFile(path) if name.endswith(".zip") else tarfile.open(path) as archive:
                for filename in FILES:
                    member = prefix + filename
                    if name.endswith(".zip"):
                        data = archive.read(member)
                    else:
                        with archive.extractfile(member) as source:
                            data = source.read()
                    (reference / filename).write_bytes(data)
            executable = reference / "reference"
            subprocess.run(
                [
                    args.compiler,
                    *flags,
                    "-DREFERENCE",
                    "-I",
                    str(root / "include"),
                    "-I",
                    str(reference),
                    "-include",
                    str(reference / "winpriv.h"),
                    str(harness),
                    *(str(reference / filename) for filename in FILES),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            expected = subprocess.check_output([str(executable)])
            if actual != expected:
                raise AssertionError(f"{name}: custom-widget trace differs from candidate")
            print(
                f"PASS {name}: {len(actual.splitlines())} lines, "
                f"sha256={hashlib.sha256(actual).hexdigest()}"
            )


if __name__ == "__main__":
    main()
