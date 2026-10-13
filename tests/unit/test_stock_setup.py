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
            value = json.loads(path.read_text())
            for family in value.get("families", {None: None}):
                self.assertTrue(setup.load_commands(path, family))

    def test_family_commands_never_leak_to_another_model(self):
        path = ROOT / "tests/e2e/object-mutation/stock-setup.json"
        self.assertEqual(setup.load_commands(path, "hp48"), ["123 :0:PORTZERO STO"])
        self.assertEqual(
            setup.load_commands(path, "hp4950"),
            [
                "123 :0:PORTZERO STO",
                "456 :1:PORTONE STO",
                "987 :2:PORTTWO STO",
                "111 :2:PORTOLD STO :2:PORTOLD PURGE",
            ],
        )
        for family in (None, "hp39", "unknown"):
            with self.assertRaises(ValueError):
                setup.load_commands(path, family)

    def test_invalid_unselected_family_is_not_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "setup.json"
            path.write_text(
                json.dumps({"version": 2, "families": {"hp48": ["1"], "hp4950": ["1,exit"]}}),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                setup.load_commands(path, "hp48")

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
