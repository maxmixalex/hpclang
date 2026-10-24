#!/usr/bin/env python3
"""Compare window APIs in four caller-supplied HPGCC source distributions."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import tarfile
import zipfile


ARCHIVES = (
    "hpgcc-1.0.3-beta-Linux.tar.bz2",
    "hpgcc1.1-with-39g-support-windows.zip",
    "hpgcc-2.0-SP2-Linux.tar.bz2",
    "hpgcc3_R004.revA.tar.gz",
)
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"}


def sources(path):
    """Read source members without extracting archive paths onto the filesystem."""
    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            for member in archive.infolist():
                if not member.is_dir() and PurePosixPath(member.filename).suffix in SOURCE_SUFFIXES:
                    yield member.filename, archive.read(member)
    else:
        with tarfile.open(path) as archive:
            for member in archive:
                if member.isfile() and PurePosixPath(member.name).suffix in SOURCE_SUFFIXES:
                    with archive.extractfile(member) as source:
                        yield member.name, source.read()


def compare(left, right):
    shared = left.keys() & right.keys()
    return {
        "shared_files": len(shared),
        "only_left": sorted(left.keys() - right.keys()),
        "only_right": sorted(right.keys() - left.keys()),
        "different_after_newline_normalization": sorted(
            name for name in shared if left[name] != right[name]
        ),
    }


def window_functions(tree, headers):
    """Inventory ordinary C declarations/definitions in the historical sources.

    This is a lexical inventory, not preprocessing, type checking or ABI proof.
    Keep paths so header typos and duplicate definitions remain visible.
    """

    def scan(files, suffix, terminator):
        found = {}
        pattern = re.compile(r"(?m)^[A-Za-z_][\w\s*]*?\b(win_\w+)\s*\([^;{}]*\)\s*" + terminator)
        for name, data in sorted(files.items()):
            if not name.endswith(suffix):
                continue
            source = data.decode("latin1")
            source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
            for symbol in pattern.findall(source):
                found.setdefault(symbol, []).append(name)
        return dict(sorted(found.items()))

    definitions = scan(tree, ".c", r"\{")
    declarations = scan(headers, ".h", ";")
    return {
        "definition_names": len(definitions),
        "declaration_names": len(declarations),
        "definitions": definitions,
        "declarations": declarations,
        "source_only": sorted(definitions.keys() - declarations.keys()),
        "header_only": sorted(declarations.keys() - definitions.keys()),
        "duplicate_definitions": {
            name: paths for name, paths in definitions.items() if len(paths) > 1
        },
    }


def audit(directory):
    releases = {}
    trees = {}
    headers = {}
    for name in ARCHIVES:
        path = directory / name
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        reference_files = []
        gui_headers = []
        count = 0
        tree = {}
        installed = {}
        for filename, data in sources(path):
            count += 1
            normalized = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
            parts = PurePosixPath(filename).parts
            if re.search(rb"\bwin_[A-Za-z0-9_]+", data):
                reference_files.append(filename)
            relative = "/" + filename
            if "/sources/win/" in relative:
                tree[relative.split("/sources/win/", 1)[1]] = normalized
            if len(parts) >= 2 and parts[-2] == "include" and parts[-1].startswith("win"):
                installed[parts[-1]] = normalized
            if filename.endswith("/headers/gui.h"):
                gui_headers.append(filename)
        releases[name] = {
            "sha256": digest,
            "source_and_header_files": count,
            "win_reference_files": sorted(reference_files),
            "window_source_files": len(tree),
            "installed_window_headers": sorted(installed),
            "distinct_gui_headers": sorted(gui_headers),
            "window_functions": window_functions(tree, installed),
        }
        trees[name] = tree
        headers[name] = installed
    return {
        "version": 2,
        "scope": "Static source/header evidence; references include comments; no runtime proof",
        "releases": releases,
        "window_sources_1_1_vs_2_0": compare(trees[ARCHIVES[1]], trees[ARCHIVES[2]]),
        "installed_headers_1_1_vs_2_0": compare(headers[ARCHIVES[1]], headers[ARCHIVES[2]]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "archives", type=Path, help="directory containing the four original archives"
    )
    args = parser.parse_args()
    print(json.dumps(audit(args.archives), indent=2))


if __name__ == "__main__":
    main()
