"""Model job selection must preserve coverage and reject unsupported filters."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("matrix", ROOT / "tools/e2e-target-matrix.py")
assert SPEC is not None and SPEC.loader is not None
matrix = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(matrix)


class MatrixTests(unittest.TestCase):
    def test_all_six_models_remain_covered(self):
        for mode in ("direct", "stock"):
            selected = matrix.targets(ROOT / "tests/e2e/targets", mode)
            self.assertEqual(
                {item["target"] for item in selected},
                {"hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g"},
            )
            self.assertEqual(len(selected), 6)

    def test_automation_flags_and_family_filters(self):
        with tempfile.TemporaryDirectory() as temporary:
            profiles = Path(temporary)
            for target, fields in (
                ("stock", {}),
                ("direct", {"automated_stock_e2e": False}),
                ("disabled", {"automated_e2e": False}),
            ):
                (profiles / f"{target}.json").write_text(
                    json.dumps({"target": target, "family": "hp39", **fields})
                )
            self.assertEqual(
                matrix.targets(profiles, "stock"),
                [{"family": "hp39", "target": "stock"}],
            )
            self.assertEqual(len(matrix.targets(profiles, "direct", "hp39")), 2)
            for family, target in (
                ("hp48", None),
                ("hp39", "disabled"),
                (None, "direct"),
            ):
                with self.assertRaises(ValueError):
                    matrix.targets(profiles, "stock", family, target)


if __name__ == "__main__":
    unittest.main()
