import hashlib
import importlib.util
from pathlib import Path
import sys
import unittest


SOURCE = Path(__file__).resolve().parents[1] / "e2e/port-recovery"
sys.path.insert(0, str(SOURCE))
SPEC = importlib.util.spec_from_file_location("reservation_verify", SOURCE / "reservation.py")
RESERVATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RESERVATION)


def unpack(data):
    return [nibble for byte in data for nibble in (byte & 15, byte >> 4)]


def packed(values):
    values = list(values)
    if len(values) % 2:
        values.append(0)
    return bytes(values[i] | values[i + 1] << 4 for i in range(0, len(values), 2))


class ObservedIdentityTests(unittest.TestCase):
    def setUp(self):
        self.pack = RESERVATION.packager()
        image = bytes.fromhex("1eff2fe1") + bytes(12) + b"HPCBIND1" + bytes(8)
        image += b"HPCROOT1" + bytes(40)
        empty = self.pack.immediate(0x2D9D) + self.pack.immediate(0x312B)
        configuration = (
            self.pack.immediate(0x2D9D)
            + self.pack.immediate(0x389B9)
            + self.pack.immediate(0x389D4)
            + self.pack.immediate(0x312B)
        )
        program = self.pack._recovery_program(
            self.pack.package(image, len(image), model="hp50g", fresh_copy=True)
        )
        self.library = self.pack._configuration_library(
            configuration, RESERVATION.LIBRARY_ID, (empty,) * 6 + (program,)
        )
        layout = self.pack.recovery_layout(
            self.library, (("admission", 6, image, 16, True),), identity_offset=32
        )
        self.position = layout["identity_object_nibble"]
        self.library = self.pack.seal_recovery_identity(self.library, self.position)

    def check_identity(self, library):
        data = unpack(library[8:])
        span = 5 + sum(data[5 + i] << (4 * i) for i in range(5))
        descriptor = packed(data[self.position : self.position + 96])
        self.assertEqual(descriptor[:8], b"HPCROOT1")
        self.assertEqual(int.from_bytes(descriptor[8:12], "little"), span)
        self.assertEqual(int.from_bytes(descriptor[12:16], "little"), self.position)
        canonical = data[:span]
        canonical[self.position + 32 : self.position + 96] = [0] * 64
        canonical[-4:] = [0] * 4
        self.assertEqual(hashlib.sha256(packed(canonical)).digest(), descriptor[16:])
        crc = 0
        for nibble in data[5 : span - 4]:
            for bit in range(4):
                carry = (crc ^ (nibble >> bit)) & 1
                crc >>= 1
                if carry:
                    crc ^= 0x8408
        self.assertEqual(sum(data[span - 4 + i] << (4 * i) for i in range(4)), crc)
        return descriptor

    def test_marker_is_included_in_identity_and_native_crc(self):
        original = self.check_identity(self.library)
        observed = RESERVATION.observed_library(
            self.pack, self.library, identity_nibble=self.position
        )
        changed = self.check_identity(observed)
        self.assertNotEqual(changed[16:], original[16:])
        self.assertGreater(len(observed), len(self.library))
        # All command bytes before the descriptor remain exact. The configuration
        # marker is appended after the commands, without changing their addresses.
        self.assertEqual(
            unpack(observed[8:])[10 : self.position],
            unpack(self.library[8:])[10 : self.position],
        )
        self.assertEqual(
            observed,
            RESERVATION.observed_library(self.pack, self.library, identity_nibble=self.position),
        )

    def test_wrong_owned_coordinate_is_rejected(self):
        for position in (0, self.position - 1, self.position + 1, len(self.library) * 2):
            with self.subTest(position=position), self.assertRaises(ValueError):
                RESERVATION.observed_library(self.pack, self.library, identity_nibble=position)


if __name__ == "__main__":
    unittest.main()
