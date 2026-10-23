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

    def test_required_rw_bounds_and_pool_accounting(self):
        payload = self.header(1600) + bytes(8)
        for minimum, maximum in ((None, None), (36864, None), (36864, 36864), (16384, 36864)):
            LAUNCHERS.validate_rw_bounds(payload, minimum, maximum)
        for minimum, maximum in ((4096, None), (36865, None), (True, None), (40960, 36864)):
            with self.assertRaises(ValueError):
                LAUNCHERS.validate_rw_bounds(payload, minimum, maximum)
        report = LAUNCHERS.memory_report(payload, 30000, 9, ["hp39gs", "hp50g"], 36864, 36864)
        self.assertEqual(report["application"]["required_rw_bytes"], 36864)
        self.assertEqual(report["application"]["minimum_rw_pages"], 4)
        for item in report["models"].values():
            self.assertEqual(
                item["minimum_direct_pool_pages"], item["direct_pool_pages_for_rw_ceiling"]
            )
            self.assertEqual(
                item["minimum_stock_pool_pages"], item["stock_pool_pages_for_rw_ceiling"]
            )

    def test_rejects_missing_or_wrong_header(self):
        for header in (b"", b"HPC1", self.header(0)[:15], b"HPCZ" + self.header(0)[4:]):
            with self.assertRaises(ValueError):
                LAUNCHERS.validate_rw_limit(header, None)

    def test_pool_accounting_separates_stock_workspace_and_rw_ceiling(self):
        payload = struct.pack("<4sIII", b"HPC1", 4097, 0, 1600) + bytes(4097)
        report = LAUNCHERS.memory_report(payload, 30000, 9, list(LAUNCHERS.MODELS), 36864)
        self.assertEqual(report["application"]["code_pages"], 2)
        self.assertEqual(report["application"]["minimum_rw_pages"], 4)
        self.assertEqual(report["launcher"]["stage_and_prefix_pages"], 8)
        self.assertEqual(report["launcher"]["stage_stack_pages"], 1)
        self.assertEqual(report["launcher"]["mmu_workspace_pages"], 1)
        for model, item in report["models"].items():
            native = 2 if LAUNCHERS.MODELS[model] <= 3 else 0
            self.assertEqual(item["minimum_direct_pool_pages"], 16)
            self.assertEqual(item["minimum_stock_pool_pages"], 16 + native)
            self.assertEqual(item["direct_pool_pages_for_rw_ceiling"], 21)
            self.assertEqual(item["stock_pool_pages_for_rw_ceiling"], 21 + native)

    def test_unbounded_rw_reports_no_promised_reservation(self):
        payload = struct.pack("<4sIII", b"HPC1", 4096, 0, 4097) + bytes(4096)
        report = LAUNCHERS.memory_report(payload, 30000, 9, ["hp48gii"], None)
        self.assertEqual(report["application"]["minimum_rw_pages"], 5)
        self.assertIsNone(report["application"]["rw_ceiling_bytes"])
        self.assertIsNone(report["models"]["hp48gii"]["stock_pool_pages_for_rw_ceiling"])
        self.assertEqual(report["models"]["hp48gii"]["minimum_stock_pool_pages"], 16)
        self.assertTrue(report["excludes"])

    def test_report_rejects_incomplete_images_and_invalid_linked_extents(self):
        payload = struct.pack("<4sIII", b"HPC1", 8, 0, 0) + bytes(8)
        for image in (payload[:-1], payload + b"x", self.header(0)):
            with self.assertRaises(ValueError):
                LAUNCHERS.memory_report(image, 30000, 9, ["hp50g"], None)
        for size, pages in ((0, 9), (True, 9), (30000, 1), (30000, 257), (30000, True)):
            with self.assertRaises(ValueError):
                LAUNCHERS.memory_report(payload, size, pages, ["hp50g"], None)


if __name__ == "__main__":
    unittest.main()
