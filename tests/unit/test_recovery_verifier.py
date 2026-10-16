import importlib.util
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zlib


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "recovery_verify", ROOT / "tests/e2e/port-recovery/verify.py"
)
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class RecoveryVerifierTests(unittest.TestCase):
    def setUp(self):
        self.slot, self.target = 0x160105, 0x140000
        self.record = bytearray(4124)
        self.record[:4] = bytes.fromhex("7e362030")
        self.record[4:12] = b"HPCSECT1"
        struct.pack_into("<3I", self.record, 12, self.slot, self.target, 4096)
        self.record[24:4120] = bytes(range(256)) * 16
        struct.pack_into("<I", self.record, 4120, zlib.crc32(self.record[1:4120]))
        self.before = bytearray(b"\xff" * 0x200000)
        self.before[self.slot : self.slot + 4124] = self.record
        self.after = self.before.copy()
        self.after[self.target : self.target + 4096] = self.record[24:4120]

    def test_replay_and_already_restored(self):
        record, repeated = VERIFY.verify_flash(self.before, self.after, self.slot, self.target)
        self.assertEqual(record, self.record)
        self.assertFalse(repeated)
        _, repeated = VERIFY.verify_flash(self.after, self.after, self.slot, self.target)
        self.assertTrue(repeated)

    def test_collateral_write_and_incomplete_replay_fail(self):
        for address in (
            0,
            self.target - 1,
            self.target,
            self.target + 4095,
            self.target + 4096,
            self.slot,
            self.slot + 4123,
            0x1FFFFF,
        ):
            with self.subTest(address=address):
                changed = self.after.copy()
                changed[address] ^= 1
                with self.assertRaises(ValueError):
                    VERIFY.verify_flash(self.before, changed, self.slot, self.target)

    def test_retirement_and_interrupted_status(self):
        retired = self.after.copy()
        retired[self.slot] = 0
        record, restored = VERIFY.verify_flash(
            self.before, retired, self.slot, self.target, retire=True
        )
        self.assertEqual(record, self.record)
        self.assertFalse(restored)
        for status in range(256):
            before = self.after.copy()
            before[self.slot] = status
            with self.subTest(status=status):
                if status and status & 0x7E == status:
                    record, restored = VERIFY.verify_flash(
                        before, retired, self.slot, self.target, retire=True
                    )
                    self.assertEqual(record, self.record)
                    self.assertTrue(restored)
                else:
                    with self.assertRaises(ValueError):
                        VERIFY.verify_flash(before, retired, self.slot, self.target, retire=True)

    def test_retirement_preserves_neighbor_and_requires_target(self):
        retired = self.after.copy()
        retired[self.slot] = 0
        for address in (self.slot - 1, self.slot, self.slot + 1, self.target, self.target + 4096):
            changed = retired.copy()
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                VERIFY.verify_flash(self.before, changed, self.slot, self.target, retire=True)
        partial = self.before.copy()
        partial[self.slot] = 0x3E
        with self.assertRaises(ValueError):
            VERIFY.verify_flash(partial, retired, self.slot, self.target, retire=True)

    def test_clean_invocation_keeps_retired_transaction(self):
        retired = self.after.copy()
        retired[self.slot] = 0
        self.assertEqual(
            VERIFY.verify_retired(retired, retired, self.slot, self.target), self.record
        )
        for address in (0, self.slot - 1, self.slot, self.slot + 1, self.target, 0x1FFFFF):
            changed = retired.copy()
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                VERIFY.verify_retired(retired, changed, self.slot, self.target)

    def test_clean_invocation_rejects_stale_or_malformed_evidence(self):
        retired = self.after.copy()
        retired[self.slot] = 0
        for address in (self.slot, self.slot + 12, self.slot + 24, self.slot + 4120, self.target):
            changed = retired.copy()
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                VERIFY.verify_retired(changed, changed, self.slot, self.target)
        with self.assertRaises(ValueError):
            VERIFY.verify_retired(retired[:-1], retired[:-1], self.slot, self.target)
        with self.assertRaises(ValueError):
            VERIFY.verify_retired(retired, retired, -1, self.target)

    def test_corrupt_journal_is_not_accepted_as_an_oracle(self):
        for offset in (0, 1, 3, 4, 12, 16, 20, 24, 4119, 4120):
            with self.subTest(offset=offset):
                before, after = self.before.copy(), self.after.copy()
                before[self.slot + offset] ^= 1
                after[self.slot + offset] ^= 1
                with self.assertRaises(ValueError):
                    VERIFY.verify_flash(before, after, self.slot, self.target)

    def test_truncated_state_and_bad_geometry_fail(self):
        for before, after, slot, target in (
            (self.before[:-1], self.after, self.slot, self.target),
            (self.before, self.after[:-1], self.slot, self.target),
            (self.before, self.after, self.slot, 0x140001),
            (self.before, self.after, self.target, self.target),
            (self.before, self.after, -1, self.target),
        ):
            with self.assertRaises(ValueError):
                VERIFY.verify_flash(before, after, slot, target)


class RecoveryPowerCutTests(unittest.TestCase):
    def setUp(self):
        RecoveryVerifierTests.setUp(self)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.paths = [root / name for name in ("before", "after", "log")]

    def check(self, before, after, event, cut):
        self.paths[0].write_bytes(before)
        self.paths[1].write_bytes(after)
        self.paths[2].write_text(event)
        return VERIFY.verify_power_cut(*self.paths, self.slot, self.target, cut)

    def test_retirement_cut_preserves_target_and_neighbor(self):
        before = self.after.copy()
        before[self.slot - 1] = 0x35
        after = before.copy()
        after[self.slot] = 0x6E
        event = (
            f"hpclang-flash-cut operation=1 address={self.slot - 1} bytes=2 "
            "value=53 mask=4096 reads=16\n"
        )
        self.paths[0].write_bytes(before)
        self.paths[1].write_bytes(after)
        self.paths[2].write_text(event)
        VERIFY.verify_retirement_cut(*self.paths, self.slot, self.target, (1, 16, 0x1000))
        for address in (self.slot - 1, self.target, self.slot + 1):
            changed = after.copy()
            changed[address] ^= 1
            self.paths[1].write_bytes(changed)
            with self.subTest(address=address), self.assertRaises(ValueError):
                VERIFY.verify_retirement_cut(*self.paths, self.slot, self.target, (1, 16, 0x1000))
        self.paths[1].write_bytes(after)
        self.paths[2].write_text(event + event)
        with self.assertRaises(ValueError):
            VERIFY.verify_retirement_cut(*self.paths, self.slot, self.target, (1, 16, 0x1000))

    def test_partial_first_program_and_evidence_rejections(self):
        # First requested word is 0x0100. Completing alternating bits leaves
        # 0xABAA; the identity and every other sector byte remain erased.
        torn = self.before.copy()
        torn[self.target : self.target + 2] = bytes.fromhex("aaab")
        event = f"hpclang-flash-cut operation=2 address={self.target} bytes=2 value=256 mask=21845 reads=16\n"
        self.check(self.before, torn, event, (2, 16, 0x5555))
        for invalid in (
            "",
            event + event,
            event.replace("reads=16", "reads=15"),
            event.replace("value=256", "value=257"),
        ):
            with self.assertRaises(ValueError):
                self.check(self.before, torn, invalid, (2, 16, 0x5555))
        for address in (0, self.target, self.target + 0x100, self.slot, len(torn) - 1):
            changed = torn.copy()
            changed[address] ^= 1
            with self.assertRaises(ValueError):
                self.check(self.before, changed, event, (2, 16, 0x5555))

    def test_partial_erase_preserves_unselected_bits(self):
        before = self.before.copy()
        before[self.target : self.target + 4096] = b"\x10\x20" * 2048
        after = before.copy()
        after[self.target : self.target + 4096] = b"\x55\x75" * 2048
        event = f"hpclang-flash-cut operation=1 address={self.target} bytes=4096 value=65535 mask=21845 reads=16\n"
        self.check(before, after, event, (1, 16, 0x5555))
        with self.assertRaises(ValueError):
            self.check(before, self.before, event, (1, 16, 0x5555))

    def test_identity_cut_distinguishes_partial_from_completed_write(self):
        event = f"hpclang-flash-cut operation=2049 address={self.target + 0x100} bytes=2 value=256 mask=65535 reads=16\n"
        self.assertTrue(self.check(self.before, self.after, event, (2049, 16, 65535)))
        torn = self.after.copy()
        torn[self.target + 0x100 : self.target + 0x102] = b"\x00\xff"
        self.assertFalse(
            self.check(self.before, torn, event.replace("mask=65535", "mask=255"), (2049, 16, 255))
        )


class RecoveryInterruptionTests(unittest.TestCase):
    def setUp(self):
        RecoveryVerifierTests.setUp(self)
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.args = SimpleNamespace(
            before=root / "before",
            after=root / "after",
            slot=self.slot,
            target=self.target,
            elf=root / "image.elf",
            binary=root / "image.bin",
            nm="unused",
        )
        self.args.before.mkdir()
        self.args.after.mkdir()
        self.after[self.target + 0x100 : self.target + 0x102] = b"\xff\xff"
        (self.args.before / "flash").write_bytes(self.before)
        (self.args.after / "flash").write_bytes(self.after)
        self.symbols = dict(
            workspace=64,
            guard_low=8464,
            stack_start=8472,
            stack_end=12568,
            guard_high=12568,
            __image_end=12576,
        )
        self.start = 0x1000
        self.physical = 0x08000000 + self.start
        self.ram = bytearray(0x80000)
        image = bytes(range(64))
        self.args.binary.write_bytes(image)
        self.ram[self.start : self.start + len(image)] = image
        workspace = self.start + self.symbols["workspace"]
        struct.pack_into(
            "<Ii4I",
            self.ram,
            workspace,
            0x48504641,
            1,
            self.slot,
            self.target,
            1,
            self.physical + self.symbols["stack_start"] + 32,
        )
        self.ram[workspace + 24 : workspace + 24 + 4124] = self.record
        self.ram[workspace + 24 + 4124 : workspace + 24 + 8248] = self.record
        struct.pack_into(
            "<2I", self.ram, self.start + self.symbols["guard_low"], 0x13579BDF, 0x2468ACE0
        )
        struct.pack_into(
            "<2I", self.ram, self.start + self.symbols["guard_high"], 0x89ABCDEF, 0xFEDCBA98
        )
        struct.pack_into("<6I", self.ram, workspace + 8272 + 26 * 4, 0, 0xFFFFFFFC, 1, 2047, 0, 0)
        (self.args.after / "sram").write_bytes(self.ram)
        self.write_cpu()
        mock = patch.object(VERIFY, "symbol_offsets", return_value=self.symbols)
        mock.start()
        self.addCleanup(mock.stop)

    def write_cpu(self, *, pc=None, sp=None, cpsr=0xD0):
        pc = self.physical + 16 if pc is None else pc
        sp = self.physical + self.symbols["stack_start"] + 32 if sp is None else sp
        (self.args.after / "state").write_text(
            f"[s3c2410-arm]\nreg-15={pc:#x}\nreg-13={sp:#x}\ncpsr={cpsr:#x}\n"
        )

    def test_owned_interruption_is_accepted(self):
        VERIFY.verify_interruption(self.args)

    def test_separate_reload_must_match_inspected_journal(self):
        offset = self.start + self.symbols["workspace"] + 24 + 4124
        self.ram[offset + 24] ^= 1
        (self.args.after / "sram").write_bytes(self.ram)
        with self.assertRaisesRegex(ValueError, "separate journal reload"):
            VERIFY.verify_interruption(self.args)

    def test_early_identity_or_incomplete_body_is_rejected(self):
        for offset in (0, 0x100, 4095):
            changed = self.after.copy()
            changed[self.target + offset] ^= 1
            (self.args.after / "flash").write_bytes(changed)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                VERIFY.verify_interruption(self.args)

    def test_released_execution_ownership_is_rejected(self):
        for changes in (dict(pc=0x8428), dict(sp=0x08000000), dict(cpsr=0x10)):
            self.write_cpu(**changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                VERIFY.verify_interruption(self.args)

    def test_wrong_result_or_broken_guard_is_rejected(self):
        for offset in (
            40,
            self.symbols["workspace"],
            self.symbols["guard_low"],
            self.symbols["workspace"] + 8272 + 31 * 4,
        ):
            changed = self.ram.copy()
            changed[self.start + offset] ^= 1
            (self.args.after / "sram").write_bytes(changed)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                VERIFY.verify_interruption(self.args)


class RecoveryRestartTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.clean, self.repaired, self.restarted = (
            root / name for name in ("clean", "repaired", "restarted")
        )
        self.catalog = [(1, 0x12345, 0x20000, 0), (124, 0x67890, 0x22000, 1)]
        for directory in (self.clean, self.repaired, self.restarted):
            directory.mkdir()
            (directory / "flash").write_bytes(b"\xff" * 0x200000)
            self.write_native(directory, self.catalog)

    def write_native(self, directory, catalog, *, depth=0, pc=0x8428):
        ram = bytearray(0x80000)
        # A contiguous native main-RAM read map, independently populated with
        # the HOME prologue, empty stack and catalog records used by the oracle.
        for page in range(128):
            struct.pack_into("<I", ram, page * 4, 0x08000000 + page * 2048)

        def write(address, value, width=5):
            for i in range(width):
                offset, half = divmod(address + i - 0x80000, 2)
                shift = half * 4
                ram[offset] = (ram[offset] & ~(15 << shift)) | (((value >> (i * 4)) & 15) << shift)

        write(0x80711, 0x90000)
        write(0x90000, 0x2A96)
        write(0x806F8, 0x94000)
        write(0x806FD, 0x94000 + 5 * (depth + 1))
        write(0x8611D, len(catalog), 3)
        for index, record in enumerate(catalog):
            for value, (offset, width) in zip(record, ((0, 3), (3, 5), (8, 5), (13, 3))):
                write(0x86120 + 16 * index + offset, value, width)
        (directory / "sram").write_bytes(ram)
        (directory / "state").write_text(f"[s3c2410-arm]\nreg-15={pc:#x}\n")

    def test_native_catalog_reordering_is_allowed(self):
        self.write_native(self.restarted, self.catalog[::-1])
        self.assertEqual(VERIFY.verify_restart(self.clean, self.repaired, self.restarted), 2)

    def test_missing_library_or_changed_binding_switch_flags_are_rejected(self):
        variants = [self.catalog[:1]]
        for field in range(4):
            altered = list(self.catalog[1])
            altered[field] ^= 1
            variants.append([self.catalog[0], tuple(altered)])
        for catalog in variants:
            with self.subTest(catalog=catalog):
                self.write_native(self.restarted, catalog)
                with self.assertRaisesRegex(ValueError, "original native library catalog"):
                    VERIFY.verify_restart(self.clean, self.repaired, self.restarted)

    def test_flash_change_is_rejected(self):
        changed = bytearray((self.restarted / "flash").read_bytes())
        changed[0x160105] = 0
        (self.restarted / "flash").write_bytes(changed)
        with self.assertRaisesRegex(ValueError, "reset changed Flash"):
            VERIFY.verify_restart(self.clean, self.repaired, self.restarted)

    def test_unfinished_return_and_nonempty_stack_are_rejected(self):
        for depth, pc in ((0, 0x08010000), (1, 0x8428)):
            with self.subTest(depth=depth, pc=pc):
                self.write_native(self.restarted, self.catalog, depth=depth, pc=pc)
                with self.assertRaises(ValueError):
                    VERIFY.verify_restart(self.clean, self.repaired, self.restarted)


if __name__ == "__main__":
    unittest.main()
