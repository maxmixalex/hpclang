import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("package_aplet", ROOT / "tools/package-aplet.py")
APLET = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(APLET)


class ApletPackageTests(unittest.TestCase):
    def test_builtin_writer_matches_historical_binary_layout(self):
        # hp2aplet output using single-byte template, launcher, graphic and epilog.
        data = {"apletcode.bin": b"\xaa", "apletepilog.bin": b"\xbb", "blank.gro": b"HPHP49-C\xcc"}
        expected = bytes.fromhex(
            "4850333942696e420158962a3011500000000000aa40a702dd2c2a700000582c"
            "2a70000059cc11290000001129000000742ab012032b3160040000bb"
        )
        self.assertEqual(APLET.encode_aplet(b"\xdd", data, b"X", b"Y"), expected)

    def test_invalid_project_fields_never_invoke_historical_writer(self):
        with patch.object(APLET.subprocess, "run") as run:
            for name, author in (
                ("bad\nN", "author"),
                ("", "author"),
                ("界面", "author"),
                ("n" * 49, "author"),
                ("Aplet", "a" * 34),
            ):
                with self.subTest(name=name, author=author), self.assertRaises(ValueError):
                    APLET.package(
                        Path("absent"), Path("absent"), Path("absent"), Path("absent"), name, author
                    )
            run.assert_not_called()

    def test_oversized_input_and_tool_failure_preserve_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for asset in APLET.ASSETS:
                (root / asset).write_bytes(b"HP39AscD test")
            payload = root / "input.hp"
            output = root / "output"
            output.mkdir()
            retained = output / "HPGCC000.000"
            retained.write_bytes(b"old complete application")
            with patch.object(APLET.subprocess, "run") as run:
                payload.write_bytes(b"HPHP49-C" + bytes(300000))
                with self.assertRaisesRegex(ValueError, "capacity"):
                    APLET.package(payload, output, root / "tool", root, "Example", "Author")
                run.assert_not_called()
                payload.write_bytes(b"HPHP49-Cpayload")
                run.return_value.returncode = 1
                run.return_value.stdout = ""
                run.return_value.stderr = "writer failed"
                with self.assertRaisesRegex(RuntimeError, "writer failed"):
                    APLET.package(payload, output, root / "tool", root, "Example", "Author")
                self.assertEqual(retained.read_bytes(), b"old complete application")


if __name__ == "__main__":
    unittest.main()
