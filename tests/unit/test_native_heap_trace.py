"""Exercise the actual host trace writer without a running emulator."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class NativeHeapTraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="native-heap-trace-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        source = cls.root / "probe.c"
        source.write_text(
            r"""
#include <assert.h>
#include "native-heap-trace.h"
static uint32_t ram[256];
static unsigned reads;
static int read32(void *ctx, uint32_t address, uint32_t *value) {
    (void)ctx; ++reads;
    assert(address >= 0x08000000 && address < 0x08000400 && !(address & 3));
    *value = ram[(address - 0x08000000)/4]; return 0;
}
static void capacity(unsigned size) {
    memset(ram, 0, sizeof ram);
    ram[0] = 0x08000040; ram[16] = size; ram[17] = 0x100;
    ram[(64+16+size)/4] = size;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    capacity(200);
    native_heap_trace_observe(read32, NULL, 256, 0x08000100);
    assert(reads == 0);
    native_heap_trace_mark("receive", 100);
    native_heap_trace_observe(read32, NULL, 256, 0x08000100);
    capacity(40);
    native_heap_trace_observe(read32, NULL, 256, 0);
    if (!strcmp(argv[1], "corrupt")) {
        ram[16] = 0xfffffffc;
        native_heap_trace_observe(read32, NULL, 256, 0x08000100);
    }
    capacity(200);
    native_heap_trace_observe(read32, NULL, 256, 0x08000100);
    native_heap_trace_mark(!strcmp(argv[1], "bad-phase") ? "bad\"phase" : "application-pass", 200);
    native_heap_trace_mark("settled-return", !strcmp(argv[1], "bad-time") ? 50 : 300);
    printf("%u\n", reads);
    return 0;
}
""",
            encoding="utf-8",
        )
        cls.binary = cls.root / "probe"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer",
                "-I",
                str(ROOT / "tests/e2e/harness/x50ng"),
                str(source),
                "-o",
                str(cls.binary),
            ],
            check=True,
            capture_output=True,
        )

    def invoke(self, mode="normal", enabled=True, settings=None):
        with tempfile.TemporaryDirectory(dir=self.root) as temporary:
            path = Path(temporary) / "trace.json"
            env = {
                key: value
                for key, value in os.environ.items()
                if not key.startswith("HPCLANG_X50NG_NATIVE_HEAP")
            }
            if enabled:
                env.update(
                    HPCLANG_X50NG_NATIVE_HEAP=str(path),
                    HPCLANG_X50NG_NATIVE_HEAP_MODEL="6",
                    HPCLANG_X50NG_NATIVE_HEAP_PC="256",
                    HPCLANG_X50NG_NATIVE_HEAP_ROOTS="0x08000000",
                )
            env.update(settings or {})
            result = subprocess.run(
                [str(self.binary), mode], env=env, text=True, capture_output=True
            )
            data = json.loads(path.read_text()) if path.exists() else None
            return result, data

    def test_disabled_performs_no_reads_or_writes(self):
        result, data = self.invoke(enabled=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "0")
        self.assertIsNone(data)

    def test_phase_summary_retains_transient_minimum(self):
        result, data = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(data["ignored_before_first_phase"], 1)
        receive, passed, returned = data["phases"]
        self.assertEqual((receive["start_us"], receive["end_us"]), (100, 200))
        self.assertEqual(receive["accepted"], 3)
        self.assertEqual(receive["rejected"], 0)
        self.assertEqual(receive["failed_allocations"], 1)
        self.assertEqual(receive["minimum_total_usable"], 40)
        self.assertEqual(receive["minimum_sample"], 2)
        self.assertEqual(receive["initial"], receive["latest"])
        self.assertIsNone(passed["minimum_total_usable"])
        self.assertIsNone(returned["end_us"])

    def test_rejected_event_is_visible(self):
        result, data = self.invoke("corrupt")
        self.assertEqual(result.returncode, 0, result.stderr)
        phase = data["phases"][0]
        self.assertEqual((phase["accepted"], phase["rejected"]), (3, 1))
        self.assertEqual(phase["minimum_total_usable"], 40)

    def test_bad_phase_and_time_fail(self):
        for mode in ("bad-phase", "bad-time"):
            with self.subTest(mode=mode):
                result, _ = self.invoke(mode)
                self.assertNotEqual(result.returncode, 0)

    def test_invalid_profile_settings_fail(self):
        for key, value in (
            ("MODEL", "0"),
            ("MODEL", "7"),
            ("PC", "257"),
            ("PC", "4294967296"),
            ("ROOTS", "0"),
            ("ROOTS", "-1"),
        ):
            with self.subTest(key=key, value=value):
                result, data = self.invoke(settings={"HPCLANG_X50NG_NATIVE_HEAP_" + key: value})
                self.assertNotEqual(result.returncode, 0)
                self.assertIsNone(data)


if __name__ == "__main__":
    unittest.main()
