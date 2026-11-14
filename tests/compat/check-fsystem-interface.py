"""Check pinned filesystem declarations and optional C/C++ source signatures."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
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


def check_signatures(original, required, root, cc, cxx):
    declarations = {}
    pattern = r"\b(?:extern\s+)?(?:int|void|char\s*\*)\s*(FS[A-Za-z0-9_]+)\s*\([^;{}]*\)\s*;"
    for match in re.finditer(pattern, clean(original)):
        name = match.group(1)
        if name in required:
            if name in declarations:
                raise ValueError("duplicate original declaration: " + name)
            declarations[name] = match.group(0)
    if set(declarations) != required:
        raise ValueError("unsupported original declaration syntax")
    source = '#include <fsystem.h>\n#ifdef __cplusplus\nextern "C" {\n#endif\n'
    source += "\n".join(declarations[name] for name in sorted(required))
    source += "\n#ifdef __cplusplus\n}\n#endif\n"
    for compiler, language, standard in ((cc, "c", "c11"), (cxx, "c++", "c++17")):
        result = subprocess.run(
            [
                compiler,
                "-x",
                language,
                "-std=" + standard,
                "-Werror",
                "-fsyntax-only",
                "-I",
                str(root / "include"),
                "-",
            ],
            input=source,
            text=True,
            capture_output=True,
        )
        if result.returncode:
            raise ValueError(language + " original declaration conflict:\n" + result.stderr)
    return dict(
        declarations=len(declarations),
        languages=["C11", "C++17"],
        scope="Original function redeclarations compile with the adapter header; not binary layout or semantics.",
    )


def audit(release, archive, root, cc=None, cxx=None):
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
    result = dict(
        release=release,
        member=member,
        original_sha256=expected,
        declarations=count,
        adapters=sorted(required),
        internal=sorted(INTERNAL),
        scope="Lexical declaration/definition inventory only; not signatures, ABI, semantics or hardware acceptance.",
    )
    if bool(cc) != bool(cxx):
        raise ValueError("provide both --cc and --cxx for declaration checks")
    if cc:
        result["signatures"] = check_signatures(original.decode("latin1"), required, root, cc, cxx)
        result["scope"] = (
            "Declaration/definition inventory and C/C++ source signature compatibility; not ABI, semantics or hardware acceptance."
        )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", choices=RELEASES, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--cc", help="C compiler executable for optional source signature checks")
    parser.add_argument("--cxx", help="C++ compiler executable; required with --cc")
    args = parser.parse_args()
    try:
        result = audit(args.release, args.archive, args.root, args.cc, args.cxx)
    except (OSError, KeyError, ValueError, tarfile.TarError) as error:
        parser.exit(1, f"filesystem interface audit failed: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
