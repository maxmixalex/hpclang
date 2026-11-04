import importlib.util
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("armtoolbox_wrap", ROOT / "tools/armtoolbox-wrap.py")
WRAP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRAP)


class StockReservationTests(unittest.TestCase):
    def test_request_leaves_room_for_object_edits_without_changing_memory_modes(self):
        # Decode the on-wire Launcher V3 trailer independently. D is a four-bit
        # count of 16-byte blocks; bits 28+ select different memory/code modes.
        wrapped = WRAP.wrap(bytes(range(256)))
        entries, request, entry, offset, marker = struct.unpack("<5I", wrapped[-20:])
        self.assertEqual((entries, entry, offset, marker), (0, 0, 0, 0x3176B34C))
        self.assertEqual(request >> 28, 0)
        self.assertEqual((request >> 24) & 15, 15)
        self.assertEqual((request >> 20) & 15, 12)  # MMU + OS RAM, no S/U flags.
        self.assertEqual(request & 0xFFFFF, 0)  # No TEMPOB policy change.
        self.assertEqual(((request >> 24) & 15) * 16, 240)

    def test_envelope_preserves_stage0_and_native_object_extent(self):
        for payload in (b"", b"x", bytes(range(256))):
            with self.subTest(length=len(payload)):
                wrapped = WRAP.wrap(payload)
                self.assertEqual(wrapped[:8], b"HPHP49-C")
                nibbles = [n for byte in wrapped[8:] for n in (byte & 15, byte >> 4)]
                prologue = sum(n << (4 * i) for i, n in enumerate(nibbles[:5]))
                extent = sum(n << (4 * i) for i, n in enumerate(nibbles[5:10]))
                self.assertEqual(prologue, 0x02A2C)
                self.assertEqual(5 + extent, len(nibbles))
                self.assertEqual(wrapped[13:20], b"\x4c\xb3\x80A>CP")
                self.assertEqual(wrapped[20:-20], payload)
