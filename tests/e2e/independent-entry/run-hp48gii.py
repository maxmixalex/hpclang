"""Boot HP48gII Stock from NOR, transfer the independent entry and verify its return."""

import argparse
import json
import runpy
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
    parser.add_argument(
        "--fixture", choices=("independent-entry", "independent-hpc1"), default="independent-entry"
    )
    parser.add_argument("--notebook", type=Path, help="run an installed-SDK HP48gII Notebook image")
    args = parser.parse_args()
    if args.notebook and args.fixture != "independent-hpc1":
        parser.error("--notebook requires --fixture independent-hpc1")
    fixture = E2E / args.fixture
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
            fixture / "build.py",
            "--model",
            spec.model,
            "--toolchain",
            args.toolchain.resolve(),
            "--output",
            artifact,
            *(["--notebook", args.notebook.resolve()] if args.notebook else []),
        ],
        check=True,
    )
    with emulator(
        spec, out / "clean", "2000 tap f6\n4000 tap f6\n7000 capture home\n9000 finish-install\n"
    ) as child:
        completed(child, 90)
    actions = ""
    if args.notebook:
        actions = (
            "3500 capture editor\n4300 tap two\n4900 tap zero\n6000 capture typed\n"
            "8500 tap on\n14000 capture returned\n"
        )
    transfer(
        spec, out / "run", state, artifact, "ENTRY48", package_name="program.HP", actions=actions
    )
    if args.notebook:
        ui = runpy.run_path(str(E2E / "notebook/verify.py"))
        font = ui["font_rows"](E2E.parents[1] / "runtime/fonts/generated/newrpl_6a.S")
        for name, text in (("editor", ""), ("typed", "20")):
            path = out / "run" / f"stock-{name}.pgm"
            magic, dims, limit, pixels = path.read_bytes().split(b"\n", 3)
            width, height = map(int, dims.split())
            if (
                magic != b"P5"
                or limit != b"255"
                or width not in (131, 132)
                or height != 64
                or len(pixels) != width * height
            ):
                raise ValueError(f"unexpected HP48gII display capture: {path}")
            pixels = b"".join(pixels[row * width : row * width + 131] for row in range(height))
            ui["check_line"](pixels, font, 0, "Notebook", path)
            ui["check_line"](pixels, font, 24, text, path, focused=True)
        returned = (out / "run/stock-returned.pgm").read_bytes()
        if returned != (out / "run/stock-checkpoint.pgm").read_bytes():
            raise ValueError("Stock display did not settle after Notebook exit")
    for script, report in (("verify.py", "report.json"), ("check-snapshot.py", "corruptions.json")):
        subprocess.run(
            [
                sys.executable,
                fixture / script,
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
