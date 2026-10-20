"""Verify fresh selected-slot library storage and Stock restart discovery."""

import shutil
import struct

from installation import LIBRARY_ID, TARGET, fixture, verify_probe
from reservation import reader
from verify import installed_recovery, require, require_stock_pc, symbol_offsets


def verify_storage(before, after, build, nm):
    prior = (before / "flash").read_bytes()
    actual = (after / "flash").read_bytes()
    changed = [i for i, (a, b) in enumerate(zip(prior, actual, strict=True)) if a != b]
    require(changed, "Selected-slot witness did not store a library")
    slot = changed[0]
    _, original, layout = fixture()
    require(
        0x140105 <= slot < 0x200000
        and slot % 4 == 1
        and slot + len(original) < (slot & ~0x1FFFF) + 0x20000,
        "Stored library leaves a user-bank allocation",
    )
    require(
        slot + len(original) <= TARGET or slot >= TARGET + 4096,
        "Stored library overlaps the planned transaction target",
    )
    record = actual[slot : slot + len(original)]

    def word(at):
        return sum(
            ((record[(at + i) // 2] >> (4 * ((at + i) & 1))) & 15) << (4 * i) for i in range(8)
        )

    journal = word(layout["recovery_binding_nibble"] + 16)
    authority = word(layout["recovery_binding_nibble"] + 24)
    library, expected, _ = fixture(journal_slot=journal, authority_slot=authority)
    require(record == expected, "Stored record differs from independently rebound library")
    require(prior[slot : slot + len(record)] == b"\xff" * len(record), "Storage overwrote data")
    expected_flash = bytearray(prior)
    expected_flash[slot : slot + len(record)] = record
    base = verify_probe(
        before, after, build, nm, 0, slot, image="installation-store", expected_flash=expected_flash
    )
    symbols = symbol_offsets(build / "installation-store.elf", nm)
    ram = (after / "sram").read_bytes()
    require(
        struct.unpack_from("<4I", ram, base + symbols["preparation_result"])
        == (0, slot, journal, authority),
        "Storage departed from prepared coordinates",
    )
    at = base + symbols["prepared_library"]
    require(ram[at : at + len(record)] == record, "Stored and prepared records differ")
    result, operations = struct.unpack_from("<2I", ram, base + symbols["storage_result"])
    require(result == 0 and operations > 0, "Native storage did not report successful programming")
    return slot, library, operations


def run_storage(args, clean, root, *, transfer, emulator, completed, verify_busy_reads):
    stored = root / "selected-storage"
    transfer(args, stored, clean, args.build, "STORELIB", package_name="installation-store.hp")
    slot, library, operations = verify_storage(clean, stored / "state", args.build, args.nm)
    require(
        verify_busy_reads(args, stored) == operations, "Storage bypassed expected Flash polling"
    )
    restart = root / "selected-storage-restart"
    restart_storage(args, stored, restart, library, slot, emulator=emulator, completed=completed)
    print(
        "Native prepared library stored at selected slot and discovered after restart", flush=True
    )


def restart_storage(args, stored, restart, library, slot, *, emulator, completed):
    restart.mkdir()
    shutil.copytree(stored / "state", restart / "state")
    with emulator(
        args, restart, "10000 capture home\n12000 finish-install\n", ["--reset"]
    ) as child:
        completed(child, 90)
    state = restart / "state"
    require_stock_pc(state)
    require(
        (state / "flash").read_bytes() == (stored / "state/flash").read_bytes(),
        "Stock restart changed the stored library",
    )
    found, _ = installed_recovery(
        (state / "flash").read_bytes(), library, LIBRARY_ID, TARGET, read=reader(state)
    )
    require(found == slot, "Stock restart did not discover the exact stored library")
