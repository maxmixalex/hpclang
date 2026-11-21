"""Boot HP48gII Stock from NOR, transfer the independent entry and verify its return."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
E2E = HERE.parent
sys.path.insert(0, str(E2E / "port-recovery"))
from run import completed, emulator, transfer  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("nor", "dist", "toolchain", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True)
    profile = json.loads((E2E / "targets/hp48gii.json").read_text())
    spec = SimpleNamespace(
        model="hp48gii", profile=profile, dist=args.dist.resolve(), flash_busy_reads=0
    )
    state = out / "clean/state"
    artifact = out / "artifact"
    subprocess.run(
        [sys.executable, E2E.parents[1] / "tools/prepare-hp48gii-nor.py", args.nor, state],
        check=True,
    )
    subprocess.run(
        [
            sys.executable,
            HERE / "build.py",
            "--model",
            spec.model,
            "--toolchain",
            args.toolchain.resolve(),
            "--output",
            artifact,
        ],
        check=True,
    )
    with emulator(
        spec, out / "clean", "2000 tap f6\n4000 tap f6\n7000 capture home\n9000 finish-install\n"
    ) as child:
        completed(child, 90)
    transfer(spec, out / "run", state, artifact, "ENTRY48", package_name="program.HP")
    for script, report in (("verify.py", "report.json"), ("check-snapshot.py", "corruptions.json")):
        subprocess.run(
            [
                sys.executable,
                HERE / script,
                "--artifact",
                artifact,
                "--baseline",
                state,
                "--state",
                out / "run/state",
                "--report",
                out / report,
            ],
            check=True,
        )


if __name__ == "__main__":
    main()
