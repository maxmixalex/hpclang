import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("sdk_package", ROOT / "tools/package-sdk.py")
PACK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACK)


class SdkPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prefix = self.root / "first prefix"
        self.sdk = self.prefix / "share/hpclang"
        for name in ("LICENSE", "cmake/hpclangConfig.cmake", "include/hpclang/abi.h"):
            path = self.sdk / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((name + "\n").encode())
        (self.sdk / "example.sh").write_bytes(b"#!/bin/sh\nexit 0\n")
        self.output = self.root / "sdk.zip"

    def test_reproducible_after_copy_timestamp_and_mode_changes(self):
        first = PACK.package(self.prefix, self.output)
        other = self.root / "different prefix"
        shutil.copytree(self.prefix, other)
        for path in other.rglob("*"):
            if path.is_file():
                os.utime(path, (1700000000, 1700000000))
                path.chmod(0o600)
        second = self.root / "other.zip"
        self.assertEqual(PACK.package(other, second), first)
        self.assertEqual(second.read_bytes(), self.output.read_bytes())
        with zipfile.ZipFile(second) as archive:
            manifest = json.loads(archive.read(PACK.MANIFEST))
            self.assertEqual(set(manifest["files"]), set(archive.namelist()) - {PACK.MANIFEST})
            for name, entry in manifest["files"].items():
                data = archive.read(name)
                self.assertEqual(entry["size"], len(data))
                self.assertEqual(entry["sha256"], hashlib.sha256(data).hexdigest())
                self.assertEqual(archive.getinfo(name).external_attr >> 16 & 0o777, entry["mode"])
            archive.extractall(self.root / "unpacked")
        self.assertEqual((self.root / "unpacked/share/hpclang/LICENSE").read_bytes(), b"LICENSE\n")

    def test_missing_sdk_and_self_inclusion_reject(self):
        with self.assertRaises(ValueError):
            PACK.package(self.root / "missing", self.output)
        with self.assertRaises(ValueError):
            PACK.package(self.prefix, self.sdk / "recursive.zip")
        self.assertFalse(self.output.exists())

    def test_symlink_reject_preserves_previous_archive(self):
        PACK.package(self.prefix, self.output)
        before = self.output.read_bytes()
        try:
            (self.sdk / "external").symlink_to(self.root, target_is_directory=True)
        except OSError:
            self.skipTest("symbolic links unavailable")
        with self.assertRaises(ValueError):
            PACK.package(self.prefix, self.output)
        self.assertEqual(self.output.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
