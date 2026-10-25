"""Run original HPGCC3 native artifacts through the Linux E2E harness serially."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

MODELS = ("hp50g", "hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus")
PROGRAMS = ("hello", "gray", "shapes")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "artifacts", type=Path, help="check-hpgcc3-graphics.py --native work directory"
    )
    parser.add_argument("--work", type=Path, required=True, help="new evidence directory")
    parser.add_argument("--mode", choices=("direct", "stock"), required=True)
    parser.add_argument("--models", nargs="+", choices=MODELS, default=MODELS)
    parser.add_argument("--harness-root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    root, artifacts, work = (
        path.resolve() for path in (args.harness_root, args.artifacts, args.work)
    )
    harness = root / "tests/e2e/harness"
    runner = harness / f"run-{args.mode}.sh"
    if not runner.is_file():
        parser.error(f"Missing harness: {runner}")
    models = list(dict.fromkeys(args.models))
    fingerprints = {}
    for program in PROGRAMS:
        artifact = artifacts / f"{program}-native-artifact"
        required = [artifact / name for name in ("e2e.elf", "e2e.hpc", "e2e.result-address")]
        required += [artifact / "launchers" / f"{model}.hp" for model in models]
        if args.mode == "direct":
            required += [artifact / "launchers" / f"{model}.bin" for model in models]
        for path in required:
            if not path.is_file():
                parser.error(f"Missing artifact: {path}")
        fingerprints[program] = {str(p.relative_to(artifact)): digest(p) for p in required}
    work.mkdir(parents=True, exist_ok=False)
    temp = work / "temp"
    temp.mkdir()
    env = dict(os.environ, RUNNER_TEMP=str(temp), TMPDIR=str(temp))
    report = {
        "mode": args.mode,
        "models": models,
        "artifact_sha256": fingerprints,
        "harness_root": str(root),
        "runner_sha256": digest(runner),
        "passed": False,
        "cases": [],
    }

    def save():
        (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    def run(command, log):
        with log.open("w", encoding="utf-8") as stream:
            return subprocess.run(
                command, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, check=False
            ).returncode

    save()
    if args.mode == "stock":
        for program in PROGRAMS:
            code = run(
                [
                    "bash",
                    str(harness / "prepare-stock-assets.sh"),
                    str(artifacts / f"{program}-native-artifact"),
                    str(work / f"{program}-assets"),
                ],
                work / f"{program}-assets.log",
            )
            if code:
                raise SystemExit(f"Stock asset preparation failed: {program}; see {work}")
    for model in models:
        for program in PROGRAMS:
            case = work / f"{model}-{program}"
            case.mkdir()
            result_name = f"{model}-{'e2e' if args.mode == 'direct' else 'stock'}.json"
            # Do not let an earlier run's result masquerade as this invocation's result.
            (root / "build" / result_name).unlink(missing_ok=True)
            command = ["bash", str(runner), model, str(artifacts / f"{program}-native-artifact")]
            if args.mode == "stock":
                command.append(str(work / f"{program}-assets"))
            code = run(command, case / "run.log")
            for path in (root / "build").glob(f"{model}-*"):
                if path.is_file() and path.suffix in (".json", ".pgm", ".lcd"):
                    shutil.copy2(path, case / path.name)
            row = {"model": model, "program": program, "exit": code, "passed": False}
            report["cases"].append(row)
            save()
            result = case / result_name
            if not result.is_file():
                raise SystemExit(f"Missing native result: {case}")
            data = json.loads(result.read_text(encoding="utf-8"))
            row["result"] = data
            save()
            if (
                code
                or data.get("status") != "pass"
                or not all(data.get(key) is True for key in ("returned", "return_settled"))
            ):
                raise SystemExit(f"Native execution/return failed: {case}")
            pixel_code = run(
                [
                    sys.executable,
                    str(Path(__file__).with_name("verify-hpgcc3-graphics.py")),
                    str(case / f"{model}-frame-2200.pgm"),
                    program,
                ],
                case / "pixels.log",
            )
            row["pixel_exit"] = pixel_code
            row["passed"] = pixel_code == 0
            save()
            if pixel_code:
                raise SystemExit(f"Native pixels failed: {case}")
            print(f"PASS {args.mode} {model} {program}", flush=True)
    report["passed"] = True
    save()


if __name__ == "__main__":
    main()
