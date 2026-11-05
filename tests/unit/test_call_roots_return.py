"""A failed producing run must not be restarted as a message-query fixture."""

import importlib.util
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


VERIFY = load("recovery_return", "tests/e2e/port-recovery/verify.py")
FIXTURES = load("call_fixtures", "tests/e2e/code-relocation/fixtures.py")
with patch.dict(sys.modules, {"verify": VERIFY, "fixtures": FIXTURES}):
    CALLS = load("call_roots_return", "tests/e2e/code-relocation/call_roots.py")


class CallReturnTests(unittest.TestCase):
    def test_exception_stops_before_second_launch(self):
        for mode in (0x17, 0x1B):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "source"
                source.mkdir()
                (source / "flash").write_bytes(b"")
                (source / "sram").write_bytes(b"")
                build = root / "build"
                (build / "launchers").mkdir(parents=True)
                (build / "launchers/hp49gplus.hp").write_bytes(b"")

                def failed_run(args, phase, *unused, **kwargs):
                    state = phase / "state"
                    state.mkdir(parents=True)
                    (state / "state").write_text(f"[s3c2410-arm]\nreg-15=0x10\ncpsr={mode}\n")

                transfer = Mock(side_effect=failed_run)
                with (
                    patch.object(CALLS, "CASES", {"native-missing-message": b"\0"}),
                    patch.object(CALLS, "catalog_records", return_value=[]),
                    patch.object(CALLS, "attachment_records", return_value=[(275,)]),
                    patch.object(CALLS, "package", return_value=b"fixture"),
                    self.assertRaisesRegex(ValueError, "abort/undefined"),
                ):
                    CALLS.verify_calls(
                        SimpleNamespace(model="hp49gplus"),
                        build,
                        source,
                        root / "output",
                        transfer=transfer,
                        native_words=lambda *args: lambda *args: 0,
                    )
                transfer.assert_called_once()


if __name__ == "__main__":
    unittest.main()
