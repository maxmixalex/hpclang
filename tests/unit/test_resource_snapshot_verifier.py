"""Malformed allocator and snapshot metadata must not become capacity evidence."""

from pathlib import Path
import json
import runpy
import struct
import tempfile
import unittest
from unittest.mock import patch

VERIFY = runpy.run_path(
    str(Path(__file__).resolve().parents[1] / "e2e/harness/verify-resource-snapshots.py")
)
BASE = 0x08000000


class ResourceVerifierTests(unittest.TestCase):
    def memory(self):
        data = bytearray(256)
        struct.pack_into("<I", data, 0, BASE + 64)
        struct.pack_into("<4I", data, 64, 32, 0x100, 0, 0)
        struct.pack_into("<I", data, 112, 32)
        return data

    def test_valid_free_capacity(self):
        heaps = VERIFY["free_capacity"](self.memory(), BASE)
        self.assertEqual(
            heaps[0],
            dict(
                heap=256,
                blocks=1,
                usable_bytes=32,
                largest_usable_bytes=32,
                metadata_bytes=20,
            ),
        )
        self.assertEqual(heaps[1]["blocks"], 0)

    def test_invalid_blocks(self):
        for offset, value in (
            (0, BASE + 65),
            (64, 0xFFFFFFFC),
            (64, 31),
            (68, 0x2100),
            (68, 0x200),
            (72, BASE + 64),
            (76, BASE + 100),
            (112, 0),
        ):
            with (
                self.subTest(offset=offset, value=value),
                self.assertRaises(ValueError),
            ):
                memory = self.memory()
                struct.pack_into("<I", memory, offset, value)
                VERIFY["free_capacity"](memory, BASE)

    def test_root_outside_snapshot(self):
        with self.assertRaises(ValueError):
            VERIFY["free_capacity"](self.memory(), BASE + 256)

    def test_metadata_schema_and_types(self):
        valid = dict(
            version=1,
            model=6,
            phase="application-pass",
            time_us=42,
            pc=32,
            cpsr=211,
            base=BASE,
            bytes=0x80000,
        )
        VERIFY["metadata"](valid, 6, "application-pass", 0x80000)
        variants = [
            dict(valid, model=True),
            dict(valid, model=4),
            dict(valid, phase="../bad"),
            dict(valid, time_us=-1),
            dict(valid, pc=1 << 32),
            dict(valid, bytes=0x40000),
            dict(valid, extra=0),
            {k: v for k, v in valid.items() if k != "base"},
        ]
        for data in variants:
            with self.subTest(data=data), self.assertRaises(ValueError):
                VERIFY["metadata"](data, 6, "application-pass", 0x80000)

    def test_unreviewed_rom_is_rejected(self):
        with self.assertRaises(ValueError):
            VERIFY["allocator_root"](bytes(512), 6)

    def lifecycle(self, directory):
        """Synthetic capture for lifecycle checks; ROM identity is tested separately."""
        (directory / "state").mkdir()
        (directory / "state/flash").write_bytes(b"synthetic ROM")
        (directory / "hp50g-stock.json").write_text(
            json.dumps(dict(status="pass", saw_pass=True, returned=True, return_settled=True))
        )
        ram = self.memory() + bytes(0x80000 - 256)
        for time, phase in enumerate(("prelaunch", "application-pass", "settled-return"), 1):
            stem = directory / f"hp50g-{phase}"
            stem.with_suffix(".sram").write_bytes(ram)
            stem.with_suffix(".json").write_text(
                json.dumps(
                    dict(
                        version=1,
                        model=6,
                        phase=phase,
                        time_us=time,
                        pc=32,
                        cpsr=211,
                        base=BASE,
                        bytes=len(ram),
                    )
                )
            )

    def test_lifecycle_report_fingerprints_and_order(self):
        verify = VERIFY["verify"]
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch.dict(
                verify.__globals__,
                allocator_root=lambda rom, model: (BASE, "synthetic"),
            ),
        ):
            directory = Path(tmp)
            self.lifecycle(directory)
            report = verify(directory, "hp50g")
            self.assertTrue(report["passed"])
            self.assertEqual([s["time_us"] for s in report["snapshots"]], [1, 2, 3])
            self.assertEqual(report["snapshots"][0]["heaps"][0]["usable_bytes"], 32)
            for digest in (
                report["rom_sha256"],
                report["status_sha256"],
                *(s["sram_sha256"] for s in report["snapshots"]),
            ):
                self.assertRegex(digest, r"^[0-9a-f]{64}$")

    def test_incomplete_or_failed_lifecycle_is_rejected(self):
        verify = VERIFY["verify"]
        for defect in (
            "missing-return",
            "missing-sidecar",
            "truncated",
            "unsettled",
            "reversed",
            "no-earlier-phase",
            "orphan",
        ):
            with (
                self.subTest(defect=defect),
                tempfile.TemporaryDirectory() as tmp,
                patch.dict(
                    verify.__globals__,
                    allocator_root=lambda rom, model: (BASE, "synthetic"),
                ),
            ):
                directory = Path(tmp)
                self.lifecycle(directory)
                returned = directory / "hp50g-settled-return.sram"
                if defect == "missing-return":
                    returned.unlink()
                    returned.with_suffix(".json").unlink()
                elif defect == "missing-sidecar":
                    returned.with_suffix(".json").unlink()
                elif defect == "truncated":
                    returned.write_bytes(returned.read_bytes()[:-1])
                elif defect == "unsettled":
                    status = directory / "hp50g-stock.json"
                    data = json.loads(status.read_text())
                    data["return_settled"] = False
                    status.write_text(json.dumps(data))
                elif defect == "reversed":
                    sidecar = returned.with_suffix(".json")
                    data = json.loads(sidecar.read_text())
                    data["time_us"] = 0
                    sidecar.write_text(json.dumps(data))
                elif defect == "no-earlier-phase":
                    for suffix in (".json", ".sram"):
                        (directory / ("hp50g-prelaunch" + suffix)).unlink()
                else:
                    (directory / "hp50g-orphan.json").write_text('{"phase":"orphan"}')
                with self.assertRaises((ValueError, FileNotFoundError)):
                    verify(directory, "hp50g")
