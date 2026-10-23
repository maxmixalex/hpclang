"""Native acceptance of guarded recovery storage, not a trusted installer."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import struct
import zlib
from types import SimpleNamespace

from prepare import changed_library_title, free_tail, prepare, reserve_bank_tail
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


def bound_header(library, identity_nibble, library_slot, journal, target, flash):
    data = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in library[8:])
    identity = bytes(
        int(data[i : i + 2][::-1], 16) for i in range(identity_nibble, identity_nibble + 96, 2)
    )
    capsule = bytes.fromhex("362030") + b"HPCSECT1"
    capsule += struct.pack("<3I", journal, target, 4096) + flash[target : target + 4096]
    return (
        b"HPCMEM2\0"
        + identity
        + struct.pack(
            "<6I", library_slot, journal, journal + 4124, target, zlib.crc32(capsule), RESERVE_BYTES
        )
    )


def packager():
    path = Path(__file__).resolve().parents[3] / "tools/package-recovery.py"
    spec = importlib.util.spec_from_file_location("recovery_packager", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def observed_library(pack, library, *, identity_nibble=None):
    """Append a completion witness without changing any generated guard command.

    The unmodified library is tested separately for successful recovery. Negative
    tests clear this marker before each reset, so unchanged data cannot pass just
    because configuration never ran. Only this fixture tail writes the marker.
    """
    data = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in library[8:])
    if identity_nibble is not None:
        # The marker changes library content, so an owned sealed admission must
        # receive a new descriptor. Never locate it through a signature search.
        descriptor = data[identity_nibble : identity_nibble + 96]
        magic = "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in b"HPCROOT1")
        if len(descriptor) != 96 or not descriptor.startswith(magic):
            raise ValueError("Observed library lacks the owned identity descriptor")
        placeholder = magic + "0" * 80
        data = data[:identity_nibble] + placeholder + data[identity_nibble + 96 :]

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
    observed = pack._configuration_library(
        configuration[:-10] + marker + configuration[-10:], LIBRARY_ID, commands
    )
    if identity_nibble is not None:
        observed = pack.seal_recovery_identity(observed, identity_nibble)
    return observed


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
    from creation import creation_record

    root = args.output / "reservation"
    root.mkdir()
    build = args.configuration_build
    authority = args.slot + 4124
    claim_slot = 0x1E0105  # Dedicated native fixture allocation, verified below.
    prefix = b"HPCMEM2"
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
            "--creation-claim-slot",
            str(claim_slot),
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
            "--layout-output",
            str(package / "recovery-layout.json"),
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
    layout = json.loads((package / "recovery-layout.json").read_text())
    (package / "recovery-observed.hp").write_bytes(
        observed_library(pack, library, identity_nibble=layout.get("identity_object_nibble"))
    )
    source = root / "install-source"
    bank = args.target & ~0x1FFFF
    reserve_bank_tail(clean, source, bank)
    # Keep Stock installation from consuming the fixture's claim allocation.
    # This is offline fixture construction, not durable publication evidence.
    source_flash = bytearray((source / "flash").read_bytes())
    if free_tail(source_flash, claim_slot & ~0x1FFFF) != claim_slot:
        raise ValueError("Creation claim fixture does not own the expected erased tail")
    source_flash[claim_slot : claim_slot + 4] = b"\0" + (478).to_bytes(3, "little")
    (source / "flash").write_bytes(source_flash)
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
        claim_flash = bytearray((baseline / "flash").read_bytes())
        artifact_bytes = (package / artifact).read_bytes()
        installed_slot, _ = installed_recovery(
            claim_flash, artifact_bytes, LIBRARY_ID, args.target, read=read
        )
        header = bound_header(
            artifact_bytes,
            layout["identity_object_nibble"],
            installed_slot,
            args.slot,
            args.target,
            claim_flash,
        )
        if claim_flash[claim_slot : claim_slot + 240] != b"\0\xde\x01\0" + b"\xff" * 236:
            raise ValueError("Stock consumed or changed the reserved claim fixture")
        claim_flash[claim_slot : claim_slot + 240] = creation_record(header, NAME, claim_slot, 0x7E)
        (baseline / "flash").write_bytes(claim_flash)
        return baseline

    def reservation_package(state, library, label):
        flash = (state / "flash").read_bytes()
        slot, _ = installed_recovery(flash, library, LIBRARY_ID, args.target, read=reader(state))
        header = bound_header(
            library, layout["identity_object_nibble"], slot, args.slot, args.target, flash
        )
        folder = package / label
        folder.mkdir()
        data = pack.immediate(0x2A2C) + pack.immediate(5 + 2 * len(header))
        data += "".join(f"{byte & 15:x}{byte >> 4:x}" for byte in header)
        (folder / "header.hp").write_bytes(
            b"HPHP49-C" + bytes(int(data[i : i + 2][::-1], 16) for i in range(0, len(data), 2))
        )
        return folder, header + b"x" * (RESERVE_BYTES - len(header))

    baseline = install("guarded", "recovery-guarded.hp")
    header_package, expected = reservation_package(baseline, library, "guarded-header")
    fill_commands = [
        "-95 CF HOME CLEAR",
        r"\{171} 1 BINDHDR REPL 'HPCRES' STO \{187} 'RMARK' STO",
        rf'\{{171}} "x" 1 {RESERVE_POWER} START DUP + NEXT RMARK \{{187}} \'RESMAKE\' STO'.replace(
            "\\'", "'"
        ),
        rf"\{{171}} {{ {USER_ELEMENTS} }} 0. CON 'HPCFILL' STO \{{187}} 'FILLMAKE' STO",
        r"\{171} RESMAKE 'BINDHDR' PURGE FILLMAKE \{187} 'HPCPAD' STO 'HPCPAD' RCL",
    ]
    filled = root / "filled"
    transfer(
        args,
        filled,
        baseline,
        header_package,
        "BINDHDR",
        package_name="header.hp",
        execute=True,
        remote_commands=fill_commands,
    )
    before = filled / "state"
    read = reader(before)
    if home_string(read, NAME) != expected:
        raise ValueError("Incorrect reservation fixture contents")
    verify_zero_real_array(read, b"HPCFILL", USER_ELEMENTS)
    gap = (read(0x806F8) - read(0x806F3)) // 2
    if not 0 < gap < len(program) - 8:
        raise ValueError("Reservation fixture did not establish low-memory pressure")
    fault = root / "fault"
    # Exercise partial header publication in the existing low-memory boot.
    # Body-sector configurations do not contain the native bank status byte.
    bank_status = 0xF0 if args.target % 0x20000 == 0 else None
    prepare(before, fault, args.slot, args.target, bound=True, bank_status=bank_status)
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
    # RAM loss must not make a valid Flash transaction unrecoverable. This
    # path has no reservation object to release and enough ordinary free RAM.
    missing_fault = root / "absent-fault"
    require_absent(reader(observed), NAME)
    prepare(observed, missing_fault, args.slot, args.target, bound=True)
    missing_after = boot(missing_fault, "absent-recovered")
    verify_flash(
        (missing_fault / "flash").read_bytes(),
        (missing_after / "flash").read_bytes(),
        args.slot,
        args.target,
        retire=True,
        authority_slot=authority,
    )
    verify_restart(observed, missing_after, missing_after)
    require_absent(reader(missing_after), NAME)
    require_marker(reader(missing_after))
    if args.flash_busy_reads and not verify_busy_reads(args, root / "absent-recovered"):
        raise ValueError("Absent-reservation recovery did not exercise the Flash driver")
    print("Absent reservation permits recovery without RAM release", flush=True)
    observed_package = (package / "recovery-observed.hp").read_bytes()
    header_package, expected_reservation = reservation_package(
        observed, observed_package, "observed-header"
    )
    admission_filled = root / "admission-filled"
    transfer(
        args,
        admission_filled,
        observed,
        header_package,
        "BINDHDR",
        package_name="header.hp",
        execute=True,
        remote_commands=["-95 CF HOME CLEAR 'RCDONE' PURGE", *fill_commands[1:]],
    )
    admission_before = admission_filled / "state"
    require_absent(reader(admission_before), MARKER)
    if home_string(reader(admission_before), NAME) != expected_reservation:
        raise ValueError("Admission fixture has the wrong reservation")
    verify_zero_real_array(reader(admission_before), b"HPCFILL", USER_ELEMENTS)
    admission_read = reader(admission_before)
    admission_gap = (admission_read(0x806F8) - admission_read(0x806F3)) // 2
    if not 0 < admission_gap < len(program) - 8:
        raise ValueError("Admission rejection fixture did not retain low-memory pressure")
    observed_slot, _ = installed_recovery(
        (admission_before / "flash").read_bytes(),
        observed_package,
        LIBRARY_ID,
        args.target,
        read=admission_read,
    )
    altered_package = changed_library_title(observed_package)
    altered_view = bytearray((admission_before / "flash").read_bytes())
    altered_view[observed_slot + 4 : observed_slot + 4 + len(altered_package) - 8] = (
        altered_package[8:]
    )
    installed_recovery(altered_view, altered_package, LIBRARY_ID, args.target, read=admission_read)
    binding_faults = {
        "resource-root": 24,
        "resource-location": 56,
        "resource-transaction": 72,
        "resource-capacity": 76,
    }
    admission_rejected = []
    for label in (
        "closed",
        "unpublished",
        "bad-crc",
        "bank-identity",
        "dirty-tail",
        "library-identity",
        "claim-missing",
        "claim-intent",
        "claim-binding",
        *binding_faults,
    ):
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
        elif label == "library-identity":
            flash[observed_slot + 4 : observed_slot + 4 + len(altered_package) - 8] = (
                altered_package[8:]
            )
        elif label == "dirty-tail":
            flash[authority + 32 + 4] = 0
        elif label == "claim-missing":
            flash[claim_slot + 4 : claim_slot + 240] = b"\xff" * 236
        elif label == "claim-intent":
            flash[claim_slot + 16] = 0x7F
        elif label == "claim-binding":
            wrong_binding = bytearray(flash[claim_slot + 20 : claim_slot + 100])
            wrong_binding[24] ^= 1
            flash[claim_slot : claim_slot + 240] = creation_record(
                wrong_binding, NAME, claim_slot, 0x7E
            )
        (fault / "flash").write_bytes(flash)
        retained = expected_reservation
        if label in binding_faults:
            ram = bytearray((fault / "sram").read_bytes())
            mapped = native_words(ram, flash)
            at = home_object(mapped, NAME) + 10 + 2 * binding_faults[label]
            physical = mapped.physical(at)
            if not 0x08000000 <= physical < 0x08080000:
                raise ValueError("Reservation fault leaves native RAM")
            ram[physical - 0x08000000] ^= 1 << (4 * (at & 1))
            retained = bytearray(expected_reservation)
            retained[binding_faults[label]] ^= 1
            if home_string(mapped, NAME) != retained:
                raise ValueError("Reservation fault did not change exactly its selected field")
            (fault / "sram").write_bytes(ram)
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
        if home_string(read, NAME) != retained:
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
                "partial_bank_status": bank_status,
                "admission_rejected": admission_rejected,
                "admission_gap": admission_gap,
                "rejected": rejected,
                "absent_reservation": True,
                "creation_claim_slot": claim_slot,
            },
            indent=2,
        )
        + "\n"
    )
