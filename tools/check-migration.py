#!/usr/bin/env python3
"""Read-only lexical migration hints; not a compiler or compatibility certificate."""

import argparse
import bisect
import codecs
import json
import os
import re
import stat
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
        "HP012",
        r"\bhpg_draw_(?:text|letter)(?:_on)?\s*\(",
        "Review text encoding: HPG text uses byte-indexed glyphs, not UTF-8. Keeping HPG requires its selected font's byte mapping; moving to hpclang_surface_draw_text/utf8 requires explicit transcoding of legacy bytes. Select a font with hpclang_target_font and include dynamic characters explicitly; source extraction cannot prove runtime glyph coverage. --encoding only decodes source for this checker; it does not transcode application text.",
    ),
    (
        "HP011",
        r"\b(?:kos_beep|kos_CheckBeepEnd)\s*\(",
        "Review KOS tone semantics: durations are milliseconds; mode bit 0 permits replacement and bit 1 waits. kos_CheckBeepEnd returning true does not guarantee silence. Use hpclang_sound_status for playing/completed/error status and hpclang_sound_stop for cancellation; check operation failures.",
    ),
    (
        "HP009",
        r"\b(?:FSOpen|FSCreate|FSOpenDir)\s*\(",
        "Review SD filesystem availability: query hpclang_device_capability(HPCLANG_CAPABILITY_STREAM_STORAGE). This backend is supported on HP49g+/HP50g, not HP39/HP40 or HP48gII; supported does not guarantee media presence or operation success. Check the FS result.",
    ),
    (
        "HP010",
        r"\b(?:sat_dir_open|sat_dir_chdir|sat_dir_getobject|sat_dir_rclobject|sat_dir_searchobject|sat_dir_getreal|sat_dir_rclreal|_sat_dir_fetchreal)\s*\(",
        "Review native RPL directory availability: query hpclang_device_capability(HPCLANG_CAPABILITY_RPL_OBJECT_STORE). Distinguish unsupported models from context-unavailable launches; this is not HP39 Aplet storage. Check each adapter's error contract even when supported.",
    ),
    (
        "HP008",
        r"\b(?:ints|uints|sys_bcd2str)\s*\(",
        "Review borrowed formatting storage: do not free or adopt the returned pointer. Copy text before another call reuses its buffer; use a caller-owned buffer for retained results.",
    ),
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


def source_encoding(value):
    try:
        # Reject unknown and non-text codecs at argument parsing, including
        # transformations such as base64 that str.decode cannot consume.
        if not isinstance(codecs.decode(b"", value), str):
            raise TypeError("not a text encoding")
    except (LookupError, TypeError) as error:
        raise argparse.ArgumentTypeError(f"unsupported source encoding: {value}") from error
    return value


def source_files(directory, errors):
    # Prune before descent: build trees can be much larger than application
    # sources. os.walk reports unreadable directories through onerror instead
    # of silently presenting an incomplete scan as a clean project.
    for parent, directories, names in os.walk(
        directory,
        topdown=True,
        onerror=lambda error: errors.append(str(error)),
        followlinks=False,
    ):
        directories[:] = sorted(name for name in directories if name not in SKIP)
        for name in sorted(names):
            path = Path(parent) / name
            if path.suffix.lower() not in SUFFIXES:
                continue
            try:
                if stat.S_ISREG(path.stat().st_mode):
                    yield path.resolve()
            except OSError as error:
                errors.append(f"{path}: {error}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path, help="C/C++ files or source directories")
    parser.add_argument("--json", action="store_true", help="emit a versioned JSON report")
    parser.add_argument("--check", action="store_true", help="exit 1 when review findings exist")
    parser.add_argument(
        "--encoding",
        type=source_encoding,
        default="utf-8-sig",
        help="source text encoding (default: UTF-8 with optional BOM); never rewrites files",
    )
    args = parser.parse_args(argv)
    files, errors, findings = set(), [], []
    for path in args.paths:
        if path.is_file():
            files.add(path.resolve())
        elif path.is_dir():
            files.update(source_files(path, errors))
        else:
            errors.append(f"{path}: path does not exist")
    for path in sorted(files):
        try:
            findings.extend(scan(path.read_text(encoding=args.encoding), path))
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
