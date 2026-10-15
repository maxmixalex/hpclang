import importlib.util
from pathlib import Path
import struct
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


if __name__ == "__main__":
    unittest.main()
