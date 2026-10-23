"""Run independent HP49g+/HP50g recovery from fresh Stock and explicit fault injection."""

import argparse
import configparser
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

from prepare import configuration_body_sector, prepare, reserve_bank_tail
from reservation import run_reservation
from verify import (
    verify,
    verify_interruption,
    verify_restart,
    verify_power_cut,
    verify_retirement_cut,
    native_words,
    require_stock_pc,
    verify_flash,
    home_string,
    verify_zero_real_array,
)


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


def emulator(
    args,
    phase,
    sequence,
    extra=(),
    usb=None,
    power_cut=None,
    *,
    flash_busy=False,
    reset_checkpoint=False,
):
    phase.mkdir(parents=True, exist_ok=True)
    seq = phase / "sequence.seq"
    seq.write_text(sequence)
    env = dict(os.environ)
    for key in tuple(env):
        if key.startswith(("HPCLANG_X50NG_", "HPCLANG_STOCK_", "HPCLANG_E2E_")):
            del env[key]
    env.update(
        TARGET=args.model,
        HPCLANG_X50NG_HARNESS="stock",
        HPCLANG_X50NG_SRAM_SIZE="0x00080000",
        HPCLANG_X50NG_LCD_HEIGHT=str(args.profile["lcd_height"]),
        HPCLANG_STOCK_MODE="install",
        HPCLANG_E2E_SEQUENCE=str(seq),
        HPCLANG_STOCK_PREFIX=str(phase / "stock"),
        HPCLANG_STOCK_JSON=str(phase / "stock.json"),
    )
    if usb is not None:
        env["HPCLANG_X50NG_USB0"] = str(usb)
        env["HPCLANG_E2E_HOST_GATE_FILE"] = str(phase / "complete")
        if power_cut is not None:
            operation, reads, mask = power_cut
            env["HPCLANG_X50NG_FLASH_POWER_CUT"] = f"{operation}:{reads}:{mask:x}"
    if args.flash_busy_reads and (usb is not None or flash_busy):
        env["HPCLANG_X50NG_FLASH_BUSY_READS"] = str(args.flash_busy_reads)
    if reset_checkpoint:
        env["HPCLANG_X50NG_RESET_CHECKPOINT"] = str(phase / "before-reset")
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


def transfer(
    args,
    recovery,
    source,
    build,
    name,
    power_cut=None,
    *,
    installed=False,
    execute=True,
    remote_commands=None,
    package_name="recovery.hp",
    consume=False,
    execution_ms=17000,
):
    if execution_ms < 2000:
        raise ValueError("Execution budget must allow the final checkpoint")
    if consume and (not execute or remote_commands is not None):
        raise ValueError("Consuming a fixture requires its default execution command")
    recovery.mkdir()
    shutil.copytree(source, recovery / "state")
    send = recovery / "send"
    send.mkdir()
    if not installed:
        shutil.copyfile(build / package_name, send / "RECOVERY.HP")
    if remote_commands is None:
        remote_commands = [f"-95 CF HOME '{name}' RCL" if execute else "-95 CF HOME"]
        if consume:
            # RCL retains the executable object on the stack. Release only this
            # one-shot fixture's HOME binding; snapshots retain its evidence.
            # Accumulating old witnesses can exhaust Stock menu/USB workspace.
            remote_commands[0] += f" '{name}' PURGE"
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
        + "\n500 capture deposited\n"
        + ("1000 tap eval\n" if execute else "")
        + f"{execution_ms - 2000} capture checkpoint\n{execution_ms} finish-install\n"
    )
    xtty, ktty = recovery / "x50-tty", recovery / "kermit-tty"
    with process(
        ["socat", f"PTY,raw,echo=0,link={xtty}", f"PTY,raw,echo=0,link={ktty}"],
        os.environ,
        recovery / "socat.log",
    ) as bridge:
        wait_for(lambda: xtty.exists() and ktty.exists(), [bridge], 10, "PTY creation")
        with emulator(args, recovery, sequence, usb=xtty, power_cut=power_cut) as child:
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
                    *[token for command in remote_commands for token in ("--remote-host", command)],
                    *([] if installed else [f"RECOVERY.HP={name}"]),
                ],
                os.environ,
                recovery / "transfer.log",
            ) as sender:
                if sender.wait():
                    raise RuntimeError("Recovery transfer failed; inspect transfer.log")
            # Stock can report an RPL error in a successful Kermit response.
            if "Error:" in (recovery / "transfer.log").read_text(errors="replace"):
                raise RuntimeError("Stock rejected recovery setup; inspect transfer.log")
            (recovery / "complete").touch()
            if power_cut is None:
                completed(child, 90)
            elif child.wait(timeout=90) != 86:
                raise RuntimeError("Emulator did not terminate at the requested Flash power cut")

    if power_cut is None:
        verify_busy_reads(args, recovery)


def verify_busy_reads(args, phase):
    """Validate saved busy-state evidence and return the observed operation count."""
    if args.flash_busy_reads:
        state = configparser.ConfigParser()
        state.read(phase / "state/state")
        evidence = state["flash"]
        operations = evidence.getint("busy-operations")
        if (
            evidence.getint("busy-read-budget") != args.flash_busy_reads
            or operations is None
            or operations < 0
            or evidence.getint("busy-status-reads") != operations * args.flash_busy_reads
            or evidence.getboolean("busy-active") is not False
        ):
            raise RuntimeError("Missing/incomplete Flash busy-state execution evidence")
        return operations
    return 0


def startup_recovery(args, clean, damaged):
    """Exercise Stock's STARTUP hook in a fresh fixture, never an existing user HOME."""
    build = args.retirement_build
    installed = args.output / "startup-installed"
    transfer(args, installed, damaged, build, "STARTUP", execute=False)
    if (installed / "state/flash").read_bytes() != (damaged / "flash").read_bytes():
        raise RuntimeError("STARTUP installation changed Flash before warmstart")
    before = installed / "state"
    for name, operation in (("startup-recovered", "retire"), ("startup-rechecked", "check")):
        phase = args.output / name
        phase.mkdir()
        shutil.copytree(before, phase / "state")
        with emulator(
            args,
            phase,
            "10000 capture home\n12000 finish-install\n",
            ["--reset"],
            flash_busy=True,
        ) as child:
            completed(child, 90)
        verify_busy_reads(args, phase)
        verify(
            SimpleNamespace(
                before=before,
                after=phase / "state",
                elf=build / "recovery.elf",
                binary=build / "recovery.bin",
                package=build / "recovery.hp",
                nm=args.nm,
                slot=args.slot,
                target=args.target,
                operation=operation,
            )
        )
        if operation == "check":
            verify_restart(clean, before, phase / "state")
        before = phase / "state"
    print("Automatic STARTUP replay/retirement and clean second warmstart verified", flush=True)


def configuration_recovery(args, clean):
    """Prove automatic replay and catalog rebuilding, with retained RAM and RAM loss.

    The library number and HOME names belong only to this fresh-firmware fixture.
    This is not an installer for a user's existing calculator.
    """
    build = args.configuration_build
    installed = args.output / "configuration-installed"
    # Construct the fixture with Stock's own driver. The synthetic read-count
    # busy model exercises our recovery polling below, not Stock's installation driver.
    setup = SimpleNamespace(**(vars(args) | {"flash_busy_reads": 0}))
    installation_source = args.output / "configuration-install-source"
    fault_bank = args.target & ~0x1FFFF
    reserve_bank_tail(clean, installation_source, fault_bank)
    transfer(
        setup,
        installed,
        installation_source,
        build,
        "RECOVERY",
        execute=False,
        package_name="recovery-library.hp",
        remote_commands=[
            "-95 CF HOME 'RECOVERY' RCL 2 STO 'RECOVERY' PURGE",
            "\"hpclang recovery\" 'HPCKEEP' STO",
        ],
    )
    protected = slice(fault_bank, fault_bank + 0x20000)
    if (installed / "state/flash").read_bytes()[protected] != (
        installation_source / "flash"
    ).read_bytes()[protected]:
        raise RuntimeError("Stock changed the protected fault bank during recovery installation")

    def boot(source, name, *, clear_ram=False, check_home=True):
        phase = args.output / name
        phase.mkdir()
        shutil.copytree(source, phase / "state")
        sequence = "10000 capture home\n12000 finish-install\n"
        if clear_ram:
            ram = phase / "state/sram"
            ram.write_bytes(bytes(ram.stat().st_size))
            # Stock asks whether to recover memory after RAM loss. Decline it;
            # recovery must come from the installed Flash library alone.
            sequence = "20000 tap f6\n40000 tap f6\n60000 capture home\n62000 finish-install\n"
        with emulator(
            args,
            phase,
            sequence,
            ["--reset"],
            flash_busy=True,
        ) as child:
            completed(child, 90)
        verify_busy_reads(args, phase)
        require_stock_pc(phase / "state")
        read = native_words(
            (phase / "state/sram").read_bytes(), (phase / "state/flash").read_bytes()
        )
        if check_home and home_string(read, b"HPCKEEP") != b"hpclang recovery":
            raise RuntimeError("Configuration recovery changed the HOME fixture string")
        return phase / "state"

    baseline = boot(installed / "state", "configuration-clean")
    if (baseline / "flash").read_bytes() != (installed / "state/flash").read_bytes():
        raise RuntimeError("Clean library configuration changed Flash")
    read = native_words((baseline / "sram").read_bytes(), (baseline / "flash").read_bytes())
    count = read(0x8611D, 3)
    if not 0 < count < 128 or 1701 not in [read(0x86120 + 16 * i, 3) for i in range(count)]:
        raise RuntimeError("Configuration library is absent from the native catalog")
    damaged = args.output / "configuration-fault"
    bound = getattr(args, "configuration_authority", False)
    authority_slot = args.slot + 4124 if bound else None
    prepare(baseline, damaged, args.slot, args.target, bound=bound)
    for lost_ram, names in (
        (False, ("configuration-recovered", "configuration-rechecked")),
        (True, ("configuration-ram-loss", "configuration-ram-loss-rechecked")),
    ):
        before = damaged
        for name in names:
            after = boot(
                before,
                name,
                clear_ram=lost_ram and before == damaged,
                check_home=not lost_ram,
            )
            if before == damaged:
                verify_flash(
                    (before / "flash").read_bytes(),
                    (after / "flash").read_bytes(),
                    args.slot,
                    args.target,
                    retire=True,
                    authority_slot=authority_slot,
                )
                # The internal restart has already replaced the first invocation's
                # diagnostics with a clean invocation. Verify the durable transition
                # above, then verify this final no-op image and the complete catalog.
                before = after
            verify(
                SimpleNamespace(
                    before=before,
                    after=after,
                    elf=build / "recovery.elf",
                    binary=build / "recovery.bin",
                    package=build / "recovery-configuration.hp",
                    nm=args.nm,
                    slot=args.slot,
                    target=args.target,
                    operation="check",
                    relocated_image=True,
                    authority_slot=authority_slot,
                )
            )
            verify_restart(baseline, before, after)
            before = after
    # A missing bank header can suppress callbacks entirely. Also damage a
    # factory callback's body while retaining its library header and pointer.
    body_target = configuration_body_sector((baseline / "flash").read_bytes(), read)
    body_fault = args.output / "configuration-body-fault"
    prepare(baseline, body_fault, args.slot, body_target, bound=bound)
    repaired = boot(body_fault, "configuration-body-recovered")
    verify_flash(
        (body_fault / "flash").read_bytes(),
        (repaired / "flash").read_bytes(),
        args.slot,
        body_target,
        retire=True,
        authority_slot=authority_slot,
    )
    for state in (repaired, boot(repaired, "configuration-body-rechecked")):
        verify_restart(baseline, repaired, state)
        verify(
            SimpleNamespace(
                before=repaired,
                after=state,
                elf=build / "recovery.elf",
                binary=build / "recovery.bin",
                package=build / "recovery-configuration.hp",
                nm=args.nm,
                slot=args.slot,
                target=body_target,
                operation="check",
                relocated_image=True,
                authority_slot=authority_slot,
            )
        )
    # Allocate after the USB server exits; otherwise its temporary storage masks
    # the intended pressure. Decimal zero forces DOREAL rather than compact
    # exact-integer elements. Keep commands below Stock's server command limit.
    filled = args.output / "configuration-lowram-filled"
    transfer(
        setup,
        filled,
        baseline,
        build,
        "UNUSED",
        installed=True,
        execute=True,
        remote_commands=[
            "-95 CF HOME CLEAR",
            r"\{171} { 26000 } 0. CON 'HPCFILL' STO \{187} 'HPCPAD' STO",
            "'HPCPAD' RCL",
        ],
    )
    before = filled / "state"
    read_filled = native_words((before / "sram").read_bytes(), (before / "flash").read_bytes())
    verify_zero_real_array(read_filled, b"HPCFILL", 26000)
    gap = (read_filled(0x806F8) - read_filled(0x806F3)) // 2
    if (
        not (build / "recovery-configuration.hp").stat().st_size - 8
        <= gap
        < (build / "recovery.hp").stat().st_size - 8
    ):
        raise RuntimeError("Low-memory fixture does not distinguish the two recovery envelopes")
    if (before / "flash").read_bytes() != (baseline / "flash").read_bytes():
        raise RuntimeError("RAM pressure setup changed Flash")
    low_fault = args.output / "configuration-lowram-fault"
    prepare(before, low_fault, args.slot, args.target, bound=bound)
    low_repaired = boot(low_fault, "configuration-lowram-recovered")
    verify_flash(
        (low_fault / "flash").read_bytes(),
        (low_repaired / "flash").read_bytes(),
        args.slot,
        args.target,
        retire=True,
        authority_slot=authority_slot,
    )
    for state in (low_repaired, boot(low_repaired, "configuration-lowram-rechecked")):
        verify_restart(before, low_repaired, state)
        verify_zero_real_array(
            native_words((state / "sram").read_bytes(), (state / "flash").read_bytes()),
            b"HPCFILL",
            26000,
        )
        verify(
            SimpleNamespace(
                before=low_repaired,
                after=state,
                elf=build / "recovery.elf",
                binary=build / "recovery.bin",
                package=build / "recovery-configuration.hp",
                nm=args.nm,
                slot=args.slot,
                target=args.target,
                operation="check",
                relocated_image=True,
                authority_slot=authority_slot,
            )
        )
    print(
        "Flash configuration recovery rebuilt the catalog after bank-header and callback-body "
        "faults, including retained RAM and bank-header RAM loss; "
        "low-memory recovery preserved the real array; later warmstarts unchanged",
        flush=True,
    )


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
            "--bootloader=" + str(firmware / args.profile["bootloader"]),
            "--firmware=" + str(firmware / "hp4950v215/2MB_FIX/2MB_215f.bin"),
            "--flash",
        ],
    ) as child:
        completed(child, 90)
    bootloader = (firmware / args.profile["bootloader"]).read_bytes()
    if (clean / "state/flash").read_bytes()[: len(bootloader)] != bootloader:
        raise RuntimeError("Clean Stock state does not contain the selected model's bootloader")
    print(f"Clean {args.model} Stock boot complete", flush=True)

    if args.suite == "identity":
        from identity import run_identity

        run_identity(
            args,
            clean / "state",
            transfer=transfer,
            emulator=emulator,
            completed=completed,
            verify_busy_reads=verify_busy_reads,
        )
        return

    if args.suite in ("template", "creation", "registration"):
        from template import run_creation, run_registration, run_template

        suites = {
            "creation": run_creation,
            "template": run_template,
            "registration": run_registration,
        }
        result = suites[args.suite](
            args,
            clean / "state",
            args.output,
            transfer=transfer,
            emulator=emulator,
            completed=completed,
            verify_busy_reads=verify_busy_reads,
        )
        (args.output / "result.json").write_text(
            json.dumps(dict(status="pass", model=args.model, suite=args.suite, **result), indent=2)
            + "\n"
        )
        return

    if args.suite in ("installation", "publication"):
        from installation import run_installation, run_publication_suite

        (run_installation if args.suite == "installation" else run_publication_suite)(
            args,
            clean / "state",
            transfer=transfer,
            emulator=emulator,
            completed=completed,
            verify_busy_reads=verify_busy_reads,
        )
        return

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
    startup_source = reset / "state"

    for cut_index, power_cut in enumerate(args.power_cut or []):
        interrupted = args.output / f"power-cut-{cut_index}"
        transfer(
            args, interrupted, reset / "state", args.build, "RECOVERYPOWER", power_cut, consume=True
        )
        restored = verify_power_cut(
            reset / "state/flash",
            interrupted / "state/flash",
            interrupted / "emulator.log",
            args.slot,
            args.target,
            power_cut,
        )
        resume = args.output / f"power-resume-{cut_index}"
        resume.mkdir()
        shutil.copytree(interrupted / "state", resume / "state")
        with emulator(
            args, resume, "10000 capture home\n12000 finish-install\n", ["--reset"]
        ) as child:
            completed(child, 90)
        baseline = clean / "state" if restored else reset / "state"
        verify_restart(baseline, interrupted / "state", resume / "state")
        reset = resume
        print(
            "Busy-operation power cut verified; Stock restarted with expected catalog",
            flush=True,
        )

    if args.interrupted_build is not None:
        interrupted = args.output / "interrupted"
        transfer(
            args, interrupted, reset / "state", args.interrupted_build, "RECOVERYCUT", consume=True
        )
        verify_interruption(
            SimpleNamespace(
                before=reset / "state",
                after=interrupted / "state",
                elf=args.interrupted_build / "recovery.elf",
                binary=args.interrupted_build / "recovery.bin",
                nm=args.nm,
                slot=args.slot,
                target=args.target,
            )
        )
        resume = args.output / "resume"
        resume.mkdir()
        shutil.copytree(interrupted / "state", resume / "state")
        with emulator(
            args,
            resume,
            "2000 capture restart\n10000 capture home\n12000 finish-install\n",
            ["--reset"],
        ) as child:
            completed(child, 90)
        # Until the bank identity is published, Stock must expose the same
        # catalog as the original damaged-bank boot, with no partial library.
        verify_restart(reset / "state", interrupted / "state", resume / "state")
        reset = resume
        print("Interrupted recovery verified; reset kept the partial bank hidden", flush=True)

    recovery = args.output / "recovery"
    transfer(args, recovery, reset / "state", args.build, "RECOVERY", consume=True)
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
    restarted = args.output / "restarted"
    restarted.mkdir()
    shutil.copytree(recovery / "state", restarted / "state")
    with emulator(
        args,
        restarted,
        "2000 capture restart\n10000 capture home\n12000 finish-install\n",
        ["--reset"],
    ) as child:
        completed(child, 90)
    catalog_entries = verify_restart(clean / "state", recovery / "state", restarted / "state")
    retirement_cut = None
    if args.retirement_build is not None:
        retirement_source = restarted / "state"
        if args.flash_busy_reads > 1:
            # Clear one live status bit only; the neighboring byte stays intact.
            mask = 0x10 << (8 * (args.slot & 1))
            retirement_cut = (1, args.flash_busy_reads // 2, mask)
            interrupted = args.output / "retirement-cut"
            transfer(
                args,
                interrupted,
                retirement_source,
                args.retirement_build,
                "RETIRECUT",
                retirement_cut,
                consume=True,
            )
            verify_retirement_cut(
                retirement_source / "flash",
                interrupted / "state/flash",
                interrupted / "emulator.log",
                args.slot,
                args.target,
                retirement_cut,
            )
            resumed = args.output / "retirement-resume"
            resumed.mkdir()
            shutil.copytree(interrupted / "state", resumed / "state")
            with emulator(
                args, resumed, "10000 capture home\n12000 finish-install\n", ["--reset"]
            ) as child:
                completed(child, 90)
            verify_restart(clean / "state", interrupted / "state", resumed / "state")
            retirement_source = resumed / "state"
        retired = args.output / "retired"
        transfer(args, retired, retirement_source, args.retirement_build, "RECOVERYRETIRE")
        verify(
            SimpleNamespace(
                before=retirement_source,
                after=retired / "state",
                elf=args.retirement_build / "recovery.elf",
                binary=args.retirement_build / "recovery.bin",
                package=args.retirement_build / "recovery.hp",
                nm=args.nm,
                slot=args.slot,
                target=args.target,
                operation="retire",
            )
        )
        retired_restart = args.output / "retired-restarted"
        retired_restart.mkdir()
        shutil.copytree(retired / "state", retired_restart / "state")
        with emulator(
            args, retired_restart, "10000 capture home\n12000 finish-install\n", ["--reset"]
        ) as child:
            completed(child, 90)
        verify_restart(clean / "state", retired / "state", retired_restart / "state")
        checked = args.output / "retired-check"
        transfer(
            args,
            checked,
            retired_restart / "state",
            args.retirement_build,
            "RECOVERYRETIRE",
            installed=True,
        )
        verify(
            SimpleNamespace(
                before=retired_restart / "state",
                after=checked / "state",
                elf=args.retirement_build / "recovery.elf",
                binary=args.retirement_build / "recovery.bin",
                package=args.retirement_build / "recovery.hp",
                nm=args.nm,
                slot=args.slot,
                target=args.target,
                operation="check",
            )
        )
        verify_restart(clean / "state", retired_restart / "state", checked / "state")
        print("Journal retirement and unchanged native catalog after reset verified", flush=True)
        startup_recovery(args, clean / "state", startup_source)
    if args.configuration_build is not None:
        configuration_recovery(args, clean / "state")
        if args.configuration_authority:
            run_reservation(
                args,
                clean / "state",
                transfer=transfer,
                emulator=emulator,
                completed=completed,
                verify_busy_reads=verify_busy_reads,
            )
    (args.output / "result.json").write_text(
        json.dumps(
            {
                "status": "pass",
                "model": args.model,
                "fault": "offline-erased-header-sector",
                "slot": args.slot,
                "target": args.target,
                "restored_catalog_entries": catalog_entries,
                "interrupted_before_identity": args.interrupted_build is not None,
                "journal_retirement": args.retirement_build is not None,
                "startup_recovery": args.retirement_build is not None,
                "configuration_recovery": args.configuration_build is not None,
                "configuration_authority": args.configuration_authority,
                "guarded_reservation": args.configuration_authority,
                "retirement_power_cut": retirement_cut,
                "flash_busy_reads": args.flash_busy_reads,
                "power_cut": args.power_cut,
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
    parser.add_argument("--model", choices=("hp49gplus", "hp50g"), default="hp50g")
    parser.add_argument(
        "--suite",
        choices=(
            "recovery",
            "installation",
            "identity",
            "template",
            "creation",
            "publication",
            "registration",
        ),
        default="recovery",
    )
    parser.add_argument("--dist", type=Path, required=True, help="portable x50ng distribution")
    parser.add_argument(
        "--build", type=Path, required=True, help="built recovery witness directory"
    )
    parser.add_argument("--output", type=Path, required=True, help="new evidence directory")
    parser.add_argument(
        "--interrupted-build",
        type=Path,
        help="optional witness built with HPCLANG_RECOVERY_STOP_BEFORE_IDENTITY",
    )
    parser.add_argument("--nm", default="llvm-nm")
    parser.add_argument(
        "--retirement-build",
        type=Path,
        help="optional witness built with HPCLANG_RECOVERY_RETIRE_JOURNAL",
    )
    parser.add_argument(
        "--configuration-build",
        type=Path,
        help="optional retirement witness with HPCLANG_RECOVERY_RESTART_AFTER_REPAIR",
    )
    parser.add_argument(
        "--configuration-authority",
        action="store_true",
        help="configuration witness binds --slot to the immediately following authority record",
    )
    parser.add_argument(
        "--flash-busy-reads",
        type=int,
        default=0,
        help="deterministic status reads per Flash operation; zero disables",
    )
    parser.add_argument("--slot", type=lambda value: int(value, 0), default=0x160105)
    parser.add_argument(
        "--power-cut",
        action="append",
        nargs=3,
        type=lambda value: int(value, 0),
        metavar=("OPERATION", "READS", "BITS"),
        help="terminate during a busy operation with selected completed bits; repeat for consecutive cuts",
    )
    parser.add_argument("--target", type=lambda value: int(value, 0), default=0x140000)
    args = parser.parse_args()
    if args.configuration_authority and args.configuration_build is None:
        parser.error("--configuration-authority requires --configuration-build")
    if not 0 <= args.flash_busy_reads <= 0xFFFFFFFF:
        parser.error("--flash-busy-reads must fit an unsigned 32-bit counter")
    for operation, reads, bits in args.power_cut or []:
        if not (
            1 <= operation <= 0xFFFFFFFFFFFFFFFF
            and 0 < reads < args.flash_busy_reads
            and 0 <= bits <= 0xFFFF
        ):
            parser.error(
                "power cut needs a positive operation, reads within the busy interval and 16-bit mask"
            )
    args.profile = json.loads((ROOT / "tests/e2e/targets" / (args.model + ".json")).read_text())
    args.dist, args.build, args.output = (
        path.resolve() for path in (args.dist, args.build, args.output)
    )
    if args.interrupted_build is not None:
        args.interrupted_build = args.interrupted_build.resolve()
    if args.retirement_build is not None:
        args.retirement_build = args.retirement_build.resolve()
    if args.configuration_build is not None:
        args.configuration_build = args.configuration_build.resolve()
    run(args)


if __name__ == "__main__":
    main()
