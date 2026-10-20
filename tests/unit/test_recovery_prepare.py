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

    def test_installation_reservation_preserves_existing_records_and_other_banks(self):
        first = 0x140105
        self.flash[first : first + 8] = bytes.fromhex("000e000012345678")
        (self.source / "flash").write_bytes(self.flash)
        PREPARE.reserve_bank_tail(self.source, self.destination, 0x140000)
        actual = (self.destination / "flash").read_bytes()
        tail = first + 8
        expected = self.flash.copy()
        capacity = (0x160000 - tail - 1) & ~3
        expected[tail : tail + 4] = b"\0" + (capacity * 2 - 2).to_bytes(3, "little")
        self.assertEqual(actual, expected)
        self.assertEqual(actual[tail + capacity : 0x160000], b"\xff" * 3)
        self.assertEqual((self.source / "flash").read_bytes(), self.flash)
        self.assertEqual(
            (self.destination / "sram").read_bytes(), (self.source / "sram").read_bytes()
        )

    def test_installation_reservation_rejects_unresolved_or_malformed_banks(self):
        for header in (
            bytes.fromhex("7e0e0030"),
            bytes.fromhex("00000000"),
            bytes.fromhex("00ffff0f"),
            bytes.fromhex("ff000000"),
        ):
            with self.subTest(header=header):
                malformed = self.flash.copy()
                malformed[0x140105:0x140109] = header
                (self.source / "flash").write_bytes(malformed)
                with self.assertRaises(ValueError):
                    PREPARE.reserve_bank_tail(self.source, self.destination, 0x140000)
                self.assertFalse(self.destination.exists())

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

    def test_partial_bank_status_preserves_the_original_recovery_records(self):
        slot, target = 0x160105, 0x140000
        PREPARE.prepare(self.source, self.destination, slot, target, bound=True)
        erased = (self.destination / "flash").read_bytes()
        for status in range(0xF0, 0x100):
            destination = self.destination.with_name(f"status-{status:x}")
            PREPARE.prepare(self.source, destination, slot, target, bound=True, bank_status=status)
            expected = bytearray(erased)
            expected[target + 0x100] = status
            self.assertEqual((destination / "flash").read_bytes(), expected)
            self.assertEqual((self.source / "flash").read_bytes(), self.flash)
            self.assertEqual(
                (destination / "sram").read_bytes(), (self.source / "sram").read_bytes()
            )

    def test_partial_bank_status_rejects_impossible_tears_and_body_sectors(self):
        for status, target in (
            (0xEF, 0x140000),
            (256, 0x140000),
            (True, 0x140000),
            (240.0, 0x140000),
            (0xF0, 0x141000),
        ):
            with self.subTest(status=status, target=target), self.assertRaises(ValueError):
                PREPARE.prepare(self.source, self.destination, 0x160105, target, bank_status=status)
            self.assertFalse(self.destination.exists())

    def test_body_fault_preserves_bank_header_and_binds_the_exact_sector(self):
        for target in (0x141000, 0x154000, 0x15F000):
            with self.subTest(target=target):
                original = self.flash.copy()
                original[target : target + 4096] = bytes(range(256)) * 16
                (self.source / "flash").write_bytes(original)
                destination = self.destination.with_name(f"fault-{target:x}")
                slot = 0x160105
                PREPARE.prepare(self.source, destination, slot, target, bound=True)
                actual = (destination / "flash").read_bytes()
                record = actual[slot : slot + 4124]
                authority = actual[slot + 4124 : slot + 4124 + 32]
                self.assertEqual(int.from_bytes(record[16:20], "little"), target)
                self.assertEqual(record[24:4120], original[target : target + 4096])
                self.assertEqual(int.from_bytes(record[-4:], "little"), zlib.crc32(record[1:-4]))
                self.assertEqual(int.from_bytes(authority[20:24], "little"), target)
                self.assertEqual(authority[24:28], record[-4:])
                self.assertEqual(
                    int.from_bytes(authority[-4:], "little"), zlib.crc32(authority[1:-4])
                )
                expected = original.copy()
                expected[target : target + 4096] = b"\xff" * 4096
                expected[slot : slot + 4124 + 32] = record + authority
                self.assertEqual(actual, expected)
                self.assertEqual(actual[0x140000:0x141000], original[0x140000:0x141000])
                self.assertEqual((self.source / "flash").read_bytes(), original)
                self.assertEqual(
                    (destination / "sram").read_bytes(), (self.source / "sram").read_bytes()
                )

    def test_bound_fixture_adds_exact_independent_authority_record(self):
        slot, target = 0x160105, 0x140000
        PREPARE.prepare(self.source, self.destination, slot, target, bound=True)
        actual = (self.destination / "flash").read_bytes()
        root = slot + 4124
        record = actual[slot:root]
        binding = actual[root : root + 32]
        self.assertEqual(binding[:12], bytes.fromhex("7e3e0040") + b"HPCAUTH1")
        self.assertEqual(
            [int.from_bytes(binding[i : i + 4], "little") for i in (12, 16, 20, 24)],
            [root, slot, target, zlib.crc32(record[1:-4])],
        )
        self.assertEqual(int.from_bytes(binding[28:], "little"), zlib.crc32(binding[1:28]))
        expected = self.flash.copy()
        expected[target : target + 4096] = b"\xff" * 4096
        expected[slot : root + 32] = record + binding
        self.assertEqual(actual, expected)
        self.assertEqual((self.source / "flash").read_bytes(), self.flash)

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
            (0x160105, 0x141001),
            (0x160105, 0x13F000),
            (0x160105, 0x160000),
            (0x200105, 0x140000),
        ):
            with self.subTest(slot=slot, target=target), self.assertRaises(ValueError):
                PREPARE.prepare(self.source, self.destination, slot, target)
            self.assertFalse(self.destination.exists())

    def test_journal_can_follow_existing_allocations_without_modifying_them(self):
        first = 0x160105
        prefix = bytes.fromhex("000e000012345678530e001078563412")
        self.flash[first : first + len(prefix)] = prefix
        (self.source / "flash").write_bytes(self.flash)
        slot = first + len(prefix)
        PREPARE.prepare(self.source, self.destination, slot, 0x140000, bound=True)
        actual = (self.destination / "flash").read_bytes()
        self.assertEqual(actual[first:slot], prefix)
        record = actual[slot : slot + 4124]
        authority = actual[slot + 4124 : slot + 4156]
        self.assertEqual(int.from_bytes(record[12:16], "little"), slot)
        self.assertEqual(int.from_bytes(authority[16:20], "little"), slot)
        expected = self.flash.copy()
        expected[0x140000:0x141000] = b"\xff" * 4096
        expected[slot : slot + 4156] = record + authority
        self.assertEqual(actual, expected)
        self.assertEqual((self.source / "flash").read_bytes(), self.flash)

    def test_journal_rejects_payload_offsets_holes_and_insufficient_tail(self):
        first = 0x160105
        for extent, slot in ((16, first + 4), (16, first + 12), (0x3FDF0, first + 0x1FEF8)):
            with self.subTest(extent=extent, slot=slot):
                flash = self.flash.copy()
                flash[first : first + 4] = b"\0" + (extent - 2).to_bytes(3, "little")
                (self.source / "flash").write_bytes(flash)
                with self.assertRaises(ValueError):
                    PREPARE.prepare(self.source, self.destination, slot, 0x140000, bound=True)
                self.assertFalse(self.destination.exists())

    def test_journal_can_share_target_bank_but_not_target_sector(self):
        first, slot = 0x140105, 0x142105
        self.flash[first : first + 4] = b"\0" + (2 * (slot - first) - 2).to_bytes(3, "little")
        (self.source / "flash").write_bytes(self.flash)
        for target in (0x142000, 0x143000):
            with self.subTest(target=target), self.assertRaisesRegex(ValueError, "overlap"):
                PREPARE.prepare(self.source, self.destination, slot, target, bound=True)
            self.assertFalse(self.destination.exists())
        PREPARE.prepare(self.source, self.destination, slot, 0x140000, bound=True)
        actual = (self.destination / "flash").read_bytes()
        self.assertEqual(actual[0x141000:slot], self.flash[0x141000:slot])
        self.assertEqual(actual[slot + 4156 :], self.flash[slot + 4156 :])

    def test_callback_body_target_is_derived_and_rejects_changed_mapping(self):
        flash = self.flash.copy()
        header, callback = 0x61BE8, 0x6896F

        def write(address, value, width=5):
            for i in range(width):
                byte = 0x140000 + (address + i - 0x40000) // 2
                shift = 4 * ((address + i) & 1)
                flash[byte] = (flash[byte] & ~(15 << shift)) | ((value >> (4 * i) & 15) << shift)

        write(header, 229, 3)
        write(header + 18, callback - header - 18)
        write(callback, 0x2D9D)
        words = {
            0x8611D: 1,
            0x86120: 229,
            0x86123: header,
            0x86128: 0x22F5A,
            0x26BBB + 8 * 7: 0x22F5A,
        }

        def read(address, count=5):
            return words[address]

        self.assertEqual(PREPARE.configuration_body_sector(flash, read), 0x154000)
        for address, value in (
            (0x8611D, 128),
            (0x86120, 228),
            (0x86128, 0x22F4F),
            (0x86123, 0x7FFFF),
        ):
            with self.subTest(address=address):
                saved = words[address]
                words[address] = value
                with self.assertRaises(ValueError):
                    PREPARE.configuration_body_sector(flash, read)
                words[address] = saved
        write(callback, 0)
        with self.assertRaises(ValueError):
            PREPARE.configuration_body_sector(flash, read)
        write(header + 18, 5)
        write(header + 23, 0x2D9D)
        with self.assertRaises(ValueError):
            PREPARE.configuration_body_sector(flash, read)


if __name__ == "__main__":
    unittest.main()
