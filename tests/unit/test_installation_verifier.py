import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch


SOURCE = Path(__file__).resolve().parents[1] / "e2e/port-recovery"
sys.path.insert(0, str(SOURCE))
SPEC = importlib.util.spec_from_file_location("installation_verify", SOURCE / "installation.py")
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)
startup = importlib.import_module("startup")


class InstallationExecutionTests(unittest.TestCase):
    fresh_copy = False

    def setUp(self):
        self.initialized = bytes(range(32))
        self.symbols = {
            "__image_end": 128,
            "installation_result": 32,
            "admission_stack_low": 64,
            "admission_stack_high": 120,
        }
        self.package = VERIFY.packager().package(
            self.initialized, 128, model="hp50g", fresh_copy=self.fresh_copy
        )
        program = VERIFY.nibbles(self.package[8:])
        prefix_length = 10 + int(program[10:15][::-1], 16) - (128 * 2 + 15)
        if self.fresh_copy:
            prefix_length = 15 + len(VERIFY.packager().entry(32, 0, fresh_copy=True))
        self.owner = 0x400
        self.base = (self.owner + (prefix_length + 1) // 2 + 7) & ~7
        self.ram = bytearray(0x80000)
        self.ram[self.owner : self.owner + len(self.package) - 8] = self.package[8:]
        self.ram[self.base : self.base + 32] = self.initialized
        struct.pack_into(
            "<8I",
            self.ram,
            self.base + 32,
            0x48504944,
            0,
            0x1E0105,
            0x802,
            0x100802,
            0x802,
            0x100802,
            0,
        )
        for offset in (64, 120):
            struct.pack_into("<2I", self.ram, self.base + offset, 0x51A7B09D, 0x51A7B09D)

    def check(self, ram, package=None):
        VERIFY.verify_execution(
            ram,
            self.initialized,
            self.symbols,
            self.package if package is None else package,
            0,
            0x1E0105,
            fresh_copy=self.fresh_copy,
        )

    def test_owned_image_is_required_in_addition_to_result_marker(self):
        self.check(self.ram)
        for offset in (
            self.owner,
            self.base,
            self.base + 40,
            self.base + 52,
            self.base + 64,
            self.base + 120,
        ):
            changed = bytearray(self.ram)
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                self.check(changed)

    def test_incomplete_snapshot_and_invalid_package_are_rejected(self):
        with self.assertRaises(ValueError):
            self.check(self.ram[:-1])
        with self.assertRaises(ValueError):
            self.check(self.ram, b"HPHP49-C")
        with self.assertRaises(ValueError):
            self.check(self.ram, self.package[:13])


class FreshInstallationExecutionTests(InstallationExecutionTests):
    fresh_copy = True


class StartupInventoryTests(unittest.TestCase):
    def test_independent_layout_and_diagnostic_checks(self):
        flash = bytearray(0x200000)
        struct.pack_into("<II", flash, 0x248A0, 0x47FF1, 0x47FF2)
        flash[0x47FF1:0x48000] = bytes([14, 96] + [128] * 13)
        header, target = 0x40020, 0x40080

        def write_word(at, value, count):
            for j in range(count):
                byte, shift = (at + j) // 2, 4 * ((at + j) & 1)
                flash[byte] = (flash[byte] & ~(15 << shift)) | ((value >> (4 * j) & 15) << shift)

        physical_header = 0x48000 * 2 + header - 0x40000
        write_word(physical_header, 221, 3)
        write_word(physical_header + 18, target - header - 18, 5)
        words = {0x8611D: 1, 0x86120: 221, 0x86123: header, 0x86128: 0x12345}
        words.update({0x26BBB + 7 * i: 0x12345 + i for i in range(16)})
        ram = bytearray(64)
        struct.pack_into("<3I", ram, 0, 0, 1, 14)
        struct.pack_into("<4I", ram, 16, 221, header, 0, target)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "flash").write_bytes(flash)
            (root / "sram").write_bytes(ram)
            with (
                patch.object(startup, "native_words", return_value=lambda at, n=5: words[at]),
                patch.object(
                    startup,
                    "symbol_offsets",
                    return_value={"startup_result": 0, "startup_entries": 16},
                ),
            ):
                self.assertEqual(
                    startup.verify_startup(root, root, root, "unused", 0),
                    {"libraries": 1, "callbacks": 1},
                )
                for offset in (0, 4, 8, 16, 20, 24, 28):
                    changed = bytearray(ram)
                    changed[offset] ^= 1
                    (root / "sram").write_bytes(changed)
                    with self.subTest(diagnostic=offset), self.assertRaises(ValueError):
                        startup.verify_startup(root, root, root, "unused", 0)
                (root / "sram").write_bytes(ram)
                for offset in (0x47FF1, 0x47FF2, 0x47FFF, physical_header // 2):
                    changed = bytearray(flash)
                    changed[offset] ^= 1
                    (root / "flash").write_bytes(changed)
                    with self.subTest(rom=offset), self.assertRaises(ValueError):
                        startup.verify_startup(root, root, root, "unused", 0)


if __name__ == "__main__":
    unittest.main()
