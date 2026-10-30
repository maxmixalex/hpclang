"""Mode switches expose inactive stacks without conflating their address ranges."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class StackBankObserverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="stack-banks-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        source = cls.root / "probe.c"
        source.write_text(
            '#include "stack-bank-observer.h"\n'
            "int main(void) {\n"
            " stack_bank_observe(16, 18, 0x1109000, 0x8001000);\n"
            " stack_bank_observe(18, 16, 0x8000f00, 0x1109000);\n"
            " stack_bank_observe(16, 18, 0x1108000, 0x8001000);\n"
            " stack_bank_observe(16, 31, 0x1108000, 0x1108000);\n"
            " stack_bank_observe(19, 17, 0, 0xffffffff);\n"
            " return 0; }\n"
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
        self.trace = Path(self.case.name) / "banks.jsonl"

    def invoke(self, enabled=True):
        env = dict(os.environ)
        env.pop("HPCLANG_X50NG_STACK_BANKS", None)
        if enabled:
            env["HPCLANG_X50NG_STACK_BANKS"] = str(self.trace)
        return subprocess.run([str(self.binary)], env=env, capture_output=True, text=True)

    def test_separate_banks_and_directions(self):
        self.assertEqual(self.invoke().returncode, 0)
        rows = [json.loads(line) for line in self.trace.read_text().splitlines()]
        indexed = {(r["from_mode"], r["to_mode"]): r for r in rows}
        self.assertEqual(len(rows), 4)
        irq = indexed[16, 18]
        self.assertEqual(irq["switches"], 2)
        self.assertEqual((irq["outgoing_min"], irq["outgoing_max"]), (0x1108000, 0x1109000))
        self.assertEqual((irq["incoming_min"], irq["incoming_max"]), (0x8001000, 0x8001000))
        self.assertEqual(indexed[18, 16]["outgoing_min"], 0x8000F00)
        shared = indexed[16, 31]
        self.assertEqual(shared["outgoing_min"], shared["incoming_min"])
        self.assertEqual(indexed[19, 17]["outgoing_min"], 0)
        self.assertEqual(indexed[19, 17]["incoming_max"], 0xFFFFFFFF)

    def test_disabled(self):
        self.assertEqual(self.invoke(enabled=False).returncode, 0)
        self.assertFalse(self.trace.exists())

    def test_preserves_existing_evidence(self):
        self.trace.write_text("retained")
        self.assertNotEqual(self.invoke().returncode, 0)
        self.assertEqual(self.trace.read_text(), "retained")

    def test_missing_parent(self):
        self.trace = self.trace / "absent" / "trace"
        self.assertNotEqual(self.invoke().returncode, 0)


if __name__ == "__main__":
    unittest.main()
