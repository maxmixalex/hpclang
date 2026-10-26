import importlib.util
import copy
import json
import struct
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("ui_verify", ROOT / "tests/e2e/ui/verify.py")
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class UiMemoryTests(unittest.TestCase):
    def test_pool_budget_is_bound_to_linked_image_and_arena(self):
        artifact = self.prefix.parent / "artifact"
        (artifact / "launchers").mkdir(parents=True)
        payload = struct.pack("<4sIII", b"HPC1", 4097, 0, 1656) + bytes(4097)
        (artifact / "e2e.hpc").write_bytes(payload)
        layout = dict(
            version=1,
            page_bytes=4096,
            pool_capacity_pages=256,
            application=dict(
                image_bytes=4097,
                code_pages=2,
                static_bytes=1656,
                boot_pages=1,
                stack_bytes=8192,
                required_rw_bytes=36864,
                rw_ceiling_bytes=36864,
            ),
            launcher=dict(
                stage_extent_bytes=4096,
                stage_and_prefix_pages=1,
                stage_stack_pages=1,
                mmu_workspace_pages=1,
            ),
            models=dict(
                hp50g=dict(
                    stock_native_workspace_pages=0,
                    minimum_stock_pool_pages=14,
                    stock_pool_pages_for_rw_ceiling=14,
                )
            ),
        )
        layout_path = artifact / "launchers/memory.json"
        layout_path.write_text(json.dumps(layout))
        self.check(self.report)
        for checkpoint in (1, 2):
            Path(f"{self.prefix}-pool-{checkpoint}.json").write_text(
                json.dumps(
                    dict(
                        version=1,
                        id=checkpoint,
                        model=6,
                        rw_bytes=36864,
                        stage_pool_pages=30,
                        native_workspace_pages=0,
                        native_os_block=0,
                        pool_l2_physical=0x08000000,
                        physical_pages=[0x08000000 + i * 4096 for i in range(30)],
                    )
                )
            )
        report = VERIFY.verify_budget(self.prefix, "hp50g", artifact)
        self.assertEqual(report["pool_required_bytes"], 14 * 4096)
        self.assertEqual(report["pool_excess_bytes"], 16 * 4096)
        self.assertEqual(report["arena_other_bytes"], 0)
        for section, key, value in (
            ("application", "image_bytes", 4096),
            ("application", "static_bytes", 0),
            ("application", "code_pages", 1),
            ("application", "rw_ceiling_bytes", 40960),
            ("launcher", "stage_and_prefix_pages", True),
            ("launcher", "stage_extent_bytes", 4097),
        ):
            bad = copy.deepcopy(layout)
            bad[section][key] = value
            layout_path.write_text(json.dumps(bad))
            with self.subTest(key=key), self.assertRaises(AssertionError):
                VERIFY.verify_budget(self.prefix, "hp50g", artifact)
            self.assertFalse(Path(f"{self.prefix}-budget.json").exists())
        layout_path.write_text(json.dumps(layout))
        (artifact / "e2e.hpc").write_bytes(payload[:-1])
        with self.assertRaises(AssertionError):
            VERIFY.verify_budget(self.prefix, "hp50g", artifact)
        (artifact / "e2e.hpc").write_bytes(payload)
        bad = dict(self.report, words=self.report["words"].copy())
        bad["words"][6] += 8
        self.path.write_text(json.dumps(bad))
        with self.assertRaises(AssertionError):
            VERIFY.verify_budget(self.prefix, "hp50g", artifact)

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
