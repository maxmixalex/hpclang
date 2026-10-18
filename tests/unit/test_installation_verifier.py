import importlib.util
from pathlib import Path
import struct
import sys
import unittest


SOURCE = Path(__file__).resolve().parents[1] / "e2e/port-recovery"
sys.path.insert(0, str(SOURCE))
SPEC = importlib.util.spec_from_file_location("installation_verify", SOURCE / "installation.py")
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class InstallationExecutionTests(unittest.TestCase):
    def setUp(self):
        self.initialized = bytes(range(32))
        self.symbols = {
            "__image_end": 128,
            "installation_result": 32,
            "admission_stack_low": 64,
            "admission_stack_high": 120,
        }
        self.package = VERIFY.packager().package(self.initialized, 128, model="hp50g")
        program = VERIFY.nibbles(self.package[8:])
        prefix_length = 10 + int(program[10:15][::-1], 16) - (128 * 2 + 15)
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


if __name__ == "__main__":
    unittest.main()
