"""Native acceptance of guarded recovery storage, not a trusted installer."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

from prepare import prepare, reserve_bank_tail
from verify import (
    home_object,
    home_string,
    installed_recovery,
    native_words,
    require_stock_pc,
    verify,
    verify_flash,
    verify_retired,
    verify_restart,
    verify_zero_real_array,
)


LIBRARY_ID = 1701
RESERVE_POWER = 15
RESERVE_BYTES = 1 << RESERVE_POWER
USER_ELEMENTS = 24000
NAME = b"HPCRES"
MARKER = b"RCDONE"


def packager():
    path = Path(__file__).resolve().parents[3] / "tools/package-recovery.py"
    spec = importlib.util.spec_from_file_location("recovery_packager", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def observed_library(pack, library):
    """Append a completion witness without changing any generated guard command.

    The unmodified library is tested separately for successful recovery. Negative
    tests clear this marker before each reset, so unchanged data cannot pass just
    because configuration never ran. Only this fixture tail writes the marker.
    """
    data = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in library[8:])

    def word(at, width=5):
        return int(data[at : at + width][::-1], 16)

    header = 14 + 2 * word(10, 2)
    fields = header + 3
    assert word(header, 3) == LIBRARY_ID
    links = fields + 10 + word(fields + 10)
    configuration_at = fields + 15 + word(fields + 15)
    assert word(links) == 0x2A4E and word(links + 5) in (35, 40)
    count = (word(links + 5) - 5) // 5
    starts = [links + 10 + 5 * i + word(links + 10 + 5 * i) for i in range(count)]
    commands = [data[first:last] for first, last in zip(starts, starts[1:] + [configuration_at])]
    configuration = data[configuration_at : 5 + word(5) - 4]
    assert configuration[-10:] == pack.immediate(0x389D4) + pack.immediate(0x312B)
    # Real 1, quoted native name, STO; never use the early-startup integer path.
    marker = pack.immediate(0x2F94C) + pack.immediate(0x389EF) + pack.immediate(0x2E48)
    marker += pack.immediate(len(MARKER), 2)
    marker += "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in MARKER)
    marker += pack.immediate(0x38A14) + pack.immediate(0x3E739)
    return pack._configuration_library(
        configuration[:-10] + marker + configuration[-10:], LIBRARY_ID, commands
    )


def reader(state):
    return native_words((state / "sram").read_bytes(), (state / "flash").read_bytes())


def require_absent(read, name):
    try:
        home_object(read, name)
    except ValueError as error:
        if "missing" not in str(error):
            raise
    else:
        raise ValueError("Unexpected retained fixture variable")


def require_marker(read):
    obj = home_object(read, MARKER)
    if read(obj) != 0x2933 or read(obj + 5, 16) != 0x0100000000000000:
        raise ValueError("Guard configuration did not finish with the expected marker")


def run_reservation(args, clean, *, transfer, emulator, completed, verify_busy_reads):
    root = args.output / "reservation"
    root.mkdir()
    build = args.configuration_build
    authority = args.slot + 4124
    prefix = f"HPCMEM1:{LIBRARY_ID:04d}:{authority:06X}:00000001".encode("ascii")
    pack = packager()
    package = root / "package"
    package.mkdir()
    # Bind recovery and admission together through the maintained CLI. Keep
    # ordinary admission.hp reusable; only the library's hidden copy is compact.
    tool_bin = Path(args.nm).parent
    subprocess.run(
        [
            sys.executable,
            str(Path(pack.__file__)),
            "--elf",
            "--model",
            args.model,
            "--flash-copy",
            "--journal-slot",
            str(args.slot),
            "--authority-slot",
            str(authority),
            "--library-id",
            str(LIBRARY_ID),
            "--reservation-name",
            NAME.decode(),
            "--reservation-bytes",
            str(RESERVE_BYTES),
            "--reservation-prefix",
            prefix.decode(),
            "--admission-elf",
            str(build / "admission.elf"),
            "--program-output",
            str(package / "recovery-configuration.hp"),
            "--nm",
            args.nm,
            "--readelf",
            str(tool_bin / "llvm-readelf"),
            "--objcopy",
            str(tool_bin / "llvm-objcopy"),
            str(build / "recovery.elf"),
            str(package / "recovery-guarded.hp"),
        ],
        check=True,
    )
    program = (package / "recovery-configuration.hp").read_bytes()
    if program != (build / "recovery-configuration.hp").read_bytes():
        raise ValueError("Recovery build binding differs from the requested transaction")
    library = (package / "recovery-guarded.hp").read_bytes()
    (package / "recovery-observed.hp").write_bytes(observed_library(pack, library))
    source = root / "install-source"
    bank = args.target & ~0x1FFFF
    reserve_bank_tail(clean, source, bank)
    original = reader(clean)
    original_count = original(0x8611D, 3)
    if not 0 < original_count < 128:
        raise ValueError("Invalid initial native catalog count")
    original_ids = {original(0x86120 + 16 * i, 3) for i in range(original_count)}
    if len(original_ids) != original_count:
        raise ValueError("Duplicate initial native library IDs")
    if LIBRARY_ID in original_ids:
        raise ValueError("Recovery fixture library ID is already occupied")

    def boot(before, label):
        phase = root / label
        phase.mkdir()
        shutil.copytree(before, phase / "state")
        with emulator(
            args,
            phase,
            "10000 capture home\n12000 finish-install\n",
            ["--reset"],
            flash_busy=True,
            reset_checkpoint=True,
        ) as child:
            completed(child, 90)
        verify_busy_reads(args, phase)
        state = phase / "state"
        require_stock_pc(state)
        read = reader(state)
        if not 0x80000 <= read(0x806F3) <= read(0x806F8) < 0x100000:
            raise ValueError("Invalid native stack geometry")
        if read(0x806FD) - read(0x806F8) != 5:
            raise ValueError("Guard configuration did not preserve the empty stack")
        return state

    def install(label, artifact):
        installed = root / (label + "-installed")
        # As in configuration_recovery, Stock constructs the fixture without
        # synthetic busy reads. Recovery itself still runs with the requested
        # busy model and verifies every operation's status-read budget.
        setup = SimpleNamespace(**(vars(args) | {"flash_busy_reads": 0}))
        transfer(
            setup,
            installed,
            source,
            package,
            "RECOVERY",
            execute=False,
            package_name=artifact,
            remote_commands=["-95 CF HOME CLEAR", "'RECOVERY' RCL 2 STO 'RECOVERY' PURGE"],
        )
        protected = slice(bank, bank + 0x20000)
        if (installed / "state/flash").read_bytes()[protected] != (source / "flash").read_bytes()[
            protected
        ]:
            raise ValueError("Recovery installation changed the protected target bank")
        baseline = boot(installed / "state", label + "-baseline")
        if (baseline / "flash").read_bytes() != (installed / "state/flash").read_bytes():
            raise ValueError("Clean guarded configuration changed Flash")
        read = reader(baseline)
        installed_recovery(
            (baseline / "flash").read_bytes(),
            (package / artifact).read_bytes(),
            LIBRARY_ID,
            args.target,
            read=read,
        )
        count = read(0x8611D, 3)
        if count != len(original_ids) + 1 or {
            read(0x86120 + 16 * i, 3) for i in range(count)
        } != original_ids | {LIBRARY_ID}:
            raise ValueError("Guarded library did not preserve the native catalog")
        return baseline

    baseline = install("guarded", "recovery-guarded.hp")
    fill_commands = [
        "-95 CF HOME CLEAR",
        rf'\{{171}} 1 "{prefix.decode()}" REPL \'HPCRES\' STO \{{187}} \'RMARK\' STO'.replace(
            "\\'", "'"
        ),
        rf'\{{171}} "x" 1 {RESERVE_POWER} START DUP + NEXT RMARK \{{187}} \'RESMAKE\' STO'.replace(
            "\\'", "'"
        ),
        rf"\{{171}} {{ {USER_ELEMENTS} }} 0. CON 'HPCFILL' STO \{{187}} 'FILLMAKE' STO",
        r"\{171} RESMAKE FILLMAKE \{187} 'HPCPAD' STO 'HPCPAD' RCL",
    ]
    filled = root / "filled"
    transfer(
        args,
        filled,
        baseline,
        build,
        "UNUSED",
        installed=True,
        execute=True,
        remote_commands=fill_commands,
    )
    before = filled / "state"
    read = reader(before)
    if home_string(read, NAME) != prefix + b"x" * (RESERVE_BYTES - len(prefix)):
        raise ValueError("Incorrect reservation fixture contents")
    verify_zero_real_array(read, b"HPCFILL", USER_ELEMENTS)
    gap = (read(0x806F8) - read(0x806F3)) // 2
    if not 0 < gap < len(program) - 8:
        raise ValueError("Reservation fixture did not establish low-memory pressure")
    fault = root / "fault"
    prepare(before, fault, args.slot, args.target, bound=True)
    repaired = boot(fault, "recovered")
    operations = verify_busy_reads(args, root / "recovered")
    if args.flash_busy_reads and not operations:
        raise ValueError("Recovery did not exercise the requested Flash busy model")
    verify_flash(
        (fault / "flash").read_bytes(),
        (repaired / "flash").read_bytes(),
        args.slot,
        args.target,
        retire=True,
        authority_slot=authority,
    )
    verify(
        SimpleNamespace(
            before=fault,
            after=repaired,
            elf=build / "recovery.elf",
            binary=build / "recovery.bin",
            package=build / "recovery-configuration.hp",
            nm=args.nm,
            slot=args.slot,
            target=args.target,
            operation="retire",
            relocated_image=True,
            authority_slot=authority,
            reset_checkpoint=root / "recovered/before-reset",
        )
    )
    rechecked = boot(repaired, "rechecked")
    if verify_busy_reads(args, root / "rechecked"):
        raise ValueError("Closed transaction triggered another Flash operation")
    if (root / "rechecked/before-reset.json").exists():
        raise ValueError("Closed transaction requested another reset")
    for state in (repaired, rechecked):
        verify_restart(baseline, repaired, state)
        verify_zero_real_array(reader(state), b"HPCFILL", USER_ELEMENTS)
        require_absent(reader(state), NAME)
        verify_retired(
            (repaired / "flash").read_bytes(),
            (state / "flash").read_bytes(),
            args.slot,
            args.target,
            authority_slot=authority,
        )
    print("Guarded low-memory recovery and repeated restart verified", flush=True)

    observed = install("observed", "recovery-observed.hp")
    require_marker(reader(observed))
    admission_filled = root / "admission-filled"
    transfer(
        args,
        admission_filled,
        observed,
        None,
        "UNUSED",
        installed=True,
        execute=True,
        remote_commands=["-95 CF HOME CLEAR 'RCDONE' PURGE", *fill_commands[1:]],
    )
    admission_before = admission_filled / "state"
    require_absent(reader(admission_before), MARKER)
    expected_reservation = prefix + b"x" * (RESERVE_BYTES - len(prefix))
    if home_string(reader(admission_before), NAME) != expected_reservation:
        raise ValueError("Admission fixture has the wrong reservation")
    verify_zero_real_array(reader(admission_before), b"HPCFILL", USER_ELEMENTS)
    admission_read = reader(admission_before)
    admission_gap = (admission_read(0x806F8) - admission_read(0x806F3)) // 2
    if not 0 < admission_gap < len(program) - 8:
        raise ValueError("Admission rejection fixture did not retain low-memory pressure")
    admission_rejected = []
    for label in ("closed", "unpublished", "bad-crc", "bank-identity", "dirty-tail"):
        fault = root / ("admission-" + label + "-fault")
        prepare(admission_before, fault, args.slot, args.target, bound=True)
        flash = bytearray((fault / "flash").read_bytes())
        # No authorized target mutation is pending in these rejection cases.
        # Keep the target intact so Stock startup can finish without recovery.
        flash[args.target : args.target + 4096] = (admission_before / "flash").read_bytes()[
            args.target : args.target + 4096
        ]
        if label == "closed":
            flash[args.slot] = flash[authority] = 0
        elif label == "unpublished":
            flash[authority] = 0x7F
        elif label == "bad-crc":
            flash[authority + 28] ^= 1
        elif label == "bank-identity":
            flash[(args.slot & ~0x1FFFF) + 0x100] ^= 1
        else:
            flash[authority + 32 + 4] = 0
        (fault / "flash").write_bytes(flash)
        phase_name = "admission-" + label + "-rejected"
        after = boot(fault, phase_name)
        read = reader(after)
        require_marker(read)
        if (after / "flash").read_bytes() != bytes(flash):
            raise ValueError("Rejected admission changed Flash")
        if verify_busy_reads(args, root / phase_name):
            raise ValueError("Rejected admission issued a Flash operation")
        if (root / phase_name / "before-reset.json").exists():
            raise ValueError("Rejected admission requested a reset")
        if home_string(read, NAME) != expected_reservation:
            raise ValueError("Rejected admission released the matching reservation")
        verify_zero_real_array(read, b"HPCFILL", USER_ELEMENTS)
        verify_restart(admission_before, after, after)
        admission_rejected.append(label)
        print(f"Admission rejected {label} without deletion or replay", flush=True)
    cases = {
        "type": "{ 3 } 0. CON 'HPCRES' STO",
        "capacity": f"\"{prefix.decode()}\" 'HPCRES' STO",
        "prefix": f"\"y\" 1 {RESERVE_POWER} START DUP + NEXT 'HPCRES' STO",
    }
    rejected = []
    for label, command in cases.items():
        installed = root / (label + "-installed")
        transfer(
            args,
            installed,
            observed,
            None,
            "UNUSED",
            installed=True,
            execute=False,
            remote_commands=["-95 CF HOME CLEAR 'RCDONE' PURGE", command],
        )
        before = installed / "state"
        require_absent(reader(before), MARKER)
        fault = root / (label + "-fault")
        prepare(before, fault, args.slot, args.target, bound=True)
        after = boot(fault, label + "-rejected")
        require_marker(reader(after))
        if (fault / "flash").read_bytes() != (after / "flash").read_bytes():
            raise ValueError("Rejected reservation changed Flash")
        for state in (before, after):
            read = reader(state)
            if label == "type":
                verify_zero_real_array(read, NAME, 3)
            elif home_string(read, NAME) != (
                prefix if label == "capacity" else b"y" * RESERVE_BYTES
            ):
                raise ValueError("Rejected reservation changed user data")
        rejected.append(label)
        print(f"Guard rejected {label} without deletion or replay", flush=True)
    (root / "result.json").write_text(
        json.dumps(
            {
                "status": "pass",
                "model": args.model,
                "initial_gap": gap,
                "reservation_bytes": RESERVE_BYTES,
                "user_elements": USER_ELEMENTS,
                "busy_operations": operations,
                "admission_rejected": admission_rejected,
                "admission_gap": admission_gap,
                "rejected": rejected,
            },
            indent=2,
        )
        + "\n"
    )
