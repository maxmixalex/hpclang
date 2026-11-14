"""Detect source-incompatible changes that a function-name inventory misses."""

import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "fsystem_interface",
    Path(__file__).resolve().parents[1] / "compat/check-fsystem-interface.py",
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


@unittest.skipUnless(shutil.which("cc") and shutil.which("c++"), "C/C++ compilers required")
class SignatureTests(unittest.TestCase):
    def check_header(self, declaration):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "include").mkdir()
            (root / "include/fsystem.h").write_text(
                '#ifdef __cplusplus\nextern "C" {\n#endif\n'
                + declaration
                + "\n#ifdef __cplusplus\n}\n#endif\n",
                encoding="utf-8",
            )
            return MODULE.check_signatures(
                "extern int FSRead(char *buffer, int count);\nextern int FSShutdown();",
                {"FSRead", "FSShutdown"},
                root,
                "cc",
                "c++",
            )

    def test_original_parameter_names_need_not_match(self):
        result = self.check_header("int FSRead(char *data, int size); int FSShutdown(void);")
        self.assertEqual(result["languages"], ["C11", "C++17"])

    def test_changed_return_type(self):
        with self.assertRaisesRegex(ValueError, "declaration conflict"):
            self.check_header("void FSRead(char *data, int size); int FSShutdown(void);")

    def test_changed_pointer_type(self):
        with self.assertRaisesRegex(ValueError, "declaration conflict"):
            self.check_header("int FSRead(const char *data, int size); int FSShutdown(void);")

    def test_cxx_catches_changed_old_style_empty_parameter_list(self):
        with self.assertRaisesRegex(ValueError, r"c\+\+ original declaration conflict"):
            self.check_header("int FSRead(char *data, int size); int FSShutdown(int mode);")


if __name__ == "__main__":
    unittest.main()
