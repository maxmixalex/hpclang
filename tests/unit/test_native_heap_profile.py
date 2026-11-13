"""Observer profiles use mapped NOR addresses, never update-file offsets."""

from pathlib import Path
import runpy
import subprocess
import unittest


HARNESS = Path(__file__).resolve().parents[1] / "e2e/harness"
ROM_IMAGE = runpy.run_path(str(HARNESS / "native-heap-profile.py"))["rom_image"]


class HeapProfileInput(unittest.TestCase):
    def test_full_nor_is_unchanged(self):
        data = bytes(range(256))
        self.assertIs(ROM_IMAGE(data), data)

    def test_update_preserves_header_and_mapping(self):
        for header in (
            b"KINPOUPDATEIMAGE",
            b"KINPOHP39G+IMAGE",
            b"KINPOHP40G+IMAGE",
            b"KINPOHP48GIIMAGE",
        ):
            data = header + b"payload"
            mapped = ROM_IMAGE(data, True)
            self.assertEqual(mapped[:0x4000], bytes(0x4000))
            self.assertEqual(mapped[0x4000:], data)

    def test_invalid_update(self):
        for data in (
            b"",
            b"KINPOUPDATEIMAG",
            b"x" * 32,
            b"KINPOUPDATEIMAGE" + bytes(0x1FC000),
        ):
            with self.assertRaises(ValueError):
                ROM_IMAGE(data, True)

    def test_disabled_capture_needs_no_rom_or_runner_paths(self):
        result = subprocess.run(
            [
                "bash",
                "-c",
                'HPCLANG_E2E_CAPTURE_NATIVE_HEAP=0; source "$1" /missing',
                "test",
                str(HARNESS / "native-heap-env.sh"),
            ],
            capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_invalid_capture_switch_rejects(self):
        result = subprocess.run(
            [
                "bash",
                "-c",
                'HPCLANG_E2E_CAPTURE_NATIVE_HEAP=invalid; source "$1" /missing',
                "test",
                str(HARNESS / "native-heap-env.sh"),
            ],
            capture_output=True,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn(b"must be 0 or 1", result.stderr)


if __name__ == "__main__":
    unittest.main()
