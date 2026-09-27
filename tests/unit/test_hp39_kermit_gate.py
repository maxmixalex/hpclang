"""Regression tests for the HP39 Stock Kermit completion gate."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SOURCE = Path(__file__).resolve().parents[2] / "tools" / "wait-hp39-kermit-fetch.py"
SPEC = importlib.util.spec_from_file_location("hp39_kermit_fetch", SOURCE)
assert SPEC is not None and SPEC.loader is not None
fetch = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fetch)


class KermitFetchGateTests(unittest.TestCase):
    def check_log(self, contents: str, expected: int) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "kermit.log"
            marker = root / "complete"
            log.write_text(contents, encoding="latin-1")
            with (
                patch.object(fetch, "watchdog_seconds", return_value=0.03),
                patch.object(fetch, "POLL_SECONDS", 0.001),
            ):
                self.assertEqual(fetch.wait_for_fetch(log, root, marker), expected)
            self.assertEqual(marker.exists(), expected == 0)

    def test_open_marker_survives_large_log_read(self) -> None:
        self.check_log(
            "openi name[LIB275L.000]\n"
            + ("packet diagnostics\n" * 2500)
            + "<sseof>Y success=1\n",
            0,
        )

    def test_prior_file_eof_does_not_release_gate(self) -> None:
        self.check_log(
            "openi name[HPGCC000.000]\n<sseof>Y success=1\n"
            "openi name[LIB275L.000]\n"
            + ("packet diagnostics\n" * 2500),
            1,
        )


if __name__ == "__main__":
    unittest.main()
