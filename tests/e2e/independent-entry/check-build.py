"""Build independent copies of the entry fixture and compare release artifacts."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def check(toolchain):
    source = Path(__file__).resolve().parent
    inputs = {name: source / name for name in ("build.py", "entry.S", "probe.c", "link.ld")}
    inputs["bootstrap-sync.S"] = source.parents[2] / "runtime/bootstrap/sync.S"
    outputs = ("program.HP", "probe.bin", "probe.elf", "metadata.json", "build.json")
    hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in inputs.items()}
    with tempfile.TemporaryDirectory(prefix="hpclang-entry-build-") as temporary:
        root = Path(temporary)
        builds = []
        for name in ("first", "independent copy"):
            copy = root / name / "source"
            copy.mkdir(parents=True)
            for filename in inputs:
                shutil.copyfile(inputs[filename], copy / filename)
            output = copy.parent / "output"
            subprocess.run(
                [
                    sys.executable,
                    str(copy / "build.py"),
                    "--toolchain",
                    str(toolchain),
                    "--bootstrap-sync",
                    str(copy / "bootstrap-sync.S"),
                    "--output",
                    str(output),
                ],
                check=True,
            )
            manifest = json.loads((output / "build.json").read_text())
            if manifest["inputs"] != hashes:
                raise ValueError("build input hashes differ from the maintained source")
            for artifact, digest in manifest["outputs"].items():
                if hashlib.sha256((output / artifact).read_bytes()).hexdigest() != digest:
                    raise ValueError(f"artifact hash mismatch: {artifact}")
            builds.append(output)
        comparisons = {}
        for name in outputs:
            original = (builds[0] / name).read_bytes()
            if original != (builds[1] / name).read_bytes():
                raise ValueError(f"independent build differs: {name}")
            comparisons[name] = hashlib.sha256(original).hexdigest()
        for name, expected in hashes.items():
            if hashlib.sha256(inputs[name].read_bytes()).hexdigest() != expected:
                raise ValueError(f"source changed during check: {name}")
    return dict(
        inputs=hashes,
        identical_outputs=comparisons,
        scope="Build reproducibility only; no emulator or hardware execution.",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check(args.toolchain.resolve()), indent=2))


if __name__ == "__main__":
    main()
