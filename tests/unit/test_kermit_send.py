import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == "posix" and shutil.which("bash"), "POSIX Kermit transport script")
class KermitSendTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.script = self.root / "send.sh"
        self.script.write_text((ROOT / "tools/run-hp48-kermit-send.sh").read_text())
        fake = self.root / "ckermit"
        fake.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$KERMIT_ARGUMENTS"\n')
        fake.chmod(0o755)
        self.capture = self.root / "arguments"

    def invoke(self, *arguments):
        return subprocess.run(
            [
                "bash",
                str(self.script),
                "/dev/fake",
                str(self.root),
                str(self.root / "debug"),
                *arguments,
            ],
            env=dict(
                os.environ,
                PATH=str(self.root) + os.pathsep + os.environ["PATH"],
                KERMIT_ARGUMENTS=str(self.capture),
            ),
            capture_output=True,
            text=True,
        )

    def commands(self):
        arguments = self.capture.read_text().splitlines()
        return arguments[arguments.index("-C") + 1].split(",")

    def test_remote_only_invokes_existing_program(self):
        result = self.invoke("--remote-host", "-95 CF HOME 'RECOVERYRETIRE' RCL")
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.commands()
        self.assertIn("remote host {-95 CF HOME 'RECOVERYRETIRE' RCL}", commands)
        self.assertFalse(any(command.startswith("send ") for command in commands))
        self.assertEqual(commands[-3:], ["finish", "if failure exit 1", "exit 0"])

    def test_upload_still_precedes_remote_command(self):
        (self.root / "PROGRAM.HP").write_bytes(b"fixture")
        result = self.invoke("--remote-host", "HOME", "PROGRAM.HP=RECOVERY")
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.commands()
        self.assertLess(
            commands.index("send /as-name:RECOVERY PROGRAM.HP"),
            commands.index("remote host {HOME}"),
        )

    def test_empty_or_incomplete_request_never_starts_kermit(self):
        for arguments in ((), ("--remote-host",), ("--remote-host", ""), ("--remote-host", "a,b")):
            with self.subTest(arguments=arguments):
                result = self.invoke(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.capture.exists())


if __name__ == "__main__":
    unittest.main()
