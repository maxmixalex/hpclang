"""Migration diagnostics must not mistake comments/literals for calls."""

import importlib.util
import contextlib
import errno
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


TOOL = Path(__file__).resolve().parents[2] / "tools/check-migration.py"
SPEC = importlib.util.spec_from_file_location("migration_check", TOOL)
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class MigrationTests(unittest.TestCase):
    def test_window_borrowing_distinguishes_internal_text_and_caller_strings(self):
        source = (
            "copy(win_text_get_text(w));\n"
            "win_choose_add(w, local);\n"
            "win_choose_add_with_label(w, option, label);\n"
            "copy(win_choose_get_choice(w));\n"
            "// win_text_get_text(w);\n"
            'const char *s = R"(win_choose_add(w, option);)";\n'
            "my_win_text_get_text(w); win_choose_additional(w);\n"
            "win_text_get_\\\ntext /* comment */ (w);\n"
        )
        rows = CHECK.scan(source, "widgets.cpp")
        self.assertEqual(
            [(r["rule"], r["line"], r["column"]) for r in rows],
            [("HP014", 1, 6), ("HP015", 2, 1), ("HP015", 3, 1), ("HP015", 4, 6), ("HP014", 8, 1)],
        )
        self.assertIn("next window call", rows[0]["message"])
        self.assertIn("use win_text_set_text for growth", rows[0]["message"])
        self.assertIn("rather than copying", rows[1]["message"])
        self.assertIn("does not transfer ownership", rows[3]["message"])

    def test_raw_address_translation_requires_design_review(self):
        rows = CHECK.scan("sys_map_v2p(ptr);\nsys_map_p2v(address);\n", "legacy.c")
        self.assertEqual(
            [(r["rule"], r["line"], r["column"]) for r in rows], [("HP013", 1, 1), ("HP013", 2, 1)]
        )
        self.assertTrue(all("not public hpclang APIs" in r["message"] for r in rows))
        self.assertTrue(all("fixed address offset" in r["message"] for r in rows))

    def test_address_translation_ignores_text_and_other_names(self):
        source = '// sys_map_v2p(p);\n"sys_map_p2v(a)"; my_sys_map_v2p(p);\n'
        self.assertEqual(CHECK.scan(source, "legacy.cpp"), [])

    def test_excluded_directories_are_not_entered(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "src").mkdir()
            source = root / "src/main.c"
            source.write_text("gets(buffer);")
            for name in CHECK.SKIP:
                (root / "src" / name / "nested").mkdir(parents=True)
                (root / "src" / name / "nested/generated.c").write_text("exit(0);")
            visited = []
            scandir = CHECK.os.scandir

            def observe(path):
                visited.append(Path(path))
                self.assertFalse(CHECK.SKIP.intersection(Path(path).relative_to(root).parts))
                return scandir(path)

            errors = []
            with mock.patch.object(CHECK.os, "scandir", side_effect=observe):
                files = list(CHECK.source_files(root, errors))
            self.assertEqual(files, [source.resolve()])
            self.assertEqual(errors, [])
            self.assertEqual(set(visited), {root, root / "src"})

    def test_unreadable_directory_reports_incomplete_scan(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            blocked = root / "src"
            blocked.mkdir()
            (root / "visible.c").write_text("gets(buffer);")
            scandir = CHECK.os.scandir

            def deny(path):
                if Path(path) == blocked:
                    raise PermissionError(errno.EACCES, "permission denied", str(path))
                return scandir(path)

            output = io.StringIO()
            with mock.patch.object(CHECK.os, "scandir", side_effect=deny):
                with contextlib.redirect_stdout(output):
                    result = CHECK.main(["--json", "--check", str(root)])
            report = json.loads(output.getvalue())
            self.assertEqual(result, 2)
            self.assertEqual(report["files"], 1)
            self.assertEqual(len(report["findings"]), 1)
            self.assertEqual(len(report["errors"]), 1)
            self.assertIn(str(blocked), report["errors"][0])

    def test_explicit_build_source_remains_reviewable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "build").mkdir()
            source = root / "build/generated.c"
            source.write_text("gets(buffer);")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                result = CHECK.main(["--json", "--check", str(root), str(source)])
            report = json.loads(output.getvalue())
            self.assertEqual(result, 1)
            self.assertEqual(report["files"], 1)
            self.assertEqual(report["errors"], [])
            self.assertEqual(report["findings"][0]["path"], str(source.resolve()))

    def test_text_hint_preserves_utf8_and_gbk_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "text.c"
            text = '// \u4e2d\u6587\nhpg_draw_text("\u4e2d\u6587", 0, 0);\n'
            for encoding in ("utf-8", "gbk"):
                with self.subTest(encoding=encoding):
                    original = text.encode(encoding)
                    source.write_bytes(original)
                    result = subprocess.run(
                        [
                            sys.executable,
                            str(TOOL),
                            "--json",
                            "--check",
                            "--encoding",
                            encoding,
                            str(source),
                        ],
                        capture_output=True,
                        text=True,
                    )
                    self.assertEqual(result.returncode, 1)
                    report = json.loads(result.stdout)
                    self.assertEqual(report["errors"], [])
                    self.assertEqual(
                        [(r["rule"], r["line"], r["column"]) for r in report["findings"]],
                        [("HP012", 2, 1)],
                    )
                    self.assertEqual(source.read_bytes(), original)

    def test_hpg_text_encoding_boundaries(self):
        source = (
            'hpg_draw_text /* bytes */ ("text", 0, 0);\n'
            "hpg_draw_text_on(g, dynamic, 0, 0);\n"
            "hpg_draw_letter(ch, 0, 0); hpg_draw_letter_on(g, ch, 0, 0);\n"
            "#if 0\n  hpg_draw_\\\ntext(dynamic, 0, 0);\n#endif\n"
            '// hpg_draw_text(hidden, 0, 0);\nconst char *s = "hpg_draw_letter()";\n'
            'auto raw = R"tag(hpg_draw_text_on(g, hidden, 0, 0))tag";\n'
            "my_hpg_draw_text(); hpg_draw_text_extra();\n"
            "hpclang_surface_draw_text(); hpclang_surface_draw_utf8();\n"
        )
        rows = CHECK.scan(source, "text.cpp")
        self.assertEqual(
            [(r["rule"], r["line"], r["column"]) for r in rows],
            [
                ("HP012", 1, 1),
                ("HP012", 2, 1),
                ("HP012", 3, 1),
                ("HP012", 3, 28),
                ("HP012", 5, 3),
            ],
        )
        self.assertTrue(all("byte-indexed" in r["message"] for r in rows))
        self.assertTrue(all("dynamic characters" in r["message"] for r in rows))
        self.assertTrue(all("does not transcode" in r["message"] for r in rows))

    def test_kos_tones_request_semantic_review_without_guessing_modes(self):
        source = (
            "kos_beep /* ms */ (880, 10, 0);\n"
            "if (kos_CheckBeepEnd()) saved = 1;\n"
            "#if 0\n  kos_\\\nbeep(440, 20, flags);\n#endif\n"
            '// kos_beep(1, 2, 3);\nconst char *s = "kos_CheckBeepEnd()";\n'
            'auto raw = R"tag(kos_beep(1, 2, 3))tag";\n'
            "mykos_beep(); kos_CheckBeepEnd_extra();\n"
            "hpclang_sound_status(); hpclang_sound_stop();\n"
        )
        rows = CHECK.scan(source, "audio.cpp")
        self.assertEqual(
            [(r["rule"], r["line"], r["column"]) for r in rows],
            [("HP011", 1, 1), ("HP011", 2, 5), ("HP011", 4, 3)],
        )
        self.assertTrue(all("does not guarantee silence" in r["message"] for r in rows))
        self.assertTrue(all("hpclang_sound_status" in r["message"] for r in rows))

    def test_capability_hints_cover_storage_entry_points(self):
        source = (
            "FSOpen /* path */ (name, mode, &file); FSCreate(name, 0, &file);\n"
            "FSOpenDir(name, &dir);\n"
            "sat_dir_open(); sat_dir_chdir(path); sat_dir_getobject(path);\n"
            "sat_dir_rclobject(path); sat_dir_searchobject(name, &paths);\n"
            "sat_dir_getreal(path, 0); sat_dir_rclreal(path, 0);\n"
            '_sat_dir_fetchreal(path, 0, "g");\n'
            '// FSOpen(name);\nconst char *s = "sat_dir_open()";\n'
            'auto text = R"tag(FSCreate(name); sat_dir_chdir(path);)tag";\n'
            "myFSOpen(name); FSOpenExtra(name); my_sat_dir_open();\n"
        )
        rows = CHECK.scan(source, "storage.cpp")
        self.assertEqual(
            [(r["rule"], r["line"]) for r in rows],
            [("HP009", 1), ("HP009", 1), ("HP009", 2)]
            + [("HP010", 3)] * 3
            + [("HP010", 4)] * 2
            + [("HP010", 5)] * 2
            + [("HP010", 6)],
        )
        self.assertIn("HPCLANG_CAPABILITY_STREAM_STORAGE", rows[0]["message"])
        self.assertIn("HPCLANG_CAPABILITY_RPL_OBJECT_STORE", rows[-1]["message"])

    def test_capability_hint_does_not_claim_to_validate_guards(self):
        source = (
            "if (hpclang_device_capability(HPCLANG_CAPABILITY_STREAM_STORAGE) == 1)\n"
            "  FSOpen(name, mode, &file);\n"
            "#if 0\n  sat_dir_\\\nopen();\n#endif\n"
        )
        rows = CHECK.scan(source, "guarded.c")
        self.assertEqual(
            [(r["rule"], r["line"], r["column"]) for r in rows],
            [("HP009", 2, 3), ("HP010", 4, 3)],
        )

    def test_borrowed_buffers_report_calls_without_claiming_a_lifetime_proof(self):
        source = (
            "auto *a = ints /* base */ (12, 10);\n"
            "auto *b = uints(34, 10);\n"
            "auto *c = sys_bcd2str(0x1234);\n"
            '// ints(1, 10);\nconst char *s = "uints(2, 10)";\n'
            'auto text = R"tag(sys_bcd2str(0);)tag";\n'
            "myints(1, 10); uints_extra(2, 10);\n"
            "copy(ints(5, 10));\n"
        )
        rows = CHECK.scan(source, "owners.cpp")
        self.assertEqual(
            [(r["rule"], r["line"], r["column"]) for r in rows],
            [("HP008", 1, 11), ("HP008", 2, 11), ("HP008", 3, 11), ("HP008", 8, 6)],
        )
        self.assertTrue(all("do not free or adopt" in row["message"] for row in rows))

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
            [(r["rule"], r["line"], r["column"]) for r in rows],
            [("HP006", 1, 1), ("HP001", 4, 3)],
        )

    def test_review_categories_and_identifier_boundaries(self):
        source = "void f() { gets(x); _Exit(1); sys_playTone2(440, 2); }\nint errno;\nmygets(x);"
        self.assertEqual(
            [r["rule"] for r in CHECK.scan(source, "old.c")],
            ["HP002", "HP003", "HP004", "HP005"],
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
                [
                    sys.executable,
                    str(TOOL),
                    "--json",
                    "--check",
                    str(root),
                    str(source),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 1)
            report = json.loads(result.stdout)
            self.assertEqual(report["files"], 1)
            self.assertEqual(len(report["findings"]), 1)
            self.assertEqual(source.read_bytes(), original)

    def test_legacy_encoding_is_explicit_and_preserves_bytes_and_locations(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "legacy.c"
            original = "// caf\u00e9\r\nvoid f() { gets(buffer); }\r\n".encode("cp1252")
            source.write_bytes(original)
            command = [sys.executable, str(TOOL), "--json", "--check", str(source)]
            default = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(default.returncode, 2)
            self.assertTrue(json.loads(default.stdout)["errors"])
            selected = subprocess.run(
                command + ["--encoding", "cp1252"], capture_output=True, text=True
            )
            self.assertEqual(selected.returncode, 1)
            report = json.loads(selected.stdout)
            self.assertEqual(report["errors"], [])
            self.assertEqual(
                [(r["rule"], r["line"], r["column"]) for r in report["findings"]],
                [("HP002", 2, 12)],
            )
            self.assertEqual(source.read_bytes(), original)

    def test_invalid_or_nontext_encoding_is_an_argument_error(self):
        for encoding in ("not-an-encoding", "base64_codec", "rot_13"):
            result = subprocess.run(
                [sys.executable, str(TOOL), "--encoding", encoding, str(TOOL)],
                capture_output=True,
                text=True,
            )
            with self.subTest(encoding=encoding):
                self.assertEqual(result.returncode, 2)
                self.assertIn("unsupported source encoding", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

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
