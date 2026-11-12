"""Local harness execution must inherit the portable environment used by CI."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == "posix" and shutil.which("bash"), "requires POSIX shell")
class SetupRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="hpclang-runtime-env-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.script = self.root / "tests/e2e/harness/setup-runtime.sh"
        self.script.parent.mkdir(parents=True)
        self.script.write_text((ROOT / "tests/e2e/harness/setup-runtime.sh").read_text())
        self.dist = self.root / "build/x50ng/dist"
        (self.dist / "runtime-lib").mkdir(parents=True)
        (self.dist / ".hpclang-portable").touch()
        self.binary(self.dist / "x50ng", "exit 0")
        self.binary(self.dist / "tools/socat", "exit 0")
        self.binary(
            self.dist / "tools/kermit",
            'printf "%s\\n" "$0" "$LD_LIBRARY_PATH" "$@"\nexit 23',
        )
        self.host = self.root / "host-tools"
        self.binary(self.host / "kermit", 'echo "wrong system Kermit"\nexit 99')
        self.binary(self.host / "ldd", 'echo "libraries resolved"')
        self.env = dict(os.environ, PATH=f"{self.host}:/usr/bin:/bin", LD_LIBRARY_PATH="/prior/lib")
        for name in ("GITHUB_ENV", "GITHUB_PATH", "HPCLANG_X50NG_DIST"):
            self.env.pop(name, None)

    @staticmethod
    def binary(path, body):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/bin/sh\n" + body + "\n")
        path.chmod(0o755)

    def run_setup(self, *arguments):
        return subprocess.run(
            ["bash", str(self.script), *arguments],
            env=self.env,
            text=True,
            capture_output=True,
            check=False,
        )

    def test_exec_uses_bundle_and_preserves_arguments_and_status(self):
        literal = "argument with spaces; $(literal)"
        result = self.run_setup("stock", "hp39", "--", "kermit", literal)
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertEqual(
            result.stdout.splitlines()[1:],
            [str(self.dist / "tools/kermit"), f"{self.dist}/runtime-lib:/prior/lib", literal],
        )

    def test_existing_ci_export_contract(self):
        env_file, path_file = self.root / "github-env", self.root / "github-path"
        self.env.update(GITHUB_ENV=str(env_file), GITHUB_PATH=str(path_file))
        result = self.run_setup("stock", "hp4950")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            env_file.read_text(), f"LD_LIBRARY_PATH={self.dist}/runtime-lib:/prior/lib\n"
        )
        self.assertEqual(path_file.read_text(), f"{self.dist}/tools\n")

    def test_missing_bundle_tool_does_not_fall_back_to_system(self):
        (self.dist / "tools/kermit").unlink()
        result = self.run_setup("stock", "hp48", "--", "kermit")
        self.assertEqual(result.returncode, 1)
        self.assertIn("missing C-Kermit", result.stderr)
        self.assertNotIn("wrong system", result.stdout)

    def test_rejects_incomplete_or_ambiguous_command(self):
        for arguments in ((), ("stock",), ("stock", "hp39", "--"), ("stock", "hp39", "kermit")):
            with self.subTest(arguments=arguments):
                self.assertEqual(self.run_setup(*arguments).returncode, 2)
