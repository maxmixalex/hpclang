import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "package_recovery", ROOT / "tools/package-recovery.py"
)
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)


class RecoveryPackageTests(unittest.TestCase):
    def test_object_extent_payload_and_owned_storage(self):
        for size in (1, 8, 9, 24, 4096, 32768):
            with self.subTest(size=size):
                payload = bytes(i % 251 for i in range(size))
                initialized = (size + 7) & ~7
                runtime = initialized + 8192
                raw = PACK.package(payload, runtime, model="hp50g")
                self.assertEqual(raw[:8], b"HPHP49-C")
                nibbles = [n for byte in raw[8:] for n in (byte & 15, byte >> 4)]

                def word(offset):
                    return sum(nibbles[offset + i] << (4 * i) for i in range(5))

                self.assertEqual(word(0), 0x2D9D)
                self.assertEqual(word(5), 0x2DCC)
                end = 10 + word(10)
                self.assertEqual(word(end), 0x312B)
                self.assertLessEqual(len(nibbles) - end - 5, 1)
                # Extract from the tail independently of encoded instruction length.
                reserve = runtime * 2 + 15
                source = end - reserve - initialized * 2
                copied = bytes(
                    nibbles[i] | (nibbles[i + 1] << 4)
                    for i in range(source, source + initialized * 2, 2)
                )
                self.assertEqual(copied, payload + bytes(initialized - size))
                self.assertEqual(nibbles[end - reserve : end], [0] * reserve)
                for phase in range(16):
                    reserved_start = phase + end - reserve
                    aligned = (reserved_start + 15) & ~15
                    self.assertGreaterEqual(aligned, reserved_start)
                    self.assertLessEqual(aligned + runtime * 2, phase + end)

    def test_invalid_inputs(self):
        for payload, runtime, model in (
            (b"", 8, "hp50g"),
            (bytes(32769), 32776, "hp50g"),
            (bytes(9), 8, "hp50g"),
            (bytes(8), 9, "hp50g"),
            (bytes(8), -8, "hp50g"),
            (bytes(8), 0x80000, "hp50g"),
            (bytes(8), 8, "hp49gplus"),
        ):
            with (
                self.subTest(runtime=runtime, model=model),
                self.assertRaises(ValueError),
            ):
                PACK.package(payload, runtime, model=model)


if __name__ == "__main__":
    unittest.main()
