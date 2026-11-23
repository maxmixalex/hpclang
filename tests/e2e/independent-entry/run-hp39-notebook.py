"""Transfer an installed SDK's independent Notebook and verify Save on HP39/40 Stock."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
E2E = HERE.parent
ROOT = E2E.parents[1]
sys.path.insert(0, str(E2E / "port-recovery"))
from run import process, wait_for  # noqa: E402


def save_sequence(model, output):
    sequences = E2E / "sequences"
    lines = []
    for line in (sequences / f"{model}-stock-usb-run.seq").read_text().splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[1] == "dismiss":
            fields[3] = os.path.relpath(sequences / fields[3], output.parent)
            line = " ".join(fields)
        lines.append(line)
        if fields[1:] == ["host-gate"]:
            break
    else:
        raise ValueError(f"{model}: missing Stock USB payload gate")
    lines.append((sequences / "hp39-independent-notebook-save.seq").read_text())
    output.write_text("\n".join(lines))


def run_stock(dist, state, out, boot, env, timeout, samples):
    with (out / "emulator.log").open("w") as trace:
        subprocess.run(
            [
                dist / "x50ng",
                f"--datadir={state}",
                "--overwrite-config",
                f"--style={dist}/style-40g.css",
                *boot,
            ],
            env={**env, "HPCLANG_X50NG_RESOURCE_SNAPSHOT": str(samples / out.name)},
            stdout=trace,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=timeout,
        )
    if json.loads((out / "result.json").read_text())["status"] != "installed":
        raise ValueError("Stock Save sequence did not finish")


def verify_save(data, state, out, samples, after_reset=False, original_note=None, extra_text=""):
    saved = "20" if after_reset else "Seed4220"
    if original_note is not None:
        saved = original_note + "20" + extra_text
    report = subprocess.check_output(
        [
            sys.executable,
            HERE / "check-hp39-aplet.py",
            data / "independent-aplet/HPGCC000.000",
            state / "sram",
            "--bitmap-pool",
            data / "independent-entry",
            "--bitmap-image",
            data / "notebook.hpc",
            "--initial-note",
            "Seed42",
            "--saved-note",
            saved,
            "--save-run",
            out,
            *(["--after-reset"] if after_reset else []),
            *(
                ["--copy-state", "saved", "--original-note", original_note]
                if original_note is not None
                else []
            ),
        ],
        text=True,
    )
    verified = json.loads(report)
    resource = runpy.run_path(str(E2E / "harness/verify-resource-snapshots.py"))
    stack_gap = runpy.run_path(str(E2E / "independent-pool/verify.py"))["stock_stack_gap_nibbles"]
    gaps = {}
    for phase in ("action", "saved", "returned", "editor2", "final"):
        prefix = samples / f"{out.name}-{phase}"
        ram = prefix.with_suffix(".sram").read_bytes()
        info = json.loads(prefix.with_suffix(".json").read_text())
        resource["metadata"](info, resource["MODELS"][data.name], phase, len(ram))
        gaps[phase] = stack_gap(ram, verified["saturn_cpu"])
    if gaps["final"] // 2 != verified["stock_stack_gap_bytes"]:
        raise ValueError("library capture and settled Stock stack gap differ")
    if gaps["editor2"] >= gaps["final"]:
        raise ValueError("expected more Stock workspace after leaving the editor")
    verified["stock_stack_gap_nibbles"] = gaps
    verified.update(model=data.name, after_reset=after_reset, reopened_note=saved)
    if original_note is not None:
        verified.update(copy_name="2", original_note=original_note)
    (out / "verified.json").write_text(json.dumps(verified, indent=2) + "\n")


def replay_save(dist, state, out, env, samples, sequence, extra_text=""):
    out.mkdir()
    suffix = E2E / "sequences/hp39-independent-notebook-save.seq"
    delay = 0
    for line in suffix.read_text().splitlines():
        if line and not line.startswith("#"):
            when, action = line.split(maxsplit=1)
            at = 8000 + int(when)
            sequence.append(f"{at + delay} {action}")
            if action == "tap zero":
                for digit in extra_text:
                    delay += 300
                    key = {"2": "two", "0": "zero"}[digit]
                    sequence.append(f"{at + delay} tap {key}")
    seq = out / "sequence.seq"
    seq.write_text("\n".join(sequence) + "\n")
    env.update(
        HPCLANG_E2E_SEQUENCE=str(seq),
        HPCLANG_STOCK_PREFIX=str(out / "screen"),
        HPCLANG_STOCK_JSON=str(out / "result.json"),
    )
    run_stock(dist, state, out, [], env, 120 + (delay + 999) // 1000, samples)


def main(samples):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, choices=("hp39gplus", "hp39gs", "hp40gs"))
    parser.add_argument("--inputs", type=Path, required=True, help="export from tests/sdk/check.py")
    parser.add_argument("--dist", type=Path, required=True, help="portable x50ng distribution")
    parser.add_argument("--firmware", type=Path, help="Stock 2.22 update image for the gs model")
    parser.add_argument("--output", required=True, type=Path, help="new run directory")
    parser.add_argument(
        "--edit-copy",
        action="store_true",
        help="also save and reopen a Stock copy while retaining the original",
    )
    args = parser.parse_args()
    profile = json.loads((E2E / "targets" / f"{args.model}.json").read_text())
    nor = profile.get("nor")
    if bool(args.firmware) == bool(nor):
        parser.error("supply --firmware for HP39gs or HP40gs only")
    data = args.inputs.resolve() / args.model
    probe = data / "independent-entry"
    if json.loads((probe / "build.json").read_text())["model"] != args.model:
        parser.error("probe build model differs from requested calculator")
    bundle = data / "independent-aplet"
    dist = args.dist.resolve()
    out = args.output.resolve()
    out.mkdir(parents=True)
    state = out / "state"
    state.mkdir()
    boot = []
    if nor:
        subprocess.run(
            [
                sys.executable,
                ROOT / "tools/prepare-nor-fixture.py",
                ROOT / nor["path"],
                state,
                "--compression",
                nor["compression"],
                "--bytes",
                str(nor["bytes"]),
                "--state-bytes",
                str(nor["state_bytes"]),
                "--sha256",
                nor["sha256"],
            ],
            check=True,
        )
    else:
        if hashlib.sha256(args.firmware.read_bytes()).hexdigest() != profile["firmware"]["sha256"]:
            parser.error(f"firmware does not match the verified {args.model} 2.22 update")
        boot = [
            "--flash-full",
            f"--bootloader={dist}/firmware/boot-50g.bin",
            f"--firmware={args.firmware.resolve(strict=True)}",
        ]
    seq = out / "sequence.seq"
    save_sequence(args.model, seq)
    env = {
        k: v
        for k, v in os.environ.items()
        if not k.startswith(("HPCLANG_X50NG_", "HPCLANG_E2E_", "HPCLANG_STOCK_"))
    }
    env.update(
        PATH=f"{dist}/tools:{env['PATH']}",
        HPCLANG_X50NG_HARNESS="stock",
        TARGET=args.model,
        HPCLANG_STOCK_MODE="install",
        HPCLANG_X50NG_LCD_HEIGHT=str(profile["lcd_height"]),
        HPCLANG_E2E_SEQUENCE=str(seq),
        HPCLANG_E2E_HOST_GATE_FILE=str(out / "gate"),
        HPCLANG_STOCK_PREFIX=str(out / "screen"),
        HPCLANG_STOCK_JSON=str(out / "result.json"),
        HPCLANG_X50NG_USB0=str(out / "calc"),
    )
    if (dist / "runtime-lib").is_dir():
        env["LD_LIBRARY_PATH"] = os.pathsep.join(
            filter(None, [str(dist / "runtime-lib"), env.get("LD_LIBRARY_PATH")])
        )
    gate = [sys.executable, ROOT / "tools/wait-hp39-kermit-fetch.py"]
    timeout = int(subprocess.check_output([*gate, "timeout"], text=True))
    with ExitStack() as stack:
        pair = stack.enter_context(
            process(
                ["socat", f"PTY,raw,echo=0,link={out}/calc", f"PTY,raw,echo=0,link={out}/host"],
                env,
                out / "serial.log",
            )
        )
        wait_for(
            lambda: (out / "host").exists() and (out / "calc").exists(),
            [pair],
            10,
            "USB serial pair",
        )
        log = out / "kermit.log"
        stack.enter_context(
            process(
                ["bash", ROOT / "tools/run-hp39-kermit-server.sh", out / "host", bundle, log],
                env,
                out / "server.log",
            )
        )
        fetch = stack.enter_context(
            process(
                [*gate, "wait", log, out / "gate", "--last-file", "HPCRESET.000"],
                env,
                out / "gate.log",
            )
        )
        run_stock(dist, state, out, boot, env, timeout + 120, samples)
        if fetch.wait(timeout=5):
            raise RuntimeError("USB transfer did not complete")
    verify_save(data, state, out, samples)
    del env["HPCLANG_X50NG_USB0"], env["HPCLANG_E2E_HOST_GATE_FILE"]
    reset = out / "reset"
    replay_save(dist, state, reset, env, samples, ["1000 tap f2", "3000 tap f6", "7000 tap aplet"])
    verify_save(data, state, reset, samples, after_reset=True)
    if args.edit_copy:
        create_copy = ["1000 tap f1", "3500 tap two", "4500 tap enter"]
        if args.model == "hp40gs":
            # Both copies start from the same settled reset original.
            long_state = samples / "full-copy-state"
            shutil.copytree(state, long_state)
            full_copy = out / "full-copy"
            extra_text = "02" * 93 + "0"  # 187 more digits after "2020".
            replay_save(dist, long_state, full_copy, env, samples, create_copy.copy(), extra_text)
            verify_save(
                data, long_state, full_copy, samples, original_note="20", extra_text=extra_text
            )
            print("hp40gs: full 191-byte copy reopened and the original retained 20")
        copy = out / "copy"
        replay_save(dist, state, copy, env, samples, create_copy)
        verify_save(data, state, copy, samples, original_note="20")
        print(f"{args.model}: copy 2 reopened 2020 and the original retained 20")
    print(f"{args.model}: saved and reopened Seed4220, reset the Aplet, then saved and reopened 20")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="hpclang-hp39-sram-") as directory:
        main(Path(directory))
