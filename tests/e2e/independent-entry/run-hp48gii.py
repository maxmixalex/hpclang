"""Boot HP48gII Stock from NOR, transfer the independent entry and verify its return."""

import argparse
import json
import runpy
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
E2E = HERE.parent
sys.path.insert(0, str(E2E / "port-recovery"))
from run import completed, emulator, transfer  # noqa: E402
from verify import home_string  # noqa: E402


def check_editor(phase, initial, *, repeat_save=False):
    ui = runpy.run_path(str(E2E / "notebook/verify.py"))
    font = ui["font_rows"](E2E.parents[1] / "runtime/fonts/generated/newrpl_6a.S")
    frames = [("editor", initial, True), ("typed", initial + "20", True)]
    if repeat_save:
        frames.extend(
            [("first-save", initial + "20", False), ("second-save", initial + "202", False)]
        )
    for name, text, focused in frames:
        path = phase / f"stock-{name}.pgm"
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
        ui["check_line"](pixels, font, 24, text, path, focused=focused)
        if not focused:
            ui["check_line"](pixels, font, 32, "Saved; exit normally", path)
    returned = (phase / "stock-returned.pgm").read_bytes()
    if returned != (phase / "stock-checkpoint.pgm").read_bytes():
        raise ValueError("Stock display did not settle after Notebook exit")


def check_home_note(state, expected):
    entry = runpy.run_path(str(HERE / "verify.py"))
    _, read, _ = entry["no_toolbox"](
        (state / "sram").read_bytes(), (state / "flash").read_bytes(), "hp48gii"
    )
    if home_string(read, b"HPCNOTE") != expected:
        raise ValueError("unexpected HOME note after Notebook exit")


def stock_workspace(samples, out, port0):
    verifier = runpy.run_path(str(HERE / "verify.py"))
    resources = runpy.run_path(str(E2E / "harness/verify-resource-snapshots.py"))
    flash = (out / "clean/state/flash").read_bytes()
    gaps = {}
    for launch in ("run", "resume"):
        phases = ["deposited", "editor", "typed"]
        if port0 and launch == "resume":
            phases.extend(("first-save", "second-save"))
        phases.append("returned")
        gaps[launch] = {}
        for phase in phases:
            prefix = samples / f"{launch}-{phase}"
            ram = prefix.with_suffix(".sram").read_bytes()
            meta = json.loads(prefix.with_suffix(".json").read_text())
            resources["metadata"](meta, resources["MODELS"]["hp48gii"], phase, len(ram))
            _, word = verifier["mapping"](ram, flash, "hp48gii")
            rstk, dstk = word(0x806F3), word(0x806F8)
            if not 0x80000 <= rstk <= dstk < 0x100000:
                raise ValueError(f"{launch}/{phase}: invalid Stock stack bounds")
            gap = dstk - rstk
            if word(0x80E9B) != gap // 5:
                raise ValueError(f"{launch}/{phase}: AVMEM differs from the Stock stack gap")
            gaps[launch][phase] = gap
    (out / "stock-workspace.json").write_text(
        json.dumps({"stack_gap_nibbles": gaps}, indent=2) + "\n"
    )


def main(samples):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("nor", "dist", "toolchain", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument(
        "--fixture", choices=("independent-entry", "independent-hpc1"), default="independent-entry"
    )
    parser.add_argument("--notebook", type=Path, help="run an installed-SDK HP48gII Notebook image")
    parser.add_argument(
        "--port0", action="store_true", help="install and recall Notebook in Port 0"
    )
    parser.add_argument(
        "--preserve-stack", action="store_true", help="verify preservation of user stack values"
    )
    args = parser.parse_args()
    if args.preserve_stack and not args.notebook:
        parser.error("--preserve-stack requires --notebook")
    if args.port0 and not args.notebook:
        parser.error("--port0 requires --notebook")
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
            *(["--preserve-stack"] if args.preserve_stack else []),
            *(["--port0"] if args.port0 else []),
            *(["--notebook", args.notebook.resolve()] if args.notebook else []),
        ],
        check=True,
    )
    with emulator(
        spec, out / "clean", "2000 tap f6\n4000 tap f6\n7000 capture home\n9000 finish-install\n"
    ) as child:
        completed(child, 90)
    type_digits = "3500 capture editor\n4300 tap two\n4900 tap zero\n6000 capture typed\n"
    save = "6600 tap down\n7300 tap enter\n"
    actions = ""
    if args.notebook:
        actions = (
            type_digits
            + (save + "9500 tap on\n" if args.port0 else "8500 tap on\n")
            + "14000 capture returned\n"
        )
    commands = ["-95 CF HOME"]
    if args.preserve_stack:
        commands.append('"first" "second"')
    if args.port0:
        commands.append("'ENTRY48' RCL 'ENTRY48' PURGE :0:NOTES STO 1 :0:TAG STO")
    commands.append(":0:NOTES RCL" if args.port0 else "'ENTRY48' RCL")
    transfer(
        spec,
        out / "run",
        state,
        artifact,
        "ENTRY48",
        package_name="program.HP",
        actions=actions,
        resource_prefix=samples / "run" if args.notebook else None,
        remote_commands=commands if args.port0 or args.preserve_stack else None,
    )
    if args.notebook:
        check_editor(out / "run", "")
        seed = b"hpclang-notebook/1\n314"
        if args.port0:
            check_home_note(out / "run/state", b"hpclang-notebook/1\n20")
        else:
            wrap = runpy.run_path(str(E2E.parents[1] / "tools/hp-rpl-string-wrap.py"))[
                "wrap_docstr"
            ]
            (artifact / "NOTE.HP").write_bytes(wrap(seed))
        resume_actions = actions
        if args.port0:
            resume_actions = (
                type_digits + save + "9500 capture first-save\n"
                "10200 tap up\n11000 tap two\n11800 tap down\n12600 tap enter\n"
                "14700 capture second-save\n15500 tap on\n19500 capture returned\n"
            )
        transfer(
            spec,
            out / "resume",
            out / "run/state",
            artifact,
            "ENTRY48",
            installed=True,
            remote_commands=["-95 CF HOME :0:NOTES RCL"] if args.port0 else None,
            extra_files=[] if args.port0 else [("NOTE.HP", "HPCNOTE")],
            actions=resume_actions,
            resource_prefix=samples / "resume",
            execution_ms=23000 if args.port0 else 17000,
        )
        check_editor(out / "resume", "20" if args.port0 else "314", repeat_save=args.port0)
        snapshot = out / "resume/state"
        check_home_note(snapshot, b"hpclang-notebook/1\n20202" if args.port0 else seed)
        subprocess.run(
            [
                sys.executable,
                fixture / "verify.py",
                "--artifact",
                artifact,
                "--baseline",
                state,
                "--state",
                snapshot,
                "--report",
                out / "resume.json",
            ],
            check=True,
        )
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

    if args.notebook:
        stock_workspace(samples, out, args.port0)

    if args.port0:
        for phase in ("report.json", "resume.json"):
            snapshot = json.loads((out / phase).read_text())
            if snapshot["port0_name"] != "NOTES" or snapshot["port0_preceding_records"] < 1:
                raise ValueError(f"{phase}: expected NOTES behind another Port 0 record")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="hpclang-hp48-workspace-") as directory:
        main(Path(directory))
