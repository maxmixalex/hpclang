import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "stock_lifecycle", ROOT / "tests/e2e/harness/stock-lifecycle.py"
)
lifecycle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lifecycle)


class StockLifecycleTests(unittest.TestCase):
    def test_transport_selection_rejects_malformed_contracts(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "e2e.stock-lifecycle.json"
            path.write_text(
                (ROOT / "tests/e2e/object-composite/stock-lifecycle.json").read_text(),
                encoding="utf-8",
            )
            env = dict(
                os.environ,
                ROOT=str(ROOT),
                APP_DIR=directory,
                HPCLANG_E2E_FAMILY="hp48",
                HPCLANG_STOCK_REQUIRE_OBJECT_PHASE="1",
                PATH=str(Path(sys.executable).parent) + os.pathsep + os.environ["PATH"],
            )
            command = [
                "bash",
                "-c",
                'set -eu; source "$ROOT/tests/e2e/harness/stock-lifecycle-env.sh"; '
                'test -z "$LIFECYCLE"; test -z "${HPCLANG_STOCK_REQUIRE_OBJECT_PHASE:-}"',
            ]
            result = subprocess.run(command, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            path.write_text('{"version":2,"families":{}}', encoding="utf-8")
            result = subprocess.run(command, env=env, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("requires at least one family", result.stderr)

    def test_family_profiles_preserve_hp39_and_add_classic_relaunches(self):
        profiles = lifecycle.load_contract(
            ROOT / "tests/e2e/object-mutation/stock-lifecycle.json",
            ROOT / "tests/e2e/sequences",
        )
        self.assertEqual(set(profiles), {"hp39", "hp48", "hp4950"})
        self.assertEqual(len(profiles["hp39"]["relaunches"]), 14)
        for family in ("hp48", "hp4950"):
            self.assertEqual(profiles[family]["initial_phase"], "0x4f530400")
            self.assertEqual(
                [step["phase"] for step in profiles[family]["relaunches"]],
                ["0x4f530401", "0x4f530402"],
            )

    def test_invalid_family_profiles_are_rejected(self):
        good = json.loads((ROOT / "tests/e2e/object-mutation/stock-lifecycle.json").read_text())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "contract.json"
            for families in ({}, [], {"unknown": good["families"]["hp39"]}, {"hp48": {}}):
                with self.subTest(families=families), self.assertRaises(ValueError):
                    path.write_text(json.dumps(good | {"families": families}), encoding="utf-8")
                    lifecycle.load_contract(path, ROOT / "tests/e2e/sequences")

    def test_repository_contract(self):
        for contract in (ROOT / "tests/e2e").glob("*/stock-lifecycle.json"):
            with self.subTest(contract=contract):
                lifecycle.load_contract(contract, ROOT / "tests/e2e/sequences")

    def test_stale_phase_and_unsettled_pass_are_rejected(self):
        good = {
            "status": "pass",
            "saw_pass": True,
            "returned": True,
            "return_settled": True,
            "object_store_phase": 0x4F530301,
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.json"
            path.write_text(json.dumps(good), encoding="utf-8")
            lifecycle.verify_result(path, "0x4f530301")
            for change in (
                {"object_store_phase": 0x4F530300},
                {"return_settled": False},
                {"returned": False},
                {"saw_pass": False},
                {"status": "timeout"},
            ):
                with self.subTest(change=change), self.assertRaises(ValueError):
                    path.write_text(json.dumps(good | change), encoding="utf-8")
                    lifecycle.verify_result(path, "0x4f530301")

    def test_contract_cannot_escape_sequences_or_silently_ignore_fields(self):
        good = json.loads((ROOT / "tests/e2e/object-composite/stock-lifecycle.json").read_text())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "contract.json"
            for change in (
                {"version": True},
                {"extra": 1},
                {"relaunches": []},
                {"initial_phase": "0x50415353"},
                {"relaunches": [{"sequence": "../escape.seq", "phase": "0x4f530301"}]},
            ):
                with self.subTest(change=change), self.assertRaises(ValueError):
                    path.write_text(json.dumps(good | change), encoding="utf-8")
                    lifecycle.load_contract(path, ROOT / "tests/e2e/sequences")


if __name__ == "__main__":
    unittest.main()
