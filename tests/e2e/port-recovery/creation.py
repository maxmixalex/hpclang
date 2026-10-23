"""Creation-claim native fixtures; production restart ownership remains separate."""

import shutil
import struct
import zlib

from prepare import free_tail
from publication import reservation_value
from reservation import reader
from verify import home_string, require, require_stock_pc, symbol_offsets


def creation_record(binding, name, slot, phase):
    """Independent fixed-format oracle; phase is excluded from the immutable CRC."""
    if len(binding) != 80 or not 1 <= len(name) <= 127 or any(not 33 <= c <= 126 for c in name):
        raise ValueError("Invalid creation-claim fixture binding or name")
    data = bytearray(240)
    data[:4] = b"\0" + (478).to_bytes(3, "little")
    data[4:12] = b"HPCOWN1\0"
    struct.pack_into("<I", data, 12, slot)
    data[16] = phase
    data[17] = len(name)
    data[20:100] = binding
    data[100 : 100 + len(name)] = name
    struct.pack_into("<I", data, 236, zlib.crc32(data[:16] + data[17:236]))
    return data


def run_creation_publication(args, before, root, slot, *, transfer, verify_busy_reads):
    from installation import AUTHORITY_SLOT, JOURNAL_SLOT, TARGET, verify_probe
    from prepare import prepare

    claim_slot = 0x1E0105
    expected = bytearray((before / "flash").read_bytes())
    require(free_tail(expected, 0x1E0000) == claim_slot, "Creation fixture tail is occupied")
    reservation = reservation_value(expected, slot)
    expected[claim_slot : claim_slot + 240] = creation_record(
        reservation[:80], b"HPCLEASE", claim_slot, 0x7E
    )
    pair = root / "creation-expected-pair"
    prepare(before, pair, JOURNAL_SLOT, TARGET, bound=True)
    records = (pair / "flash").read_bytes()
    for address, size in ((JOURNAL_SLOT, 4124), (AUTHORITY_SLOT, 32)):
        expected[address : address + size] = records[address : address + size]
    expected[AUTHORITY_SLOT] = 0x7F
    phase = root / "creation-published"
    image = "installation-creation"
    transfer(
        args,
        phase,
        before,
        args.build,
        "CREATE",
        package_name=image + ".hp",
        remote_commands=[
            "-95 CF HOME CLEAR \"unrelated data\" 'HPCKEEP' STO 'CREATE' RCL 'CREATE' PURGE"
        ],
    )
    after = phase / "state"
    base = verify_probe(
        before, after, args.build, args.nm, 0, slot, image=image, expected_flash=expected
    )
    symbols = symbol_offsets(args.build / (image + ".elf"), args.nm)
    ram = (after / "sram").read_bytes()
    result = struct.unpack_from("<5I", ram, base + symbols["creation_result"])
    staging = struct.unpack_from("<2I", ram, base + symbols["creation_stage_result"])
    require(
        staging == (0, 0),
        "Creation witness did not stage its transaction",
    )
    require(result[:4] == (0xFFFFFFFC, 0, 0, 0), "Creation intent/receipt commit failed")
    require(
        result[4] > 0 and verify_busy_reads(args, phase) == result[4],
        "Creation Flash operations or busy-read budget differ",
    )
    require(
        home_string(reader(after), b"HPCLEASE") == reservation,
        "Created reservation differs from its bound snapshot",
    )
    require(
        home_string(reader(after), b"HPCKEEP") == b"unrelated data",
        "Creation publication changed unrelated HOME data",
    )
    checked = root / "creation-admission"
    checker = "installation-creation-admission"
    transfer(
        args,
        checked,
        after,
        args.build,
        "CHECKCREATE",
        package_name=checker + ".hp",
        remote_commands=["-95 CF HOME CLEAR 'CHECKCREATE' RCL 'CHECKCREATE' PURGE"],
    )
    state = checked / "state"
    # Publication leaves its checked predicate on the stack; CHECKCREATE's
    # command clears it before executing the independent admission program.
    base = verify_probe(
        after, state, args.build, args.nm, 0, slot, image=checker, baseline_results=1
    )
    symbols = symbol_offsets(args.build / (checker + ".elf"), args.nm)
    values = struct.unpack_from(
        "<3I", (state / "sram").read_bytes(), base + symbols["staged_creation_result"]
    )
    require(values == (0, 0xFFFFFFFC, 0), "Staged gate failed correct/wrong binding checks")
    require(verify_busy_reads(args, checked) == 0, "Staged gate wrote Flash")
    require(
        home_string(reader(state), b"HPCLEASE") == reservation,
        "Staged gate changed the reservation",
    )
    require(
        home_string(reader(state), b"HPCKEEP") == b"unrelated data",
        "Staged gate changed unrelated HOME data",
    )
    print("Native creation, staging and read-only admission verified", flush=True)


def run_creation_visibility(args, before, root, slot, *, emulator, completed, verify_busy_reads):
    flash = bytearray((before / "flash").read_bytes())
    binding = reservation_value(flash, slot)[:80]
    first = free_tail(flash, 0x1E0000)
    phases = (0xFF, 0x7F, 0x7E, 0x7C, 0)
    require(first + 240 * len(phases) < 0x200000, "No tail capacity for creation-claim fixtures")
    for index, status in enumerate(phases):
        address = first + index * 240
        flash[address : address + 240] = creation_record(binding, b"HPCLEASE", address, status)
    phase = root / "creation-claim-visibility"
    phase.mkdir()
    shutil.copytree(before, phase / "state")
    (phase / "state/flash").write_bytes(flash)
    with emulator(
        args, phase, "10000 capture home\n12000 finish-install\n", ["--reset"], flash_busy=True
    ) as child:
        completed(child, 90)
    after = phase / "state"
    require_stock_pc(after)
    require((after / "flash").read_bytes() == flash, "Stock changed a creation-claim allocation")
    original, actual = reader(before), reader(after)
    count = original(0x8611D, 3)
    require(
        actual(0x8611D, 3) == count
        and all(
            actual(0x86120 + 16 * i, 16) == original(0x86120 + 16 * i, 16) for i in range(count)
        ),
        "Creation-claim allocations changed the library catalog",
    )
    require(verify_busy_reads(args, phase) == 0, "Creation-claim visibility probe wrote Flash")
    print("Five creation-claim phases remain invisible to Stock after restart", flush=True)


def run_creation_abort(
    args, before, root, slot, *, transfer, emulator, completed, verify_busy_reads
):
    from installation import AUTHORITY_SLOT, JOURNAL_SLOT, verify_probe
    from reservation import require_absent
    from staging import staged_flash

    expected = bytearray(staged_flash(before))
    claim = 0x1E0105
    require(free_tail(expected, 0x1E0000) == claim, "Creation closure tail is occupied")
    binding = reservation_value((before / "flash").read_bytes(), slot)[:80]
    expected[claim : claim + 240] = creation_record(binding, b"HPCLEASE", claim, 0)
    for address, size in ((JOURNAL_SLOT, 4124), (AUTHORITY_SLOT, 32)):
        expected[address : address + 4] = b"\0" + (2 * size - 2).to_bytes(3, "little")
    phase = root / "creation-abort"
    image = "installation-creation-abort"
    transfer(
        args,
        phase,
        before,
        args.build,
        "CLOSECLAIM",
        package_name=image + ".hp",
        remote_commands=[
            "-95 CF HOME CLEAR \"unrelated data\" 'HPCKEEP' STO",
            "'CLOSECLAIM' RCL 'CLOSECLAIM' PURGE",
        ],
    )
    after = phase / "state"
    base = verify_probe(
        before, after, args.build, args.nm, 0, slot, image=image, expected_flash=expected
    )
    symbols = symbol_offsets(args.build / (image + ".elf"), args.nm)
    ram = (after / "sram").read_bytes()
    creation = struct.unpack_from("<5I", ram, base + symbols["creation_result"])
    staging = struct.unpack_from("<2I", ram, base + symbols["creation_stage_result"])
    closed = struct.unpack_from("<3I", ram, base + symbols["creation_close_result"])
    removed = struct.unpack_from("<12I", ram, base + symbols["publication_result"])
    require(creation[:4] == (0xFFFFFFFC, 0, 0, 0), "Closure omitted fresh creation receipt")
    require(staging == (0, 0) and closed == (0, 0, 0), "Creation closure or repeat failed")
    require(removed[2:4] == (0, 0) and removed[5:7] == (0, 0), "Release did not consume receipt")
    require(
        removed[4] > creation[4] and verify_busy_reads(args, phase) == removed[4],
        "Creation closure Flash operation accounting differs",
    )

    def verify_closed(state):
        require_stock_pc(state)
        require((state / "flash").read_bytes() == expected, "Closed records changed")
        read, original = reader(state), reader(before)
        require_absent(read, b"HPCLEASE")
        require(home_string(read, b"HPCKEEP") == b"unrelated data", "Closure changed user data")
        count = original(0x8611D, 3)
        require(
            read(0x8611D, 3) == count
            and all(
                read(0x86120 + 16 * i, 16) == original(0x86120 + 16 * i, 16) for i in range(count)
            ),
            "Creation closure changed the native catalog",
        )

    verify_closed(after)
    restart = root / "creation-abort-restart"
    restart.mkdir()
    shutil.copytree(after, restart / "state")
    with emulator(
        args, restart, "10000 capture home\n12000 finish-install\n", ["--reset"], flash_busy=True
    ) as child:
        completed(child, 90)
    verify_closed(restart / "state")
    require(verify_busy_reads(args, restart) == 0, "Closed claim programmed Flash after restart")
    print("Fresh creation cancelled, resource released and claim closed across restart", flush=True)
