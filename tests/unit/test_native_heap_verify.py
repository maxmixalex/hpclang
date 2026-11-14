"""Acceptance must distinguish measured capacity, missing observations and failed runs."""

import copy
from pathlib import Path
import runpy
import unittest

VERIFY = runpy.run_path(
    str(Path(__file__).resolve().parents[1] / "e2e/harness/verify-native-heap-trace.py")
)["verify"]


class HeapTraceVerification(unittest.TestCase):
    def setUp(self):
        self.profile = dict(
            model=6, return_pc=0x1C914, free_list_roots=0x08002D14, begin=0x08000000, end=0x08080000
        )
        self.status = dict(status="pass", saw_pass=True, returned=True, return_settled=True)
        empty = dict(usable=0, largest=0, metadata=0, blocks=0)

        def heaps(value):
            return [dict(usable=value, largest=value, metadata=20, blocks=1), empty.copy()]

        sample = dict(
            phase="armtoolbox",
            start_us=100,
            end_us=200,
            accepted=3,
            rejected=0,
            failed_allocations=1,
            minimum_sample=2,
            minimum_total_usable=40,
            initial=heaps(100),
            minimum=heaps(40),
            latest=heaps(80),
        )
        unsampled = dict(
            phase="application-pass",
            start_us=200,
            end_us=300,
            accepted=0,
            rejected=0,
            failed_allocations=0,
            minimum_sample=0,
            minimum_total_usable=None,
            initial=[empty.copy(), empty.copy()],
            minimum=[empty.copy(), empty.copy()],
            latest=[empty.copy(), empty.copy()],
        )
        settled = copy.deepcopy(unsampled)
        settled.update(phase="settled-return", start_us=300, end_us=None)
        self.trace = dict(
            version=1,
            model=6,
            return_pc=self.profile["return_pc"],
            roots=self.profile["free_list_roots"],
            end=self.profile["end"],
            ignored_before_first_phase=5,
            phases=[sample, unsampled, settled],
        )

    def test_failed_allocations_are_observations_not_rejected_samples(self):
        result = VERIFY(self.trace, self.profile, self.status)
        self.assertEqual(result["accepted"], 3)
        self.assertEqual(result["failed_allocations"], 1)
        self.assertEqual(result["minima"], [dict(phase="armtoolbox", usable=40)])

    def test_zero_capacity_with_metadata_is_a_real_sample(self):
        phase = self.trace["phases"][0]
        phase["minimum"][0].update(usable=0, largest=0)
        phase["minimum_total_usable"] = 0
        self.assertEqual(VERIFY(self.trace, self.profile, self.status)["minima"][0]["usable"], 0)

    def test_reject_incomplete_or_wrong_run(self):
        for field, value in (
            ("status", "timeout"),
            ("saw_pass", False),
            ("returned", False),
            ("return_settled", False),
        ):
            with self.subTest(field=field), self.assertRaises(ValueError):
                VERIFY(self.trace, self.profile, dict(self.status, **{field: value}))
        with self.assertRaises(ValueError):
            VERIFY(self.trace, dict(self.profile, model=5), self.status)

    def test_reject_missing_or_invalid_measurements(self):
        for field, value in (
            ("rejected", 1),
            ("minimum_sample", 4),
            ("minimum_total_usable", 20),
            ("end_us", 201),
            ("failed_allocations", 4),
        ):
            trace = copy.deepcopy(self.trace)
            trace["phases"][0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                VERIFY(trace, self.profile, self.status)
        trace = copy.deepcopy(self.trace)
        trace["phases"] = trace["phases"][1:]
        with self.assertRaisesRegex(ValueError, "no qualified"):
            VERIFY(trace, self.profile, self.status)

    def test_unsampled_is_not_zero_capacity(self):
        self.trace["phases"][1]["minimum_total_usable"] = 0
        with self.assertRaisesRegex(ValueError, "empty phase"):
            VERIFY(self.trace, self.profile, self.status)


if __name__ == "__main__":
    unittest.main()
