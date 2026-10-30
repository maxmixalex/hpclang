"""Resource snapshots preserve byte-exact evidence without changing guest memory."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cc"), "requires host C compiler")
class ResourceSnapshotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="resource-snapshot-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        source = cls.root / "probe.c"
        source.write_text(
            '#include "resource-snapshot.h"\n'
            "static int calls;\n"
            "static int read_memory(void *ctx, uint32_t address, void *buffer, size_t count) {\n"
            " ++calls; if (ctx || address != 0x08000000u) return -1;\n"
            " for (size_t i=0;i<count;++i) ((unsigned char *)buffer)[i]=(unsigned char)i;\n"
            " return 0;\n}\n"
            "int main(int argc, char **argv) {\n"
            " if(argc != 5) return 2;\n"
            " int result=resource_snapshot(argv[1],argv[2],(unsigned)atoi(argv[3]),123456,32,211,\n"
            " read_memory,atoi(argv[4]) ? &calls : NULL);\n"
            ' printf("%d %d\\n",result,calls); return 0;\n}\n',
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
                "-I",
                str(ROOT / "tests/e2e/harness/x50ng"),
                str(source),
                "-o",
                str(cls.binary),
            ],
            check=True,
            capture_output=True,
        )

    def setUp(self):
        self.temp_case = tempfile.TemporaryDirectory(dir=self.root)
        self.addCleanup(self.temp_case.cleanup)
        self.path = Path(self.temp_case.name)
        self.prefix = str(self.path / "capture")

    def invoke(self, phase="before-launch", model=1, failure=0, prefix=None):
        return subprocess.check_output(
            [
                str(self.binary),
                self.prefix if prefix is None else prefix,
                phase,
                str(model),
                str(failure),
            ],
            text=True,
        ).strip()

    def test_disabled_does_not_read(self):
        self.assertEqual(self.invoke(model=0, failure=1, prefix=""), "0 0")
        self.assertEqual(list(self.path.iterdir()), [])

    def test_all_models_have_exact_bytes_and_identity(self):
        for model in range(1, 7):
            phase = f"model-{model}"
            self.assertEqual(self.invoke(phase, model), "1 1")
            size = 0x40000 if model == 4 else 0x80000
            self.assertEqual(
                (self.path / f"capture-{phase}.sram").read_bytes(),
                bytes(range(256)) * (size // 256),
            )
            self.assertEqual(
                json.loads((self.path / f"capture-{phase}.json").read_text()),
                dict(
                    version=1,
                    phase=phase,
                    model=model,
                    time_us=123456,
                    pc=32,
                    cpsr=211,
                    base=0x08000000,
                    bytes=size,
                ),
            )

    def test_invalid_selection_never_reads(self):
        for phase, model in (
            ("../escape", 1),
            ('bad"json', 1),
            ("", 1),
            ("a" * 64, 1),
            ("valid", 0),
            ("valid", 7),
        ):
            self.assertEqual(self.invoke(phase, model), "-1 0")
        self.assertEqual(list(self.path.iterdir()), [])

    def test_read_failure_leaves_no_evidence(self):
        self.assertEqual(self.invoke(failure=1), "-1 1")
        self.assertEqual(list(self.path.iterdir()), [])

    def test_existing_evidence_is_never_overwritten(self):
        self.assertEqual(self.invoke(), "1 1")
        before = {p.name: p.read_bytes() for p in self.path.iterdir()}
        self.assertEqual(self.invoke(), "-1 1")
        self.assertEqual({p.name: p.read_bytes() for p in self.path.iterdir()}, before)

    def test_sidecar_collision_removes_only_new_raw_file(self):
        sidecar = self.path / "capture-before-launch.json"
        sidecar.write_text("retained")
        self.assertEqual(self.invoke(), "-1 1")
        self.assertEqual(list(self.path.iterdir()), [sidecar])
        self.assertEqual(sidecar.read_text(), "retained")
