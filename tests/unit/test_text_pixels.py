import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "text_pixels", ROOT / "tests/e2e/system-services/verify.py"
)
pixels = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pixels)


class TextPixelTests(unittest.TestCase):
    def test_visible_text_and_corruption(self):
        for height in (64, 80):
            with self.subTest(height=height), tempfile.TemporaryDirectory() as directory:
                prefix = Path(directory) / "text"
                header = f"P5\n132 {height}\n255\n".encode()
                blank = bytearray([255] * (132 * height))
                Path(f"{prefix}-199.pgm").write_bytes(header + blank)
                good = blank.copy()
                for y in (2, 20):
                    good[y * 132 + 2] = 0
                    good[(y + 1) * 132 + 3] = 0
                Path(f"{prefix}-200.pgm").write_bytes(header + good)
                Path(f"{prefix}-201.pgm").write_bytes(header + good)
                keyboard = blank.copy()
                keyboard[132 : 132 + 40] = bytes(40)
                pressed = keyboard.copy()
                pressed[48 * 132 + 16] = 0
                Path(f"{prefix}-202.pgm").write_bytes(header + pressed)
                Path(f"{prefix}-203.pgm").write_bytes(header + keyboard)
                pixels.verify(prefix, height)
                unrelated = keyboard.copy()
                unrelated[20 * 132 + 60] = 0
                for data in (pressed, blank, unrelated, keyboard[:-1]):
                    Path(f"{prefix}-203.pgm").write_bytes(header + data)
                    with self.assertRaises(AssertionError):
                        pixels.verify(prefix, height)
                Path(f"{prefix}-203.pgm").write_bytes(header + keyboard)
                bad = good.copy()
                bad[2 * 132 + 2] = 255
                for data in (bad, blank, good[:-1]):
                    Path(f"{prefix}-200.pgm").write_bytes(header + data)
                    with self.assertRaises(AssertionError):
                        pixels.verify(prefix, height)
                Path(f"{prefix}-200.pgm").write_bytes(header + good)
                Path(f"{prefix}-199.pgm").write_bytes(header + good)
                with self.assertRaises(AssertionError):
                    pixels.verify(prefix, height)


if __name__ == "__main__":
    unittest.main()
