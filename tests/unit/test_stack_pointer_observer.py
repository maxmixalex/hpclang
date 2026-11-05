"""SP observations include untouched reservations and keep CPU modes separate."""

import json
import copy
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("ui_sp_verify", ROOT / "tests/e2e/ui/verify.py")
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class StackPointerBudgetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="stack-budget-test-")
        self.addCleanup(self.temp.cleanup)
        self.prefix = Path(self.temp.name) / "frame"
        self.trace = Path(f"{self.prefix}-stack-pointer.jsonl")
        self.output = Path(f"{self.prefix}-stack-pointer-budget.json")
        Path(f"{self.prefix}-memory-1.json").write_text(
            json.dumps(dict(version=1, id=1, words=[1, 48, 2424, 2424, 8192, 18892, 22880, 36864]))
        )
        self.row = dict(
            version=1,
            mode=31,
            writes=10000,
            minimum=0x1109000 - 2424,
            maximum=0x1109000,
            minimum_pc=0x1220100,
        )

    def write(self, rows):
        self.trace.write_text("".join(json.dumps(row) + "\n" for row in rows))

    def test_reservation_can_exceed_watermark(self):
        self.row["minimum"] = 0x1109000 - 4144
        self.write([self.row])
        result = VERIFY.verify_stack_pointer(self.prefix)
        self.assertEqual(result["stack_pointer_bytes"], 4144)
        self.assertEqual(result["stack_touched_bytes"], 2424)
        self.assertEqual(json.loads(self.output.read_text()), result)

    def test_native_mode_does_not_replace_application_budget(self):
        native = dict(self.row, mode=19, minimum=0x8007000, maximum=0x8008000)
        self.write([native, self.row])
        self.assertEqual(VERIFY.verify_stack_pointer(self.prefix)["stack_pointer_bytes"], 2424)
        self.write([native])
        with self.assertRaises(AssertionError):
            VERIFY.verify_stack_pointer(self.prefix)
        self.assertFalse(self.output.exists())

    def test_user_mode_and_shared_bank_transitions(self):
        user = dict(self.row, mode=16)
        self.write([user])
        self.assertEqual(VERIFY.verify_stack_pointer(self.prefix)["stack_pointer_bytes"], 2424)
        system = dict(self.row, minimum=0x1109000 - 4144, maximum=0x1109000 - 48)
        self.write([user, system])
        self.assertEqual(VERIFY.verify_stack_pointer(self.prefix)["stack_pointer_bytes"], 4144)
        for key, value in (("minimum", 0x1109000 - 8192), ("maximum", 0x1109004)):
            self.write([user, dict(system, **{key: value})])
            with self.assertRaises(AssertionError):
                VERIFY.verify_stack_pointer(self.prefix)
            self.assertFalse(self.output.exists())

    def test_invalid_bounds_and_schema_remove_stale_success(self):
        for key, value in (
            ("minimum", 0x1109000 - 8192),
            ("minimum", 0x1109000 - 2000),
            ("maximum", 0x1109004),
            ("minimum", 1),
            ("minimum_pc", 0x1000100),
            ("minimum_pc", 0x1220101),
            ("writes", 0),
            ("writes", True),
            ("version", 2),
            ("mode", 0),
            ("minimum", -4),
        ):
            with self.subTest(key=key, value=value):
                row = copy.deepcopy(self.row)
                row[key] = value
                self.write([row])
                self.output.write_text("stale success")
                with self.assertRaises(AssertionError):
                    VERIFY.verify_stack_pointer(self.prefix)
                self.assertFalse(self.output.exists())

    def test_missing_duplicate_and_truncated_trace_rejected(self):
        for rows in ([], [self.row, self.row]):
            self.write(rows)
            with self.assertRaises(AssertionError):
                VERIFY.verify_stack_pointer(self.prefix)
        self.trace.write_text('{"version":')
        with self.assertRaises(json.JSONDecodeError):
            VERIFY.verify_stack_pointer(self.prefix)
        self.trace.unlink()
        with self.assertRaises(FileNotFoundError):
            VERIFY.verify_stack_pointer(self.prefix)


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class StackPointerObserverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="stack-pointer-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        source = cls.root / "probe.c"
        source.write_text(
            '#include "stack-pointer-observer.h"\n'
            "int main(void) {\n"
            " stack_pointer_observe(0x1109000, 0x1200100, 31);\n"
            " /* No guest memory is written: reserve and immediately release. */\n"
            " stack_pointer_observe(0x1108000, 0x1200104, 31);\n"
            " stack_pointer_observe(0x1109000, 0x1200108, 31);\n"
            " stack_pointer_observe(0x8000100, 0x1200110, 18);\n"
            " stack_pointer_observe(0, 0x1000100, 31);\n"
            " return 0;\n}\n"
        )
        cls.binary = cls.root / "probe"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "tests/e2e/harness/x50ng"),
                str(source),
                "-o",
                str(cls.binary),
            ],
            check=True,
        )

    def setUp(self):
        self.case = tempfile.TemporaryDirectory(dir=self.root)
        self.addCleanup(self.case.cleanup)
        self.path = Path(self.case.name) / "trace.jsonl"

    def invoke(self, enabled=True):
        env = dict(os.environ)
        env.pop("HPCLANG_X50NG_STACK_POINTER", None)
        if enabled:
            env["HPCLANG_X50NG_STACK_POINTER"] = str(self.path)
        return subprocess.run([str(self.binary)], env=env, capture_output=True, text=True)

    def test_untouched_reservation_and_modes(self):
        self.assertEqual(self.invoke().returncode, 0)
        rows = {row["mode"]: row for row in map(json.loads, self.path.read_text().splitlines())}
        self.assertEqual(set(rows), {18, 31})
        self.assertEqual(
            rows[31],
            dict(
                version=1,
                mode=31,
                writes=3,
                minimum=0x1108000,
                maximum=0x1109000,
                minimum_pc=0x1200104,
            ),
        )
        self.assertEqual(rows[18]["minimum"], 0x8000100)

    def test_disabled_creates_nothing(self):
        self.assertEqual(self.invoke(enabled=False).returncode, 0)
        self.assertFalse(self.path.exists())

    def test_existing_evidence_is_preserved(self):
        self.path.write_text("previous evidence")
        self.assertNotEqual(self.invoke().returncode, 0)
        self.assertEqual(self.path.read_text(), "previous evidence")

    def test_unwritable_destination_fails(self):
        self.path = self.path / "absent" / "trace.jsonl"
        self.assertNotEqual(self.invoke().returncode, 0)


if __name__ == "__main__":
    unittest.main()
