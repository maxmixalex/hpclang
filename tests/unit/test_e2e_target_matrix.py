"""Model job selection must preserve coverage and reject unsupported filters."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("matrix", ROOT / "tools/e2e-target-matrix.py")
assert SPEC is not None and SPEC.loader is not None
matrix = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(matrix)


class MatrixTests(unittest.TestCase):
    def test_boot_dialog_is_relative_to_profile_and_clears_when_absent(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            profile = directory / "probe.json"
            data = {"target": "probe", "family": "hp39", "model": 2, "lcd_height": 64}
            pattern = directory / "dialog.pgm"
            pattern.write_bytes(b"P5\n1 1\n255\n\xff")
            command = [sys.executable, str(ROOT / "tools/e2e-target-env.py"), str(profile)]
            for value, expected_status in (("dialog.pgm", 0), ("missing.pgm", 2), ("bad\npath", 2)):
                profile.write_text(json.dumps({**data, "boot_dialog": value}))
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(result.returncode, expected_status, result.stderr)
                if expected_status == 0:
                    self.assertIn(f"HPCLANG_E2E_BOOT_DIALOG={pattern.resolve()}", result.stdout)
            profile.write_text(json.dumps(data))
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=True)
            self.assertIn("HPCLANG_E2E_BOOT_DIALOG=\n", result.stdout)

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
