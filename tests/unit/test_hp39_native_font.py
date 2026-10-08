"""ROM table identity must be independent of agreement with rendered pixels."""

import contextlib
from dataclasses import replace
import importlib.util
import io
import lzma
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "hp39_font_oracle", ROOT / "tests/e2e/harness/verify-hp39-native-font.py"
)
oracle = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = oracle
SPEC.loader.exec_module(oracle)


class NativeFontIdentityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.nor = lzma.decompress((ROOT / "tests/e2e/fixtures/hp39gplus/flash.bin.xz").read_bytes())
        cls.bases = oracle.BASES["hp39gplus"]

    def test_d155_matches_reviewed_ascii_and_editline(self):
        with contextlib.redirect_stdout(io.StringIO()):
            oracle.verify_ascii_anchors(self.nor, self.bases)
            oracle.verify_editline_fingerprint(self.nor, self.bases, "hp39gplus")

    def test_wrong_table_location_cannot_validate_itself(self):
        for field in ("main", "mini"):
            with self.subTest(font=field), self.assertRaises(ValueError):
                shifted = replace(self.bases, **{field: getattr(self.bases, field) + 1})
                oracle.verify_ascii_anchors(self.nor, shifted)
        shifted = replace(self.bases, editline=self.bases.editline + 1)
        with self.assertRaises(SystemExit):
            oracle.verify_editline_fingerprint(self.nor, shifted, "hp39gplus")


if __name__ == "__main__":
    unittest.main()
