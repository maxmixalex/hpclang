import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
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
