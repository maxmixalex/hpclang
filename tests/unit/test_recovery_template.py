"""Cross-check the native-template oracle against independent fixture creation."""

import importlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
with patch.object(sys, "path", [str(ROOT / "tests/e2e/port-recovery"), *sys.path]):
    INSTALLATION = importlib.import_module("installation")
    TEMPLATE = importlib.import_module("template")


class TemplateTests(unittest.TestCase):
    def reservation_fixture(self, name="CUSTOM", capacity=4096):
        pack = TEMPLATE.packager()
        image = pack.bind_image(
            bytes.fromhex("1eff2fe1")
            + bytes(12)
            + b"HPCBIND1"
            + bytes(8)
            + b"HPCROOT1"
            + bytes(40)
            + b"HPCRESV1"
            + bytes(136),
            16,
            INSTALLATION.JOURNAL_SLOT,
            INSTALLATION.AUTHORITY_SLOT,
        )
        image = pack.bind_reservation(image, 80, (name, capacity))
        program = pack._recovery_program(pack.package(image, len(image), model="hp50g"))
        empty = pack.immediate(0x2D9D) + pack.immediate(0x312B)
        library = pack._configuration_library(
            empty, INSTALLATION.LIBRARY_ID, (program,) + (empty,) * 5 + (program,)
        )
        return pack, library, image

    def test_reservation_metadata_comes_from_linked_descriptor(self):
        for name, capacity in (("CUSTOM", 4096), ("HPCRES", 32768)):
            with self.subTest(name=name, capacity=capacity):
                pack, library, image = self.reservation_fixture(name, capacity)
                layout = pack.recovery_layout(
                    library, (("admission", 6, image, 16, False),), reservation_offset=80
                )
                self.assertEqual(layout["reservation"], dict(name=name, bytes=capacity))
                changed = bytearray(image)
                changed[96] ^= 1
                with self.assertRaisesRegex(ValueError, "match"):
                    pack.recovery_layout(
                        library, (("admission", 6, changed, 16, False),), reservation_offset=80
                    )

    def test_reservation_metadata_rejects_invalid_descriptor(self):
        pack, library, image = self.reservation_fixture()
        for offset in (-4, 81, len(image)):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                pack.recovery_layout(
                    library, (("admission", 6, image, 16, False),), reservation_offset=offset
                )
        for at, value in ((80, 0), (88, 0), (92, 0), (223, 1)):
            changed = bytearray(image)
            changed[at] = value
            if at == 88:
                changed[88:92] = bytes(4)
            with self.subTest(at=at), self.assertRaises(ValueError):
                pack.recovery_layout(
                    library, (("admission", 6, changed, 16, False),), reservation_offset=80
                )
        with self.assertRaisesRegex(ValueError, "admission"):
            pack.recovery_layout(library, (), reservation_offset=80)

    def test_native_template_uses_bound_reservation(self):
        pack, library, image = self.reservation_fixture("ALTRESOURCE", 8192)
        layout = pack.recovery_layout(
            library,
            (("recovery", 0, image, 16, False), ("admission", 6, image, 16, False)),
            identity_offset=32,
            reservation_offset=80,
        )
        library = pack.seal_recovery_identity(library, layout["identity_object_nibble"])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "library.hp").write_bytes(library)
            (root / "layout.json").write_text(json.dumps(layout))
            TEMPLATE.generate(root / "library.hp", root / "layout.json", root / "template.c")
            generated = (root / "template.c").read_text()
        self.assertIn("template_reservation_bytes = 8192u", generated)
        self.assertIn("template_reservation_name_length = 11u", generated)
        self.assertIn(
            "template_reservation_name[] = {" + ",".join(map(str, b"ALTRESOURCE")) + "}",
            generated,
        )

    def test_rebind_matches_library_created_with_final_coordinates(self):
        library, record, layout = INSTALLATION.fixture()
        self.assertEqual(TEMPLATE.fixture(library, layout), (library, record, layout))
        for journal, authority in ((0x180105, 0x181121), (0x1A0105, 0x1C0105)):
            with self.subTest(journal=journal, authority=authority):
                expected = INSTALLATION.fixture(journal_slot=journal, authority_slot=authority)
                actual = TEMPLATE.fixture(
                    library, layout, journal_slot=journal, authority_slot=authority
                )
                self.assertEqual(actual, expected)
                self.assertNotEqual(actual[0], library)
        self.assertEqual(INSTALLATION.fixture(), (library, record, layout))

    def test_incomplete_pair_is_not_silently_ignored(self):
        library, _, layout = INSTALLATION.fixture()
        for binding in (dict(journal_slot=0x180105), dict(authority_slot=0x181121)):
            with self.subTest(binding=binding), self.assertRaisesRegex(ValueError, "Incomplete"):
                TEMPLATE.fixture(library, layout, **binding)


if __name__ == "__main__":
    unittest.main()
