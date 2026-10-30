#!/usr/bin/env python3
"""Read-only lexical migration hints; not a compiler or compatibility certificate."""

import argparse
import bisect
import json
import re
import sys
from pathlib import Path


SUFFIXES = {".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".hxx"}
SKIP = {".git", "build", ".cache", "third_party"}
TOKENS = re.compile(
    # Consume preprocessing numbers before character literals: C++ digit
    # separators must not hide the code between two numeric constants.
    r"(?<![\w.])(?:[0-9]|\.[0-9])(?:[eEpP][+-]|[\w.]|'[\w])*|"
    r'//[^\n]*|/\*.*?\*/|(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})'
    r'\(.*?\)(?P=delimiter)"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
    re.DOTALL,
)
RULES = (
    (
        "HP001",
        r"\b(?:sat_peek\w*|sat_poke\w*)\s*\(",
        "Review raw Saturn access; use object snapshots/logical object-store handles and port services.",
    ),
    (
        "HP002",
        r"\b(?:gets|_gets)\s*\(",
        "Use hpconio.h only for fixed arrays; pointers need hpclang_hpgcc_gets(buffer, capacity). Check cancellation.",
    ),
    (
        "HP003",
        r"\b(?:exit|_Exit|quick_exit)\s*\(",
        "Check cleanup and saved data: exit does not unwind automatic C++ owners. Prefer normal return or explicit cleanup.",
    ),
    (
        "HP004",
        r"\b(?:sys_playTone|sys_playTone2)\s*\(",
        "Check units: sys_playTone uses centiseconds; sys_playTone2 uses milliseconds. Both block.",
    ),
    (
        "HP005",
        r"(?m)^[ \t]*(?:volatile\s+)?int\s+errno\s*(?:=|;)",
        "Remove the application errno definition; include errno.h and inspect errno only after an operation fails.",
    ),
    (
        "HP007",
        r"\b(?:set_decimal_digits|set_decimal_separator)\s*\(",
        "Review legacy formatting: these settings do not change standard printf. Define HPCLANG_HPGCC_LEGACY_FORMAT=1 before the first compatibility include, or call named hpclang_hpgcc_* formatters; audit the legacy format grammar.",
    ),
)


def scan(source, path):
    # C line splicing precedes comment recognition. Keep an offset map so
    # diagnostics still point to the original physical source line/column.
    positions = []
    logical = []
    index = 0
    while index < len(source):
        splice = re.match(r"\\\r?\n", source[index:]) if source[index] == "\\" else None
        if splice:
            index += len(splice[0])
            continue
        positions.append(index)
        logical.append(source[index])
        index += 1
    logical = "".join(logical)
    comments = list(logical)
    code = list(logical)
    for match in TOKENS.finditer(logical):
        for at in range(match.start(), match.end()):
            if logical[at] != "\n":
                code[at] = " "
                if match[0].startswith(("//", "/*")):
                    comments[at] = " "
    code, comments = "".join(code), "".join(comments)
    lines = [-1] + [i for i, value in enumerate(source) if value == "\n"]
    findings = []

    def emit(at, rule, message):
        original = positions[at]
        line = bisect.bisect_left(lines, original)
        findings.append(
            dict(
                path=str(path),
                line=line,
                column=original - lines[line - 1],
                rule=rule,
                message=message,
            )
        )

    for rule, pattern, message in RULES:
        for match in re.finditer(pattern, code):
            emit(match.start(), rule, message)
    for match in re.finditer(r'^\s*#\s*include\s*[<"](hpgcc49|hpgcc3)\.h[>"]', comments, re.M):
        # The directive itself must be code, not a line inside a raw string.
        at = comments.index("#", match.start(), match.end())
        if code[at] == "#":
            emit(
                at,
                "HP006",
                "Replace the legacy umbrella with individual installed compatibility headers; do not reuse old startup objects or binary libraries.",
            )
    return sorted(findings, key=lambda item: (item["line"], item["column"], item["rule"]))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path, help="C/C++ files or source directories")
    parser.add_argument("--json", action="store_true", help="emit a versioned JSON report")
    parser.add_argument("--check", action="store_true", help="exit 1 when review findings exist")
    args = parser.parse_args(argv)
    files, errors, findings = set(), [], []
    for path in args.paths:
        if path.is_file():
            files.add(path.resolve())
        elif path.is_dir():
            files.update(
                p.resolve()
                for p in path.rglob("*")
                if p.is_file()
                and p.suffix.lower() in SUFFIXES
                and not SKIP.intersection(p.relative_to(path).parts)
            )
        else:
            errors.append(f"{path}: path does not exist")
    for path in sorted(files):
        try:
            findings.extend(scan(path.read_text(encoding="utf-8-sig"), path))
        except (OSError, UnicodeError) as error:
            errors.append(f"{path}: {error}")
    report = dict(
        schema_version=1,
        files=len(files),
        findings=findings,
        errors=errors,
        scope="Lexical review only: no macro expansion, type/lifetime analysis or target validation; inactive preprocessor branches are included.",
    )
    if args.json:
        print(json.dumps(report, ensure_ascii=True, indent=2))
    else:
        for finding in findings:
            print("{path}:{line}:{column}: {rule}: {message}".format(**finding))
        for error in errors:
            print(error, file=sys.stderr)
        print(f"Scanned {len(files)} files; {len(findings)} review findings. {report['scope']}")
    return 2 if errors else 1 if args.check and findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
