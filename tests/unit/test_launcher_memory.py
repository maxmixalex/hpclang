import importlib.util
from pathlib import Path
import re
import struct
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("build_launchers", ROOT / "tools/build-launchers.py")
LAUNCHERS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LAUNCHERS)


class LauncherMemoryTests(unittest.TestCase):
    def test_memory_contract_matches_runtime_headers(self):
        abi = (ROOT / "include/hpclang/abi.h").read_text()
        bootstrap = (ROOT / "include/hpclang/bootstrap.h").read_text()
        for name, expected, header in (
            ("HPCLANG_PAGE_SIZE", LAUNCHERS.PAGE_BYTES, abi),
            ("HPCLANG_STACK_MIN", LAUNCHERS.STACK_BYTES, abi),
            ("HPCLANG_L2_ENTRIES", LAUNCHERS.MAX_RW_PAGES, bootstrap),
        ):
            value = re.search(r"^#define " + name + r" (\d+)u$", header, re.MULTILINE)
            self.assertIsNotNone(value, name)
            self.assertEqual(int(value[1]), expected, name)
        self.assertIn("#define HPCLANG_RW_PAGE_CAPACITY HPCLANG_L2_ENTRIES", bootstrap)

    @staticmethod
    def header(static):
        return struct.pack("<4sIII", b"HPC1", 8, 0, static)

    def test_static_data_rounding_and_reserved_pages(self):
        for static, minimum in ((0, 12288), (1, 16384), (4096, 16384), (4097, 20480)):
            with self.subTest(static=static):
                LAUNCHERS.validate_rw_limit(self.header(static), minimum)
                with self.assertRaises(ValueError):
                    LAUNCHERS.validate_rw_limit(self.header(static), minimum - 4096)
        LAUNCHERS.validate_rw_limit(self.header(1600), 36864)
        LAUNCHERS.validate_rw_limit(self.header(1600), 1048576)

    def test_invalid_limits_and_unbounded_default(self):
        header = self.header(1600)
        LAUNCHERS.validate_rw_limit(header, None)
        for limit in (True, 0, -4096, 36865, 1048576 + 4096, "36864", 36864.0):
            with self.subTest(limit=limit), self.assertRaises(ValueError):
                LAUNCHERS.validate_rw_limit(header, limit)
        with self.assertRaises(ValueError):
            LAUNCHERS.validate_rw_limit(self.header(0xFFFFFFFF), 1048576)

    def test_rejects_missing_or_wrong_header(self):
        for header in (b"", b"HPC1", self.header(0)[:15], b"HPCZ" + self.header(0)[4:]):
            with self.assertRaises(ValueError):
                LAUNCHERS.validate_rw_limit(header, None)


if __name__ == "__main__":
    unittest.main()
