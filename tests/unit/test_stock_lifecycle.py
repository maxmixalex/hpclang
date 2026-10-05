import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "stock_lifecycle", ROOT / "tests/e2e/harness/stock-lifecycle.py"
)
lifecycle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lifecycle)


class StockLifecycleTests(unittest.TestCase):
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
        good = json.loads((ROOT / "tests/e2e/object-mutation/stock-lifecycle.json").read_text())
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
