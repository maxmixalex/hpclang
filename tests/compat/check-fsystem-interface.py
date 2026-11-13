"""Inventory pinned original filesystem declarations; not ABI/semantic proof."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import tarfile


RELEASES = {
    "2.0": (
        "hpgcc_20/include/fsystem.h",
        "e02c08e2715dd3dd813f691b8603bbd63e088ac6d9be580d455e5f3afd48fd17",
        41,
    ),
    "3": (
        "libs_workspace/headers/fsystem.h",
        "8a16b096187c56b214f508206646ab8c9716797602db638f84dd0c5fa22b029e",
        43,
    ),
}
INTERNAL = {"FSSleep", "FSWakeUp"}


def clean(source):
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*|^[ \t]*#[^\n]*", "", source, flags=re.MULTILINE)


def names(source, ending):
    return set(re.findall(r"\b(FS[A-Za-z0-9_]+)\s*\([^;{}]*\)\s*" + ending, clean(source)))


def audit(release, archive, root):
    member, expected, count = RELEASES[release]
    with tarfile.open(archive) as package:
        with package.extractfile(member) as stream:
            original = stream.read()
    if hashlib.sha256(original).hexdigest() != expected:
        raise ValueError("unreviewed original header hash")
    declared = names(original.decode("latin1"), ";")
    if len(declared) != count or not INTERNAL <= declared:
        raise ValueError("original declaration inventory changed")
    header = (root / "include/fsystem.h").read_text(encoding="utf-8")
    implementation = (root / "runtime/libc/hpgcc-fsystem.c").read_text(encoding="utf-8")
    exposed = names(header, ";")
    defined = names(implementation, r"\{")
    required = declared - INTERNAL
    missing = sorted(required - (exposed & defined))
    if missing:
        raise ValueError("missing maintained adapters: " + ", ".join(missing))
    if INTERNAL & exposed:
        raise ValueError("internal controller operations unexpectedly exposed")
    return dict(
        release=release,
        member=member,
        original_sha256=expected,
        declarations=count,
        adapters=sorted(required),
        internal=sorted(INTERNAL),
        scope="Lexical declaration/definition inventory only; not signatures, ABI, semantics or hardware acceptance.",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", choices=RELEASES, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    try:
        result = audit(args.release, args.archive, args.root)
    except (OSError, KeyError, ValueError, tarfile.TarError) as error:
        parser.exit(1, f"filesystem interface audit failed: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
