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


if __name__ == "__main__":
    unittest.main()
