"""Native-PC SP writes and disjoint stacks remain observable without guest writes."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class StackRegionObserverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="stack-regions-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        source = cls.root / "probe.c"
        source.write_text(
            '#include "stack-pointer-observer.h"\n'
            '#include "stack-region-observer.h"\n'
            "static void observe(uint32_t sp, uint32_t pc, uint32_t mode) {\n"
            " stack_pointer_observe(sp, pc, mode); stack_region_observe(sp, pc, mode); }\n"
            "int main(void) {\n"
            " observe(0x1109000, 0x1220100, 31);\n"
            " observe(0x1108000, 0x7700, 31); /* ROM uses application stack */\n"
            " observe(0x1301000, 0x1300100, 31); /* separate stage stack */\n"
            " observe(0x1300000, 0x7704, 31);\n"
            " observe(0x12ffffc, 0x7708, 31); /* crosses a region boundary */\n"
            " observe(0x8001000, 0x18, 18); /* IRQ stack remains separate */\n"
            " observe(0, 0x10, 19); /* retain invalid-looking SP evidence */\n"
            " observe(0xffffffff, 0x14, 17);\n"
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
        self.regions = Path(self.case.name) / "regions.jsonl"
        self.application = Path(self.case.name) / "application.jsonl"

    def invoke(self, regions=True, application=True):
        env = dict(os.environ)
        for name, enabled, path in (
            ("HPCLANG_X50NG_STACK_REGIONS", regions, self.regions),
            ("HPCLANG_X50NG_STACK_POINTER", application, self.application),
        ):
            env.pop(name, None)
            if enabled:
                env[name] = str(path)
        return subprocess.run([str(self.binary)], env=env, capture_output=True, text=True)

    def test_native_calls_modes_and_boundaries(self):
        self.assertEqual(self.invoke().returncode, 0)
        rows = [json.loads(line) for line in self.regions.read_text().splitlines()]
        indexed = {(row["mode"], row["region"]): row for row in rows}
        self.assertEqual(
            set(indexed), {(31, 17), (31, 18), (31, 19), (18, 128), (19, 0), (17, 4095)}
        )
        self.assertEqual(len(indexed), len(rows))
        app = indexed[31, 17]
        self.assertEqual(
            (app["minimum"], app["maximum"], app["minimum_pc"], app["writes"]),
            (0x1108000, 0x1109000, 0x7700, 2),
        )
        self.assertEqual(indexed[31, 18]["minimum"], 0x12FFFFC)
        self.assertEqual(indexed[19, 0]["minimum"], 0)
        self.assertEqual(indexed[17, 4095]["maximum"], 0xFFFFFFFF)
        old = [json.loads(line) for line in self.application.read_text().splitlines()]
        self.assertEqual(len(old), 1)
        self.assertEqual((old[0]["writes"], old[0]["minimum"]), (1, 0x1109000))

    def test_independent_opt_in(self):
        self.assertEqual(self.invoke(regions=False).returncode, 0)
        self.assertFalse(self.regions.exists())
        self.assertTrue(self.application.exists())
        self.application.unlink()
        self.assertEqual(self.invoke(application=False).returncode, 0)
        self.assertTrue(self.regions.exists())
        self.assertFalse(self.application.exists())

    def test_existing_evidence_is_not_overwritten(self):
        self.regions.write_text("retained")
        self.assertNotEqual(self.invoke().returncode, 0)
        self.assertEqual(self.regions.read_text(), "retained")

    def test_missing_parent_fails(self):
        self.regions = self.regions / "absent" / "trace"
        self.assertNotEqual(self.invoke().returncode, 0)


if __name__ == "__main__":
    unittest.main()
