"""Independently inspect an original-filesystem-example Stock FAT card with mtools."""

import argparse
from pathlib import Path
import subprocess


def verify(image):
    expected = {
        "KEEP.TXT": b"RETAINED\n",
        "RESULT.TXT": b"ORIGINAL-FSYSTEM-PASS\n",
        "CAPTURE.BIN": b"Hello HP49G+\nAppended string",
        "Another document.bin": b"RENAME-SENTINEL\n",
    }
    expected.update({f"SEED{i}.TXT": b"ENTRY\n" for i in range(6)})
    for name, contents in expected.items():
        actual = subprocess.check_output(["mtype", "-i", str(image), f"::/{name}"])
        if actual != contents:
            raise ValueError(f"{name}: raw FAT contents differ")
    subprocess.run(["mdir", "-i", str(image), "::/Test Dir"], check=True, capture_output=True)
    for name in ("Test file 1", "Subdir", "My own text document.txt"):
        result = subprocess.run(
            ["mdir", "-b", "-i", str(image), f"::/Test Dir/{name}"],
            capture_output=True,
            check=False,
        )
        if result.returncode == 0:
            raise ValueError(f"{name}: deleted/moved entry still exists")
    print("PASS raw FAT original-example contents, rename, deletion and retained fixtures")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    verify(parser.parse_args().image)
