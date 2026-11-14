"""Exercise emitted LDM user-bank SP instrumentation independently of the guest."""

import argparse
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class UserStackObserverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="user-stack-observer-")
        cls.addClassCleanup(cls.temp.cleanup)
        root = Path(cls.temp.name)
        function = runpy.run_path(str(ROOT / "tools/prepare-x50ng.py"))[
            "patch_stack_pointer_observer"
        ]
        emitted = {}

        def capture(path, old, new, label):
            emitted[label] = new

        with patch.dict(function.__globals__, replace_once=capture), patch("shutil.copy2"):
            function(root, argparse.Namespace(headless_header=root / "frontend.h"))
        # Execute the actual generated translator fragment with TCG stand-ins.
        # The source-register assignment must remain intact; only an enabled
        # register-13 load emits the separate observation with the ARM PC.
        source = root / "probe.c"
        source.write_text(
            "#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n"
            "typedef uint32_t TCGv;\n"
            "static unsigned reg, value, calls, observed, address;\n"
            "static TCGv tcg_const_i32(uint32_t v) { return v; }\n"
            "static void tcg_temp_free_i32(TCGv v) { (void)v; }\n"
            "static void gen_helper_set_user_reg(TCGv r, TCGv v) { reg=r; value=v; }\n"
            "static void gen_helper_hpclang_user_stack(TCGv v, TCGv pc)\n"
            "{ ++calls; observed=v; address=pc; }\n"
            "int main(int argc, char **argv) {\n"
            " if (argc != 2) return 2;\n"
            " unsigned i=(unsigned)strtoul(argv[1], 0, 0);\n"
            " struct { uint32_t pc; } context={0x3f3e4}, *s=&context;\n"
            " TCGv tmp2=i, tmp=0x08001234;\n"
            + emitted["ARM LDM user-bank SP assignments"]
            + ' printf("%u %u %u %u %u\\n",reg,value,calls,observed,address);\n'
            " return 0; }\n",
            encoding="utf-8",
        )
        cls.binary = root / "probe"
        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(cls.binary)],
            check=True,
        )
        helper = emitted["stack pointer helper implementation"].split(
            "void HELPER(hpclang_user_stack)", 1
        )[1]
        source.write_text(
            '#include "stack-region-observer.h"\n'
            "void observe_user" + helper + "\n"
            "int main(void) { observe_user(0x08001234, 0x3f3e0); return 0; }\n",
            encoding="utf-8",
        )
        cls.helper = root / "helper"
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
                str(cls.helper),
            ],
            check=True,
        )

    def test_saved_sp_is_attributed_to_user_bank(self):
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.jsonl"
            subprocess.run(
                [str(self.helper)],
                env=dict(os.environ, HPCLANG_X50NG_STACK_REGIONS=str(trace)),
                check=True,
            )
            row = json.loads(trace.read_text())
            self.assertEqual((row["mode"], row["region"], row["writes"]), (16, 128, 1))
            self.assertEqual((row["minimum"], row["minimum_pc"]), (0x08001234, 0x3F3E0))

    def observe(self, register, setting):
        env = dict(os.environ)
        env.pop("HPCLANG_X50NG_STACK_REGIONS", None)
        if setting is not None:
            env["HPCLANG_X50NG_STACK_REGIONS"] = setting
        return tuple(
            map(int, subprocess.check_output([str(self.binary), str(register)], env=env).split())
        )

    def test_user_sp_value_and_instruction_address(self):
        self.assertEqual(self.observe(13, "trace"), (13, 0x08001234, 1, 0x08001234, 0x3F3E0))

    def test_other_registers_are_not_stack_writes(self):
        for register in (0, 8, 12, 14):
            self.assertEqual(self.observe(register, "trace"), (register, 0x08001234, 0, 0, 0))

    def test_disabled_and_empty_trace_emit_no_observer(self):
        for setting in (None, ""):
            self.assertEqual(self.observe(13, setting), (13, 0x08001234, 0, 0, 0))


if __name__ == "__main__":
    unittest.main()
