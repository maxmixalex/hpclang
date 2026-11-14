"""Reject incomplete or exhausted Notebook ARM resource observations."""

import json
from pathlib import Path
import runpy
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
VERIFY = runpy.run_path(str(ROOT / "tests/e2e/notebook-entry/verify.py"))["verify_resources"]


class NotebookResourcesTests(unittest.TestCase):
    def test_resource_bounds_and_required_observation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "hp39gplus-frame-memory-1.json"
            with self.assertRaises(FileNotFoundError):
                VERIFY(root, "hp39gplus")
            words = [1, 32, 2048, 2112, 8192, 12000, 25000, 36864]
            path.write_text(json.dumps(dict(version=1, id=1, words=words)))
            self.assertEqual(VERIFY(root, "hp39gplus"), words)
            for index, value in [
                (0, 2),
                (1, 0),
                (2, 2200),
                (3, 8192),
                (4, 4096),
                (5, 25001),
                (5, 0),
                (6, 30000),
                (7, 36865),
                (1, True),
            ]:
                with self.subTest(index=index, value=value):
                    bad = words.copy()
                    bad[index] = value
                    path.write_text(json.dumps(dict(version=1, id=1, words=bad)))
                    with self.assertRaises(AssertionError):
                        VERIFY(root, "hp39gplus")


if __name__ == "__main__":
    unittest.main()
