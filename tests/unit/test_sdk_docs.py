import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("sdk_check", ROOT / "tests/sdk/check.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class SdkDocsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.package = Path(self.temp.name) / "sdk"
        shutil.copytree(ROOT / "docs/sdk", self.package / "docs")
        shutil.copyfile(ROOT / "docs/development/licensing.md", self.package / "docs/licensing.md")

    def test_public_guides_work_without_development_tree(self):
        CHECK.check_public_docs(self.package)
        page = self.package / "docs/migration.md"
        source = page.read_text(encoding="utf-8")
        source = source.replace(
            "https://maxmixalex.github.io/hpclang/development/hpgcc-compat/#historical-console-output",
            "../development/hpgcc-compat.md#historical-console-output",
        )
        page.write_text(source, encoding="utf-8")
        with self.assertRaisesRegex(AssertionError, "Broken installed documentation link"):
            CHECK.check_public_docs(self.package)

    def test_missing_guide_rejects(self):
        (self.package / "docs/licensing.md").unlink()
        with self.assertRaisesRegex(AssertionError, "missing a public guide"):
            CHECK.check_public_docs(self.package)

    def test_examples_are_not_links_but_escape_is_rejected(self):
        page = self.package / "docs/extra.md"
        page.write_text("```cpp\nvalue[index](argument);\n```\n`value[index](argument)`\n")
        CHECK.check_public_docs(self.package)
        (self.package.parent / "outside.md").write_text("outside installation\n")
        page.write_text("[unbundled](../../outside.md)\n")
        with self.assertRaisesRegex(AssertionError, "Broken installed documentation link"):
            CHECK.check_public_docs(self.package)


if __name__ == "__main__":
    unittest.main()
