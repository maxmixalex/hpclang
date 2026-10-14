"""Runtime planning preserves coverage while separating Stock-only fixtures."""

import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("runtime_plan", ROOT / "tools/e2e-runtime-plan.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
PROFILES = [json.loads(path.read_text()) for path in (ROOT / "tests/e2e/targets").glob("*.json")]


class RuntimePlanTests(unittest.TestCase):
    def test_existing_universal_coverage(self):
        result = MODULE.plan([{"mode": "runtime", "keyboard": True}], PROFILES)
        self.assertEqual(len(result["builds"]), 1)
        self.assertEqual(len(result["direct"]), 3)
        self.assertEqual(len(result["stock"]), 6)
        self.assertEqual({job["model"]["target"] for job in result["direct"]}, {""})

    def test_stock_only_model_payloads(self):
        result = MODULE.plan(
            [{"mode": "notebook", "keyboard": False, "contexts": ["stock"], "build": "model"}],
            PROFILES,
        )
        self.assertEqual(result["direct"], [])
        self.assertEqual(len(result["builds"]), 6)
        self.assertEqual(len(result["stock"]), 6)
        for job in result["stock"]:
            self.assertEqual(job["suite"]["artifact"], f"notebook-{job['model']['target']}")
        self.assertTrue(all(build["stock"] for build in result["builds"]))

    def test_port_fixture_only_selects_supported_families(self):
        result = MODULE.plan(
            [
                {
                    "mode": "notebook-backup",
                    "keyboard": False,
                    "contexts": ["stock"],
                    "families": ["hp48", "hp4950"],
                }
            ],
            PROFILES,
        )
        self.assertEqual(result["direct"], [])
        self.assertEqual(len(result["stock"]), 3)
        self.assertEqual(len(result["builds"]), 1)

    def test_direct_model_builds_do_not_expand_to_other_calculators(self):
        result = MODULE.plan(
            [{"mode": "probe", "keyboard": False, "contexts": ["direct"], "build": "model"}],
            PROFILES,
        )
        self.assertEqual(len(result["direct"]), 6)
        self.assertTrue(all(job["model"]["target"] for job in result["direct"]))
        self.assertFalse(any(build["stock"] for build in result["builds"]))

    def test_invalid_schema_fails_before_building(self):
        base = {"mode": "runtime", "keyboard": True}
        for change in (
            {"extra": True},
            {"keyboard": 1},
            {"mode": "../bad"},
            {"contexts": []},
            {"contexts": ["stock", "stock"]},
            {"families": ["typo"]},
            {"build": "typo"},
            {"verifier": "../verify.py"},
        ):
            with self.subTest(change=change), self.assertRaises(ValueError):
                MODULE.plan([{**base, **change}], PROFILES)
        with self.assertRaises(ValueError):
            MODULE.plan([base, base], PROFILES)

    def test_disabled_stock_target_is_not_scheduled(self):
        profiles = [{**p, "automated_stock_e2e": False} for p in PROFILES]
        with self.assertRaises(ValueError):
            MODULE.plan([{"mode": "probe", "keyboard": False}], profiles)

    def test_artifact_collisions_are_rejected(self):
        with self.assertRaises(ValueError):
            MODULE.plan(
                [
                    {"mode": "probe-hp39gplus", "keyboard": False},
                    {"mode": "probe", "keyboard": False, "build": "model"},
                ],
                PROFILES,
            )
