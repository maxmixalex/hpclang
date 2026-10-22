"""Verify fresh selected-slot library storage and Stock restart discovery."""

import shutil
import struct

from installation import LIBRARY_ID, TARGET, fixture, verify_probe
from reservation import reader
from verify import installed_recovery, require, require_stock_pc, symbol_offsets


def verify_storage(before, after, build, nm, *, make_fixture=fixture, image="installation-store"):
    prior = (before / "flash").read_bytes()
    actual = (after / "flash").read_bytes()
    changed = [i for i, (a, b) in enumerate(zip(prior, actual, strict=True)) if a != b]
    require(changed, "Selected-slot witness did not store a library")
    slot = changed[0]
    _, original, layout = make_fixture()
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
    extra = {}
    if "admission_creation_nibble" in layout:
        extra["claim_slot"] = word(layout["admission_creation_nibble"])
    library, expected, _ = make_fixture(journal_slot=journal, authority_slot=authority, **extra)
    require(record == expected, "Stored record differs from independently rebound library")
    require(prior[slot : slot + len(record)] == b"\xff" * len(record), "Storage overwrote data")
    expected_flash = bytearray(prior)
    expected_flash[slot : slot + len(record)] = record
    base = verify_probe(
        before, after, build, nm, 0, slot, image=image, expected_flash=expected_flash
    )
    symbols = symbol_offsets(build / (image + ".elf"), nm)
    ram = (after / "sram").read_bytes()
    require(
        struct.unpack_from("<4I", ram, base + symbols["preparation_result"])
        == (0, slot, journal, authority),
        "Storage departed from prepared coordinates",
    )
    if extra:
        claim = extra["claim_slot"]
        require(
            struct.unpack_from("<I", ram, base + symbols["preparation_creation_slot"])[0] == claim,
            "Stored claim coordinate differs from preparation",
        )
        extents = [(slot, len(record)), (journal, 4124), (authority, 32), (claim, 240)]
        for index, (address, size) in enumerate(extents):
            require(
                0x140105 <= address < 0x200000
                and address % 4 == 1
                and address % 0x20000 >= 0x105
                and address + size < (address & ~0x1FFFF) + 0x20000,
                "Stored binding extent leaves native allocation space",
            )
            require(
                address + size <= TARGET or address >= TARGET + 4096,
                "Stored binding extent overlaps target",
            )
            require(
                prior[address : address + size] == b"\xff" * size,
                "Stored binding extent was not erased",
            )
            for other, length in extents[:index]:
                require(
                    address + size <= other or other + length <= address,
                    "Stored binding extents overlap",
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
    cut = root / "interrupted-storage"
    transfer(
        args,
        cut,
        clean,
        args.build,
        "STORECUT",
        power_cut=(operations, 1, 0x0400),
        package_name="installation-store.hp",
    )
    interrupted = (cut / "state/flash").read_bytes()
    complete = (stored / "state/flash").read_bytes()
    require(
        interrupted[slot] == 0xFB
        and interrupted[:slot] == complete[:slot]
        and interrupted[slot + 1 :] == complete[slot + 1 :],
        "Final library commit cut did not isolate the unpublished status",
    )
    reset = root / "interrupted-storage-restart"
    reset.mkdir()
    shutil.copytree(cut / "state", reset / "state")
    with emulator(args, reset, "10000 capture home\n12000 finish-install\n", ["--reset"]) as child:
        completed(child, 90)
    require_stock_pc(reset / "state")
    require(
        (reset / "state/flash").read_bytes() == interrupted,
        "Stock restart changed the interrupted library",
    )
    read, original = reader(reset / "state"), reader(clean)
    count = original(0x8611D, 3)
    require(
        read(0x8611D, 3) == count
        and all(read(0x86120 + 16 * i, 16) == original(0x86120 + 16 * i, 16) for i in range(count)),
        "Stock restart registered an unpublished library",
    )
    cleanup = root / "interrupted-storage-cleanup"
    transfer(
        args,
        cleanup,
        reset / "state",
        args.build,
        "CLEANLIB",
        package_name="installation-cleanup.hp",
    )
    cleaned = verify_cleanup(reset / "state", cleanup / "state", args.build, args.nm, slot)
    require(
        cleaned == 3 and verify_busy_reads(args, cleanup) == cleaned,
        "Interrupted library cleanup bypassed expected Flash polling",
    )
    print(
        "Interrupted library commit abandoned without changing other Flash or catalog", flush=True
    )


def restart_storage(
    args, stored, restart, library, slot, *, emulator, completed, library_id=LIBRARY_ID
):
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
        (state / "flash").read_bytes(), library, library_id, TARGET, read=reader(state)
    )
    require(found == slot, "Stock restart did not discover the exact stored library")


def verify_cleanup(before, after, build, nm, slot):
    prior = (before / "flash").read_bytes()
    status, metadata = prior[slot], prior[slot + 3]
    require(status != 0x53, "Cleanup fixture must never abandon a live library")
    expected = bytearray(prior)
    expected[slot] = 0
    expected[slot + 3] &= 15
    base = verify_probe(
        before,
        after,
        build,
        nm,
        0,
        0xFFFFFFFF,
        image="installation-cleanup",
        expected_flash=expected,
    )
    symbols = symbol_offsets(build / "installation-cleanup.elf", nm)
    result, operations = struct.unpack_from(
        "<2I", (after / "sram").read_bytes(), base + symbols["storage_result"]
    )
    needed = (
        int(bool(metadata & 0xF0)) + int(status != 0 and status & 0x53 == 0x53) + int(status != 0)
    )
    require(
        result == 0 and operations == needed, "Cleanup did not follow bounded header transition"
    )
    return operations
