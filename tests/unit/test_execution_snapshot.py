"""Instruction snapshots preserve guest state and distinguish SRAM/model geometry."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class ExecutionSnapshotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="execution-snapshot-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        source = cls.root / "probe.c"
        source.write_text(
            "#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n#include <assert.h>\n"
            "typedef struct { uint32_t pc, cpsr; } CPUState;\n"
            "static unsigned char ram[0x80000];\n"
            "static uint32_t cpsr_read(CPUState *env) { return env->cpsr; }\n"
            "static int cpu_memory_rw_debug(void *env, uint32_t at, void *out, size_t n, int write) {\n"
            " assert(env && !write && at == 0x08000000 && n <= sizeof ram);\n"
            ' if (getenv("FAIL_READ")) return -1;\n'
            " memcpy(out, ram, n); return 0; }\n"
            '#include "execution-snapshot.h"\n'
            "int main(void) {\n"
            " CPUState env = {0x01220100, 0x80000010};\n"
            " for (unsigned i = 0; i < sizeof ram; ++i) ram[i] = (unsigned char)i;\n"
            " execution_snapshot(&env, 0x01220200);\n"
            " assert(env.pc == 0x01220100 && env.cpsr == 0x80000010);\n"
            " for (unsigned i = 0; i < sizeof ram; ++i) assert(ram[i] == (unsigned char)i);\n"
            " execution_snapshot(&env, 0x01220300); /* one capture only */\n"
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
        self.temp = tempfile.TemporaryDirectory(dir=self.root)
        self.addCleanup(self.temp.cleanup)
        self.prefix = Path(self.temp.name) / "snapshot"

    def invoke(self, model="hp50g", **extra):
        env = dict(os.environ, TARGET=model, HPCLANG_X50NG_EXECUTION_SNAPSHOT=str(self.prefix))
        env.pop("FAIL_READ", None)
        env.update(extra)
        return subprocess.run([str(self.binary)], env=env, capture_output=True, text=True)

    def test_exact_snapshot_once_without_guest_changes(self):
        self.assertEqual(self.invoke().returncode, 0)
        report = json.loads(Path(f"{self.prefix}-entry.json").read_text())
        self.assertEqual((report["model"], report["pc"], report["time_us"]), (6, 0x01220200, 0))
        self.assertEqual(Path(f"{self.prefix}-entry.sram").read_bytes(), bytes(range(256)) * 2048)

    def test_hp48_geometry(self):
        self.assertEqual(self.invoke("hp48gii").returncode, 0)
        self.assertEqual(Path(f"{self.prefix}-entry.sram").stat().st_size, 0x40000)

    def test_existing_evidence_preserved(self):
        raw = Path(f"{self.prefix}-entry.sram")
        raw.write_bytes(b"retained")
        self.assertNotEqual(self.invoke().returncode, 0)
        self.assertEqual(raw.read_bytes(), b"retained")

    def test_invalid_model_and_failed_read_publish_nothing(self):
        for model, extra in (("unknown", {}), ("hp50g", {"FAIL_READ": "1"})):
            self.assertNotEqual(self.invoke(model, **extra).returncode, 0)
            self.assertFalse(Path(f"{self.prefix}-entry.sram").exists())
            self.assertFalse(Path(f"{self.prefix}-entry.json").exists())


if __name__ == "__main__":
    unittest.main()
