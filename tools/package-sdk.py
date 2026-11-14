#!/usr/bin/env python3
"""Create a deterministic distribution ZIP from an installed source SDK."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import zipfile


MANIFEST = "hpclang-sdk-manifest.json"


def package(prefix, output):
    prefix, output = Path(prefix).resolve(), Path(output).resolve()
    source = prefix / "share/hpclang"
    if output.is_relative_to(source):
        raise ValueError("archive must be outside the SDK source directory")
    for required in ("LICENSE", "cmake/hpclangConfig.cmake", "include/hpclang/abi.h"):
        if not (source / required).is_file():
            raise ValueError("incomplete installed SDK: " + required)
    files = []
    for directory, directories, names in os.walk(source, followlinks=False):
        for name in directories + names:
            path = Path(directory) / name
            if path.is_symlink():
                raise ValueError("installed SDK contains a symbolic link: " + str(path))
        files.extend(Path(directory) / name for name in names)
    entries = {}
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
        # Stored entries avoid dependence on a particular zlib implementation.
        # The font source/license archive is already compressed.
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED) as archive:

            def write(name, data, mode):
                info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (0o100000 | mode) << 16
                archive.writestr(info, data)

            for path in sorted(files):
                if not path.is_file():
                    raise ValueError("installed SDK contains a non-file: " + str(path))
                name = path.relative_to(prefix).as_posix()
                data = path.read_bytes()
                mode = 0o755 if data.startswith(b"#!") or path.suffix == ".sh" else 0o644
                entries[name] = dict(
                    size=len(data), sha256=hashlib.sha256(data).hexdigest(), mode=mode
                )
                write(name, data, mode)
            manifest = json.dumps(dict(version=1, files=entries), sort_keys=True, indent=2) + "\n"
            write(MANIFEST, manifest.encode("utf-8"), 0o644)
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return dict(files=len(entries), sha256=hashlib.sha256(output.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = package(args.prefix, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"SDK packaging failed: {error}\n")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
