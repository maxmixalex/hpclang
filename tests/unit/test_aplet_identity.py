import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("aplet_identity", ROOT / "tools/verify-hp39-aplet.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ApletIdentityTests(unittest.TestCase):
    def test_exact_application(self):
        body = b"wrapped launcher with embedded program"
        MODULE.verify(b"HPHP49-C" + body, b"HP39BinB\x04TEST-prefix-" + body + b"-suffix")

    def test_mixed_or_damaged_application(self):
        body = b"wrapped launcher with embedded program"
        for actual in (b"", body[:-1], body[:-1] + b"X", body + body):
            with self.subTest(actual=actual), self.assertRaises(ValueError):
                MODULE.verify(b"HPHP49-C" + body, b"HP39BinB" + actual)

    def test_headers_and_empty_launcher(self):
        for launcher, aplet in (
            (b"HPHP49-C", b"HP39BinB"),
            (b"wrong launcher", b"HP39BinBwrong launcher"),
            (b"HPHP49-Cprogram", b"wrong apletprogram"),
        ):
            with self.subTest(launcher=launcher), self.assertRaises(ValueError):
                MODULE.verify(launcher, aplet)


if __name__ == "__main__":
    unittest.main()
