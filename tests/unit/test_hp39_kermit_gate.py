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
            self.assertEqual(Path(f"{marker}.failed").exists(), expected != 0)

    def test_open_marker_survives_large_log_read(self) -> None:
        self.check_log(
            "openi name[LIB275L.000]\n"
            + ("packet diagnostics\n" * 2500)
            + "<sseof>Y success=1\nsseot justone=0\n",
            0,
        )

    def test_prior_file_eof_does_not_release_gate(self) -> None:
        self.check_log(
            "openi name[HPGCC000.000]\n<sseof>Y success=1\n"
            "openi name[LIB275L.000]\n" + ("packet diagnostics\n" * 2500),
            1,
        )

    def test_eof_without_transaction_ack_does_not_release(self) -> None:
        self.check_log("openi name[LIB275L.000]\n<sseof>Y success=1\n", 1)

    def test_directory_and_payload_are_distinct_transactions(self) -> None:
        parser = fetch.Completions()
        self.assertEqual(parser.feed("openi name[HP39DIR.000]\n<sseof>Y success=1\n"), [])
        self.assertEqual(parser.feed("sseot justone=0\n"), ["HP39DIR.000"])
        self.assertEqual(parser.feed("openi name[LIB275L.000]\nsseot justone=0\n"), [])

    def test_partial_lines_and_large_reads(self) -> None:
        parser = fetch.Completions()
        self.assertEqual(parser.feed("openi name[HP39"), [])
        self.assertEqual(parser.feed("DIR.000]\n" + "packet diagnostics\n" * 2500), [])
        self.assertEqual(parser.feed("<sseof>Y success=1\nsseot just"), [])
        self.assertEqual(parser.feed("one=0\n"), ["HP39DIR.000"])

    def test_directory_gate_is_published_before_payload(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "kermit.log"
            marker = root / "complete"
            log.write_text("openi name[HP39DIR.000]\n<sseof>Y success=1\nsseot justone=0\n")
            with (
                patch.object(fetch, "watchdog_seconds", return_value=0.01),
                patch.object(fetch, "POLL_SECONDS", 0.001),
            ):
                self.assertEqual(fetch.wait_for_fetch(log, root, marker), 1)
            self.assertTrue(Path(f"{marker}.directory").exists())
            self.assertFalse(marker.exists())
            self.assertIn("payload", Path(f"{marker}.failed").read_text())


if __name__ == "__main__":
    unittest.main()
