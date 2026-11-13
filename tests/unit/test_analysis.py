from pathlib import Path
import json
import os
import runpy
import shutil
import subprocess
import sys
import tempfile
import unittest


compile_key = runpy.run_path(str(Path(__file__).resolve().parents[2] / "tools/analyze.py"))[
    "compile_key"
]


class AnalysisCommands(unittest.TestCase):
    def test_move_contract_exception_is_limited_to_timer_fixtures(self):
        arguments = runpy.run_path(str(Path(__file__).resolve().parents[2] / "tools/analyze.py"))[
            "contract_test_analyzer_args"
        ]
        for source in (
            "tests/unit/test_timer_cpp.cpp",
            "tests/e2e/system-services/timer.cpp",
        ):
            self.assertEqual(
                arguments(source),
                [
                    "--extra-arg=-Xanalyzer",
                    "--extra-arg=-analyzer-disable-checker=cplusplus.Move",
                ],
            )
        for source in (
            "include/hpclang/timer.hpp",
            "runtime/core/timer.c",
            "examples/timer-cpp/main.cpp",
            "tests/unit/other.cpp",
            "examples/test_timer_cpp.cpp",
        ):
            self.assertEqual(arguments(source), [], source)

    def key(self, extra=(), directory="/build/a", output="first.o"):
        return compile_key(
            [
                "/tools/clang",
                "-I/sdk/include",
                "--target=arm-none-eabi",
                "-march=armv4t",
                "-ffunction-sections",
                *extra,
                "-o",
                output,
                "-c",
                "/sdk/runtime.c",
            ],
            Path(directory),
        )

    def test_same_configuration_in_different_targets(self):
        self.assertEqual(self.key(), self.key(directory="/build/b", output="second.o"))

    def test_definitions_and_architectures_remain_distinct(self):
        self.assertNotEqual(self.key(["-DMODEL=1"]), self.key(["-DMODEL=2"]))
        self.assertNotEqual(self.key(), self.key(["--target=x86_64-linux-gnu"]))

    def test_relative_include_paths_keep_their_context(self):
        for option in ("-I", "-isystem", "-iquote", "-include", "-imacros"):
            with self.subTest(option=option):
                self.assertNotEqual(
                    self.key([option, "generated"]),
                    self.key([option, "generated"], directory="/build/b"),
                )
        self.assertNotEqual(self.key(["-Igenerated"]), self.key(["-Igenerated"], directory="/b"))

    def test_absolute_include_spelling_is_preserved(self):
        self.assertNotEqual(self.key(["-I/sdk/alias"]), self.key(["-I/sdk/real"]))

    def test_unknown_options_and_response_files_keep_context(self):
        for option in (
            "@options",
            "-fmodule-map-file=module.map",
            "-new-compiler-option",
        ):
            with self.subTest(option=option):
                self.assertNotEqual(self.key([option]), self.key([option], directory="/build/b"))


@unittest.skipUnless(os.name == "posix" and shutil.which("git"), "POSIX analysis runner")
class AnalysisRunner(unittest.TestCase):
    def test_second_configuration_failure_and_duplicate_elimination(self):
        for tracked in (True, False):
            with self.subTest(tracked=tracked):
                self.second_configuration_case(tracked)

    def test_ignored_sources_are_excluded_but_unstaged_sources_are_owned(self):
        owned_sources = runpy.run_path(
            str(Path(__file__).resolve().parents[2] / "tools/analyze.py")
        )["owned_sources"]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            (root / ".gitignore").write_text("ignored/\n")
            (root / "tracked.c").write_text("int tracked;\n")
            subprocess.run(["git", "-C", str(root), "add", "tracked.c"], check=True)
            (root / "new source.c").write_text("int added;\n")
            (root / "ignored").mkdir()
            (root / "ignored/private.c").write_text("int ignored;\n")
            self.assertEqual(owned_sources(root), {".gitignore", "tracked.c", "new source.c"})

    def second_configuration_case(self, tracked):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "tools").mkdir()
            script = root / "tools/analyze.py"
            shutil.copyfile(Path(__file__).resolve().parents[2] / "tools/analyze.py", script)
            source = root / "sample.c"
            source.write_text("int sample(void) { return 0; }\n")
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            if tracked:
                subprocess.run(["git", "-C", str(root), "add", "sample.c"], check=True)
            checker = root / "check"
            checker.write_text(
                f"#!{sys.executable}\n"
                "import json, pathlib, sys\n"
                "db=pathlib.Path(sys.argv[sys.argv.index('-p')+1])/'compile_commands.json'\n"
                "entries=json.loads(db.read_text())\n"
                "assert len(entries)==2, entries\n"
                "sys.exit(any('-DBROKEN=1' in e['arguments'] for e in entries))\n"
            )
            checker.chmod(0o755)
            command = [sys.executable, str(script), str(checker)]
            for index, broken in enumerate((0, 0, 1)):
                build = root / f"build{index}"
                build.mkdir()
                entry = {
                    "directory": str(build),
                    "file": str(source),
                    "arguments": [
                        "/tools/clang",
                        f"-DBROKEN={broken}",
                        "-c",
                        str(source),
                        "-o",
                        f"output{index}.o",
                    ],
                }
                (build / "compile_commands.json").write_text(json.dumps([entry]))
                command.extend(("--arm-build", str(build)))
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("3 compile entries, 2 unique configurations", result.stdout)
            self.assertIn("1/1 sources completed", result.stdout)


if __name__ == "__main__":
    unittest.main()
