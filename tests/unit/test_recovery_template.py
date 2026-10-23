"""Cross-check the native-template oracle against independent fixture creation."""

import importlib
from pathlib import Path
import sys
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
with patch.object(sys, "path", [str(ROOT / "tests/e2e/port-recovery"), *sys.path]):
    INSTALLATION = importlib.import_module("installation")
    TEMPLATE = importlib.import_module("template")


class TemplateTests(unittest.TestCase):
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
