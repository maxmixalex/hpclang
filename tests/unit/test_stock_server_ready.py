"""Exercise the shipped readiness loop without real-time sleeps or a calculator."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("bash"), "requires bash")
class StockServerReadyTests(unittest.TestCase):
    def invoke(self, ready_after, exits_after):
        script = (ROOT / "tests/e2e/harness/run-stock-hp4950-usb.sh").read_text()
        # Run the actual loop and its existing failure check, with deterministic
        # stand-ins for elapsed time and the emulator process's liveness.
        start = script.index("# Do not put protocol bytes")
        end = script.index("# Store extras first", start)
        gate = script[start:end]
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [
                    "bash",
                    "-c",
                    """
set -euo pipefail
SERVER_READY="$1/ready"
X50_PID=123
ticks=0
ready_after=$2
exits_after=$3
# An empty capture is not readiness.
: > "$SERVER_READY"
kill() { (( ticks < exits_after )); }
sleep() {
    ticks=$((ticks + 1))
    if (( ticks == ready_after )); then printf ready > "$SERVER_READY"; fi
}
"""
                    + gate
                    + '\nprintf "ticks=%s\\n" "$ticks"\n',
                    "readiness-test",
                    directory,
                    str(ready_after),
                    str(exits_after),
                ],
                capture_output=True,
                text=True,
            )
        return result

    def test_slow_live_emulator_reaches_readiness(self):
        # The preceding fixed 400-poll loop rejects this valid boot.
        result = self.invoke(405, 500)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ticks=405", result.stdout)

    def test_exited_emulator_without_capture_fails(self):
        for exits_after in (0, 3):
            with self.subTest(exits_after=exits_after):
                result = self.invoke(405, exits_after)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("server-ready capture was not produced", result.stderr)

    def test_ready_capture_stops_waiting(self):
        result = self.invoke(1, 500)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ticks=1", result.stdout)

    def test_outer_timeout_still_bounds_the_emulator(self):
        script = (ROOT / "tests/e2e/harness/run-stock-hp4950-usb.sh").read_text()
        self.assertRegex(script, r'timeout "\$\{EMULATOR_TIMEOUT\}s".*x50ng')
        self.assertLess(
            script.index('timeout "${EMULATOR_TIMEOUT}s"'),
            script.index("# Do not put protocol bytes"),
        )


if __name__ == "__main__":
    unittest.main()
