import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("ui_verify", ROOT / "tests/e2e/ui/verify.py")
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class UiMemoryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.prefix = Path(temporary.name) / "model-frame"
        self.path = Path(f"{self.prefix}-memory-1.json")
        self.report = {
            "version": 1,
            "id": 1,
            "words": [1, 48, 2188, 2416, 8192, 18892, 22920, 36864],
        }

    def check(self, report):
        self.path.write_text(json.dumps(report))
        return VERIFY.verify_memory(self.prefix)

    def test_pool_observation_requires_distinct_stable_native_pages(self):
        with self.assertRaises(FileNotFoundError):
            VERIFY.verify_pool(self.prefix, 6)
        report = dict(
            version=1,
            model=6,
            rw_bytes=36864,
            stage_pool_pages=30,
            native_workspace_pages=0,
            native_os_block=0,
            pool_l2_physical=0x08000000,
            physical_pages=[0x08000000 + i * 4096 for i in range(30)],
        )

        def save(value, checkpoint):
            Path(f"{self.prefix}-pool-{checkpoint}.json").write_text(
                json.dumps(dict(value, id=checkpoint))
            )

        for checkpoint in (1, 2):
            save(report, checkpoint)
        self.assertEqual(VERIFY.verify_pool(self.prefix, 6), 30)
        for invalid in (
            dict(report, version=True),
            dict(report, model=5),
            dict(report, stage_pool_pages=31),
            dict(report, native_workspace_pages=2),
            dict(report, rw_bytes=32768),
            dict(report, pool_l2_physical=0x09000000),
            dict(report, physical_pages=[0x08000000] * 30),
            dict(report, physical_pages=[True] + report["physical_pages"][1:]),
            dict(report, native_os_block=42),
        ):
            save(invalid, 1)
            with self.subTest(report=invalid), self.assertRaises(AssertionError):
                VERIFY.verify_pool(self.prefix, 6)

    def test_final_cleanup_watermark_and_heap_are_required(self):
        self.assertEqual(self.check(self.report), (2416, 8192, 18892, 22920))
        for index, value in (
            (1, 0),
            (2, 44),
            (3, 0),
            (3, 2184),
            (3, 8192),
            (3, 2417),
            (4, 16384),
            (5, 0),
            (5, 22921),
            (6, 30000),
            (7, 53248),
        ):
            with self.subTest(index=index, value=value):
                invalid = dict(self.report, words=self.report["words"].copy())
                invalid["words"][index] = value
                with self.assertRaises(AssertionError):
                    self.check(invalid)

    def test_native_allocation_cross_checks_pool_without_double_counting(self):
        pool = dict(
            version=1,
            model=6,
            rw_bytes=36864,
            stage_pool_pages=32,
            native_workspace_pages=0,
            native_os_block=0x08001010,
            pool_l2_physical=0x08008000,
            physical_pages=[0x08002000, 0x08003000] + [0x08008000 + i * 4096 for i in range(30)],
        )
        allocation = dict(
            version=1,
            model=6,
            pointer=0x08001010,
            usable_bytes=0x2FF0,
            flags=0x2102,
            next=0,
            previous=0,
            metadata_bytes=20,
            mapped_pages=2,
            unmapped_usable_bytes=0xFF0,
        )

        def save(block, pages=pool):
            for checkpoint in (1, 2):
                for kind, value in (("allocation", block), ("pool", pages)):
                    Path(f"{self.prefix}-{kind}-{checkpoint}.json").write_text(
                        json.dumps(dict(value, id=checkpoint))
                    )

        save(allocation)
        self.assertEqual(VERIFY.verify_allocation(self.prefix, 6), 32 * 4096 + 0xFF0 + 20)
        for invalid in (
            dict(allocation, version=True),
            dict(allocation, model=1),
            dict(allocation, usable_bytes=0xFFFFFFFF),
            dict(allocation, flags=0x102),
            dict(allocation, mapped_pages=3),
            dict(allocation, unmapped_usable_bytes=0x2FF0),
            dict(allocation, metadata_bytes=0),
            dict(allocation, pointer=0),
            dict(allocation, next=0x08001000),
            dict(allocation, previous=0x08080000),
            dict(allocation, extra=1),
        ):
            save(invalid)
            with self.subTest(report=invalid), self.assertRaises(AssertionError):
                VERIFY.verify_allocation(self.prefix, 6)
        invalid_pool = dict(pool, physical_pages=[0x08004000] + pool["physical_pages"][1:])
        save(allocation, invalid_pool)  # a mapped page overlaps only the footer
        with self.assertRaises(AssertionError):
            VERIFY.verify_allocation(self.prefix, 6)
        save(allocation)
        path = Path(f"{self.prefix}-allocation-2.json")
        changed = dict(allocation, flags=0x2103, id=2)
        path.write_text(json.dumps(changed))
        with self.assertRaises(AssertionError):
            VERIFY.verify_allocation(self.prefix, 6)
        path.unlink()
        with self.assertRaises(FileNotFoundError):
            VERIFY.verify_allocation(self.prefix, 6)
        empty = dict.fromkeys(allocation, 0)
        empty.update(version=1, model=6)
        save(empty, dict(pool, native_os_block=0))
        self.assertEqual(VERIFY.verify_allocation(self.prefix, 6), 32 * 4096)

    def test_schema_and_missing_artifact_are_not_accepted(self):
        with self.assertRaises(FileNotFoundError):
            VERIFY.verify_memory(self.prefix)
        for invalid in (
            dict(self.report, version=2),
            dict(self.report, id=2),
            dict(self.report, version=True),
            dict(self.report, extra=0),
            dict(self.report, words=self.report["words"][:-1]),
            dict(self.report, words=[True] + self.report["words"][1:]),
            dict(self.report, words=[1.0] + self.report["words"][1:]),
        ):
            with self.subTest(report=invalid), self.assertRaises(AssertionError):
                self.check(invalid)

    def test_update_observations_require_complete_unsaturated_samples(self):
        path = Path(f"{self.prefix}-memory-2.json")
        with self.assertRaises(FileNotFoundError):
            VERIFY.verify_updates(self.prefix)
        report = {"version": 1, "id": 2, "words": [1, 5614, 1234, 9, 34]}
        path.write_text(json.dumps(report))
        self.assertEqual(VERIFY.verify_updates(self.prefix), (5614, 1234, 9, 34))
        for index, value in (
            (0, 2),
            (1, 0),
            (1, 0xFFFFFFFF),
            (2, 0),
            (2, 0xFFFFFFFF),
            (3, 0),
            (3, 8),
            (3, 10),
            (4, 0),
            (4, 1),
            (4, 32),
            (4, 33),
            (4, 36),
            (1, True),
            (1, 1.5),
            (1, -1),
            (1, 0x100000000),
        ):
            invalid = dict(report, words=report["words"].copy())
            invalid["words"][index] = value
            path.write_text(json.dumps(invalid))
            with self.subTest(index=index, value=value), self.assertRaises(AssertionError):
                VERIFY.verify_updates(self.prefix)
        for invalid in (
            dict(report, version=True),
            dict(report, version=2),
            dict(report, id=1),
            dict(report, extra=0),
            dict(report, words=report["words"][:-1]),
        ):
            path.write_text(json.dumps(invalid))
            with self.subTest(report=invalid), self.assertRaises(AssertionError):
                VERIFY.verify_updates(self.prefix)


if __name__ == "__main__":
    unittest.main()
