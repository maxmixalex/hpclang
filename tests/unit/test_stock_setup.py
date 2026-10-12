import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("stock_setup", ROOT / "tools/stock-setup.py")
setup = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(setup)


class StockSetupTests(unittest.TestCase):
    def test_repository_contracts(self):
        for path in (ROOT / "tests/e2e").glob("*/stock-setup.json"):
            self.assertTrue(setup.load_commands(path))

    def test_rejects_invalid_schema_and_command_boundaries(self):
        invalid = [
            [],
            {"version": True, "commands": ["1"]},
            {"version": 2, "commands": ["1"]},
            {"version": 1, "commands": ["1"], "ignored": True},
        ]
        invalid += [
            {"version": 1, "commands": commands}
            for commands in ([], "1", [1], [""], [" "], ["1\n2"], ["1,exit"], ["é"], ["1"] * 33)
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "setup.json"
            for value in invalid:
                with self.subTest(value=value):
                    path.write_text(json.dumps(value), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        setup.load_commands(path)
