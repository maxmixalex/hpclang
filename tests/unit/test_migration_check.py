"""Migration diagnostics must not mistake comments/literals for calls."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


TOOL = Path(__file__).resolve().parents[2] / "tools/check-migration.py"
SPEC = importlib.util.spec_from_file_location("migration_check", TOOL)
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class MigrationTests(unittest.TestCase):
    def test_digit_separators_do_not_hide_calls(self):
        source = (
            "auto a = 1'000; gets(buffer); auto b = 2'000;\n"
            "auto c = 0xAB'CD; exit(1); auto d = 0b10'01;\n"
            "auto e = 1.2'3e+4; sys_playTone(440, 10);\n"
            "auto f = .1'2; set_decimal_digits(2);\n"
            "auto g = 1'000_suffix; sat_peek(0);\n"
            "char c1 = 'a'; char32_t c2 = U'b';\n"
            'const char *s = "1\'000; gets(hidden);";\n'
            "// 2'000; exit(0);\n"
        )
        rows = CHECK.scan(source, "numbers.cpp")
        self.assertEqual(
            [(row["rule"], row["line"]) for row in rows],
            [("HP002", 1), ("HP003", 2), ("HP004", 3), ("HP007", 4), ("HP001", 5)],
        )
        self.assertEqual(rows[0]["column"], source.index("gets") + 1)

    def test_comments_and_literals(self):
        source = """// gets(x);\n/* sat_peek(0); */
const char *s = "exit(1)";
const char *r = R"tag(
#include <hpgcc3.h>
sys_playTone(440, 10);
)tag";
extern int errno;
char c = '\\'';
"""
        self.assertEqual(CHECK.scan(source, "literal.cpp"), [])

    def test_positions_and_splicing(self):
        source = '#include "hpgcc49.h"\n// hidden \\\ngets(x);\n  sat_\\\npeek(0);\n'
        rows = CHECK.scan(source, "old.c")
        self.assertEqual(
            [(r["rule"], r["line"], r["column"]) for r in rows], [("HP006", 1, 1), ("HP001", 4, 3)]
        )

    def test_review_categories_and_identifier_boundaries(self):
        source = "void f() { gets(x); _Exit(1); sys_playTone2(440, 2); }\nint errno;\nmygets(x);"
        self.assertEqual(
            [r["rule"] for r in CHECK.scan(source, "old.c")], ["HP002", "HP003", "HP004", "HP005"]
        )

    def test_decimal_settings_request_review_even_with_local_opt_in(self):
        source = (
            "#define HPCLANG_HPGCC_LEGACY_FORMAT 1\n"
            "// set_decimal_digits(2);\n"
            'const char *s = "set_decimal_separator(44)";\n'
            "set_decimal_digits /* precision */ (2);\n"
            "set_decimal_\\\nseparator(44);\n"
            "my_set_decimal_digits(2);\n"
        )
        rows = CHECK.scan(source, "format.c")
        self.assertEqual(
            [(row["rule"], row["line"], row["column"]) for row in rows],
            [("HP007", 4, 1), ("HP007", 5, 1)],
        )
        self.assertTrue(all("HPCLANG_HPGCC_LEGACY_FORMAT=1" in row["message"] for row in rows))

    def test_cli_is_read_only_deduplicates_and_skips_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "old.c"
            original = b"gets(buffer);\n"
            source.write_bytes(original)
            (root / "build").mkdir()
            (root / "build/generated.c").write_text("exit(0);")
            result = subprocess.run(
                [sys.executable, str(TOOL), "--json", "--check", str(root), str(source)],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 1)
            report = json.loads(result.stdout)
            self.assertEqual(report["files"], 1)
            self.assertEqual(len(report["findings"]), 1)
            self.assertEqual(source.read_bytes(), original)

    def test_io_errors_are_not_clean_results(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "bad.c"
            source.write_bytes(b"\xff")
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOL),
                    "--json",
                    str(source),
                    str(source.with_name("missing.c")),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertEqual(len(json.loads(result.stdout)["errors"]), 2)


if __name__ == "__main__":
    unittest.main()
