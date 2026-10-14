"""Media evidence rejects unsafe paths and detects content corruption."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("stock_media", ROOT / "tools/e2e-stock-media.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class MediaTests(unittest.TestCase):
    def read(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "media.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            return MODULE.manifest(path)

    def test_fixture_declares_utf8_output_and_existing_sentinel(self):
        data = MODULE.manifest(ROOT / "tests/e2e/notebook-export/stock-media.json")
        self.assertIn("αβ", data["expected_files"]["HPCNOTE.TXT"])
        self.assertIn("KEEP.TXT", data["initial_files"])

    def test_invalid_schema_and_paths(self):
        base = {"version": 1, "initial_files": {}, "expected_files": {"OUT.TXT": "text"}}
        for change in (
            {"version": True},
            {"version": 2},
            {"extra": 0},
            {"initial_files": []},
            {"expected_files": {}},
            {"expected_files": {"OUT.TXT": 123}},
            {"expected_files": {"OUT.TXT": "x" * 4097}},
            {"expected_files": {"../OUT": "bad"}},
            {"initial_files": {"bad\nname": "bad"}},
        ):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.read({**base, **change})

    def test_oracle_checks_retained_and_exported_bytes(self):
        data = {"initial_files": {"KEEP.TXT": "keep"}, "expected_files": {"OUT.TXT": "α\n"}}
        with patch.object(MODULE.subprocess, "check_output", side_effect=[b"keep", "α\n".encode()]):
            MODULE.verify(data, Path("card.img"))
        for contents in ([b"damaged", b"unused"], [b"keep", b"wrong"]):
            with patch.object(MODULE.subprocess, "check_output", side_effect=contents):
                with self.assertRaises(ValueError):
                    MODULE.verify(data, Path("card.img"))
