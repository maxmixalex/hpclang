"""Compare bounded HPC1 release artifacts from two isolated source copies."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import shutil
import subprocess
import sys
import tempfile


def check(toolchain):
    source = Path(__file__).resolve().parents[3]
    fixture = runpy.run_path(str(Path(__file__).with_name("build.py")))
    inputs = {
        name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in fixture["INPUTS"]
    }
    with tempfile.TemporaryDirectory(prefix="hpclang-hpc1-") as temporary:
        outputs = []
        for name in ("first", "independent copy"):
            root = Path(temporary) / name
            for filename in inputs:
                target = root / "source" / filename
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source / filename, target)
            output = root / "output"
            subprocess.run(
                [
                    sys.executable,
                    str(root / "source" / fixture["FIXTURE"] / "build.py"),
                    "--toolchain",
                    str(toolchain),
                    "--output",
                    str(output),
                ],
                check=True,
            )
            manifest = json.loads((output / "build.json").read_text())
            if manifest["inputs"] != inputs:
                raise ValueError("isolated source hashes differ")
            for artifact, expected in manifest["outputs"].items():
                if hashlib.sha256((output / artifact).read_bytes()).hexdigest() != expected:
                    raise ValueError("artifact hash mismatch: " + artifact)
            outputs.append(output)
        hashes = {}
        for name in (*fixture["OUTPUTS"], "build.json"):
            data = (outputs[0] / name).read_bytes()
            if data != (outputs[1] / name).read_bytes():
                raise ValueError("independent artifact differs: " + name)
            hashes[name] = hashlib.sha256(data).hexdigest()
    if any(
        hashlib.sha256((source / name).read_bytes()).hexdigest() != expected
        for name, expected in inputs.items()
    ):
        raise ValueError("maintained inputs changed during check")
    return dict(
        inputs=inputs,
        identical_outputs=hashes,
        scope="Build reproducibility only; no native execution or service qualification",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check(args.toolchain.resolve()), indent=2))


if __name__ == "__main__":
    main()
