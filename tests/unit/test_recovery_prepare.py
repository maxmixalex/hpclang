import importlib.util
from pathlib import Path
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "recovery_prepare", ROOT / "tests/e2e/port-recovery/prepare.py"
)
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)


class RecoveryPrepareTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.source = Path(self.directory.name) / "source"
        self.destination = Path(self.directory.name) / "fault"
        self.source.mkdir()
        self.flash = bytearray(b"\xff" * 0x200000)
        for bank in range(6):
            base = 0x140000 + bank * 0x20000
            self.flash[base + 0x100 : base + 0x105] = (0xF0 + ((bank + 8) << 8)).to_bytes(
                5, "little"
            )
        (self.source / "flash").write_bytes(self.flash)
        (self.source / "sram").write_bytes(b"original RAM; requires hardware reset")

    def test_only_target_and_journal_change_and_source_is_preserved(self):
        PREPARE.prepare(self.source, self.destination, 0x160105, 0x140000)
        actual = (self.destination / "flash").read_bytes()
        self.assertEqual((self.source / "flash").read_bytes(), self.flash)
        self.assertEqual(
            (self.source / "sram").read_bytes(), (self.destination / "sram").read_bytes()
        )
        record = actual[0x160105 : 0x160105 + 4124]
        self.assertEqual(record[24:4120], self.flash[0x140000:0x141000])
        self.assertEqual(zlib.crc32(record[1:4120]), int.from_bytes(record[4120:], "little"))
        expected = self.flash.copy()
        expected[0x140000:0x141000] = b"\xff" * 4096
        expected[0x160105 : 0x160105 + 4124] = record
        self.assertEqual(actual, expected)

    def test_existing_output_is_never_overwritten(self):
        self.destination.mkdir()
        retained = self.destination / "flash"
        retained.write_bytes(b"previous state")
        with self.assertRaises(FileExistsError):
            PREPARE.prepare(self.source, self.destination, 0x160105, 0x140000)
        self.assertEqual(retained.read_bytes(), b"previous state")

    def test_invalid_bank_or_nonempty_journal_bank_rejected_before_copy(self):
        for offset in (0x140100, 0x160100, 0x160105, 0x17FFFF):
            with self.subTest(offset=offset):
                changed = self.flash.copy()
                changed[offset] ^= 1
                (self.source / "flash").write_bytes(changed)
                with self.assertRaises(ValueError):
                    PREPARE.prepare(self.source, self.destination, 0x160105, 0x140000)
                self.assertFalse(self.destination.exists())

    def test_invalid_geometry_is_rejected(self):
        for slot, target in (
            (0x140105, 0x140000),
            (0x160106, 0x140000),
            (0x160105, 0x141000),
            (0x200105, 0x140000),
        ):
            with self.subTest(slot=slot, target=target), self.assertRaises(ValueError):
                PREPARE.prepare(self.source, self.destination, slot, target)
            self.assertFalse(self.destination.exists())


if __name__ == "__main__":
    unittest.main()
