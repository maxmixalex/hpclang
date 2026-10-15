"""Run independent HP50g recovery from a fresh Stock boot and explicit fault injection."""

import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time
from types import SimpleNamespace

from prepare import prepare
from verify import verify


ROOT = Path(__file__).resolve().parents[3]


@contextmanager
def process(command, environment, log):
    with log.open("w") as output:
        child = subprocess.Popen(command, env=environment, stdout=output, stderr=output)
        try:
            yield child
        finally:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


def wait_for(predicate, children, timeout, description):
    deadline = time.monotonic() + timeout
    while not predicate():
        if any(child.poll() is not None for child in children):
            raise RuntimeError(f"Process exited before {description}")
        if time.monotonic() >= deadline:
            raise TimeoutError(description)
        time.sleep(0.1)


def emulator(args, phase, sequence, extra=(), usb=None):
    phase.mkdir(parents=True, exist_ok=True)
    seq = phase / "sequence.seq"
    seq.write_text(sequence)
    env = dict(os.environ)
    for key in tuple(env):
        if key.startswith(("HPCLANG_X50NG_", "HPCLANG_STOCK_", "HPCLANG_E2E_")):
            del env[key]
    env.update(
        TARGET="hp50g",
        HPCLANG_X50NG_HARNESS="stock",
        HPCLANG_X50NG_SRAM_SIZE="0x00080000",
        HPCLANG_X50NG_LCD_HEIGHT="80",
        HPCLANG_STOCK_MODE="install",
        HPCLANG_E2E_SEQUENCE=str(seq),
        HPCLANG_STOCK_PREFIX=str(phase / "stock"),
        HPCLANG_STOCK_JSON=str(phase / "stock.json"),
    )
    if usb is not None:
        env["HPCLANG_X50NG_USB0"] = str(usb)
        env["HPCLANG_E2E_HOST_GATE_FILE"] = str(phase / "complete")
    command = [
        str(args.dist / "x50ng"),
        "--datadir=" + str(phase / "state"),
        "--overwrite-config",
        "--style=" + str(args.dist / "style-50g.css"),
        *extra,
    ]
    return process(command, env, phase / "emulator.log")


def completed(child, timeout):
    if child.wait(timeout=timeout):
        raise RuntimeError("Emulator failed; inspect phase emulator.log")


def run(args):
    # No stale ready captures, host gates, saved RAM or output from another run.
    args.output.mkdir(parents=True)
    firmware = args.dist / "firmware"
    clean = args.output / "clean"
    with emulator(
        args,
        clean,
        "2000 tap f6\n4000 tap f6\n7000 capture home\n9000 finish-install\n",
        [
            "--bootloader=" + str(firmware / "boot-50g.bin"),
            "--firmware=" + str(firmware / "hp4950v215/2MB_FIX/2MB_215f.bin"),
            "--flash",
        ],
    ) as child:
        completed(child, 90)
    print("Clean Stock boot complete", flush=True)

    reset = args.output / "reset"
    reset.mkdir()
    prepare(clean / "state", reset / "state", args.slot, args.target)
    damaged = (reset / "state/flash").read_bytes()
    with emulator(
        args, reset, "2000 capture restart\n10000 capture home\n12000 finish-install\n", ["--reset"]
    ) as child:
        completed(child, 90)
    if (reset / "state/flash").read_bytes() != damaged:
        raise RuntimeError("Hardware reset changed the injected Flash state")
    print("Fault prepared and hardware reset complete", flush=True)

    recovery = args.output / "recovery"
    recovery.mkdir()
    shutil.copytree(reset / "state", recovery / "state")
    send = recovery / "send"
    send.mkdir()
    shutil.copyfile(args.build / "recovery.hp", send / "RECOVERY.HP")
    prefix = []
    for line in (ROOT / "tests/e2e/sequences/hp4950-stock-usb-run.seq").read_text().splitlines():
        tokens = line.split()
        # Welcome prompts were acknowledged during the clean boot.
        if tokens[1:] != ["tap", "f6"]:
            prefix.append(line)
        if tokens[1:] == ["host-gate"]:
            break
    else:
        raise ValueError("Stock USB sequence has no host gate")
    sequence = (
        "\n".join(prefix)
        + "\n500 capture deposited\n1000 tap eval\n15000 capture returned\n17000 finish-install\n"
    )
    xtty, ktty = recovery / "x50-tty", recovery / "kermit-tty"
    with process(
        ["socat", f"PTY,raw,echo=0,link={xtty}", f"PTY,raw,echo=0,link={ktty}"],
        os.environ,
        recovery / "socat.log",
    ) as bridge:
        wait_for(lambda: xtty.exists() and ktty.exists(), [bridge], 10, "PTY creation")
        with emulator(args, recovery, sequence, usb=xtty) as child:
            wait_for(
                lambda: (recovery / "stock-server-usb.pgm").exists(),
                [bridge, child],
                120,
                "Stock USB server readiness",
            )
            log = recovery / "kermit.log"
            with process(
                [
                    sys.executable,
                    str(ROOT / "tools/kermit_watchdog.py"),
                    "run",
                    str(log),
                    "bash",
                    str(ROOT / "tools/run-hp48-kermit-send.sh"),
                    str(ktty),
                    str(send),
                    str(log),
                    "--remote-host",
                    "-95 CF HOME 'RECOVERY' RCL",
                    "RECOVERY.HP=RECOVERY",
                ],
                os.environ,
                recovery / "transfer.log",
            ) as sender:
                if sender.wait():
                    raise RuntimeError("Recovery transfer failed; inspect transfer.log")
            (recovery / "complete").touch()
            completed(child, 90)
    verify(
        SimpleNamespace(
            before=reset / "state",
            after=recovery / "state",
            elf=args.build / "recovery.elf",
            binary=args.build / "recovery.bin",
            package=args.build / "recovery.hp",
            nm=args.nm,
            slot=args.slot,
            target=args.target,
        )
    )
    (args.output / "result.json").write_text(
        json.dumps(
            {
                "status": "pass",
                "model": "hp50g",
                "fault": "offline-erased-header-sector",
                "slot": args.slot,
                "target": args.target,
            },
            indent=2,
        )
        + "\n"
    )
    print("PASS: fresh-firmware independent Stock recovery", flush=True)


def main():
    def terminate(signum, _frame):
        raise SystemExit(128 + signum)

    signal.signal(signal.SIGTERM, terminate)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, required=True, help="portable x50ng distribution")
    parser.add_argument(
        "--build", type=Path, required=True, help="built recovery witness directory"
    )
    parser.add_argument("--output", type=Path, required=True, help="new evidence directory")
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument("--slot", type=lambda value: int(value, 0), default=0x160105)
    parser.add_argument("--target", type=lambda value: int(value, 0), default=0x140000)
    args = parser.parse_args()
    args.dist, args.build, args.output = (
        path.resolve() for path in (args.dist, args.build, args.output)
    )
    run(args)


if __name__ == "__main__":
    main()
