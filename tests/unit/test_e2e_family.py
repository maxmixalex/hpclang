"""The CI matrix schedules parallel jobs; a family must not nest parallel guests."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("bash"), "family harness requires bash")
class FamilyTests(unittest.TestCase):
    def test_all_targets_run_serially_and_failure_is_preserved(self):
        for mode in ("direct", "stock"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                harness = root / "tests/e2e/harness"
                targets = root / "tests/e2e/targets"
                harness.mkdir(parents=True)
                targets.mkdir()
                (root / "tools").mkdir()
                (root / "tools/e2e-target-matrix.py").write_text(
                    (ROOT / "tools/e2e-target-matrix.py").read_text()
                )
                app = root / "app with spaces"
                app.mkdir()
                runner = harness / "run-family.sh"
                runner.write_text((ROOT / "tests/e2e/harness/run-family.sh").read_text())
                for target in ("c", "a", "b"):
                    (targets / f"{target}.json").write_text(
                        json.dumps({"target": target, "family": "hp39"})
                    )
                (harness / f"run-{mode}.sh").write_text(
                    """#!/usr/bin/env bash
set -euo pipefail
mkdir "$2/active"
printf 'start %s\\n' "$1" >> "$2/events"
sleep 0.05
printf 'end %s\\n' "$1" >> "$2/events"
rmdir "$2/active"
[[ "$1" != b ]]
"""
                )
                command = ["bash", str(runner), mode, "hp39", str(app)]
                if mode == "stock":
                    command.append(str(root / "assets"))
                result = subprocess.run(
                    command,
                    env=dict(os.environ, HPCLANG_E2E_REQUIRE_KEYBOARD="0", HPCLANG_E2E_TARGET=""),
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertEqual(
                    (app / "events").read_text().splitlines(),
                    [f"{event} {target}" for target in "abc" for event in ("start", "end")],
                )
                self.assertIn("FAIL b (exit 1)", result.stderr)
                self.assertIn("PASS c", result.stdout)
                (app / "events").unlink()
                selected = subprocess.run(
                    command,
                    env=dict(
                        os.environ,
                        HPCLANG_E2E_REQUIRE_KEYBOARD="0",
                        HPCLANG_E2E_TARGET="c",
                    ),
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                self.assertEqual(selected.returncode, 0, selected.stdout + selected.stderr)
                self.assertEqual((app / "events").read_text().splitlines(), ["start c", "end c"])
                rejected = subprocess.run(
                    command,
                    env=dict(
                        os.environ,
                        HPCLANG_E2E_REQUIRE_KEYBOARD="0",
                        HPCLANG_E2E_TARGET="absent",
                    ),
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                self.assertEqual(rejected.returncode, 2, rejected.stdout + rejected.stderr)


if __name__ == "__main__":
    unittest.main()
