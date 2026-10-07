"""Transfer progress must survive slow hosts without accepting retry loops."""

from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[2] / "tools" / "kermit_watchdog.py"
SPEC = importlib.util.spec_from_file_location("watchdog_under_test", SOURCE)
assert SPEC is not None and SPEC.loader is not None
watchdog = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(watchdog)


def ack(window: int) -> str:
    return f"input send rotated send window={window}\n"


class TransferWatchdogTests(unittest.TestCase):
    def test_chunk_boundaries_duplicates_and_wrap(self) -> None:
        progress = watchdog.Progress()
        self.assertFalse(progress.feed("input send rotated send win"))
        self.assertTrue(progress.feed("dow=63\r\n"))
        self.assertFalse(progress.feed(ack(63)))
        self.assertTrue(progress.feed(ack(0)))
        self.assertFalse(progress.feed("input send rotated send window=64\n"))

    def test_retry_traffic_does_not_extend_idle_budget(self) -> None:
        with patch.object(watchdog.time, "monotonic", return_value=0) as clock:
            timer = watchdog.Watchdog()
            timer.observe(ack(1))
            clock.return_value = watchdog.IDLE_SECONDS - 1
            timer.observe("rpack block check OK=0\ninput send ACK for previous=0\n" + ack(1))
            self.assertIsNone(timer.failure())
            clock.return_value = watchdog.IDLE_SECONDS
            self.assertIn("no acknowledged", timer.failure())

    def test_progress_extends_idle_but_not_absolute_ceiling(self) -> None:
        with patch.object(watchdog.time, "monotonic", return_value=0) as clock:
            timer = watchdog.Watchdog()
            for now in range(60, watchdog.TOTAL_SECONDS, 60):
                clock.return_value = now
                timer.observe(ack((now // 60) % 64))
                self.assertIsNone(timer.failure())
            clock.return_value = watchdog.TOTAL_SECONDS
            timer.observe(ack(63))
            self.assertIn("absolute ceiling", timer.failure())

    def test_process_exit_status_and_stale_log(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "kermit.log"
            log.write_text(ack(1))
            self.assertEqual(watchdog.run(log, [sys.executable, "-c", "raise SystemExit(7)"]), 7)
            self.assertFalse(log.exists())

    def test_stalled_process_is_terminated(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with (
                patch.object(watchdog, "IDLE_SECONDS", 0.05),
                patch.object(watchdog, "POLL_SECONDS", 0.01),
            ):
                self.assertEqual(
                    watchdog.run(
                        Path(directory) / "kermit.log",
                        [sys.executable, "-c", "import time; time.sleep(30)"],
                    ),
                    124,
                )


if __name__ == "__main__":
    unittest.main()
