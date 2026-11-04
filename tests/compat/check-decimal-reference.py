"""Probe pinned caller-supplied HPGCC decimal formatters with documented host shims."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


FILES = ("stdio/vsprintf.c", "strings/dtoa.c", "math/dround.c")
RELEASES = (
    (
        "hpgcc1.1-with-39g-support-windows.zip",
        "sources/hplib/",
        (
            "4f0b4585eab7acec8c77f3e00e1e59b67016258c777244b0eee8bf1964c680c5",
            "7d41d06fe3b3af21907d0683eb169ceab2eeb88cf9febaf16ecae056dfc662dd",
            "5c1a2a0f44013283cbb3d8d7c2c96fdeb2983774b5564bcf72c83f2b7e1d0a7a",
        ),
    ),
    (
        "hpgcc-2.0-SP2-Linux.tar.bz2",
        "hpgcc_20/sources/hplib/",
        (
            "2f96c7c34e151dacf1d238b7c85349e088296548e21a0545d2012fa31f5b1e79",
            "f4f7db72a9a29660b1607cf891b9033745c300c2e5666b07e71b443df28e2705",
            "e0ca8366d654fbad98db6325042c36c5a20a056c95f1a98c285c412cc601ea95",
        ),
    ),
)

# Original behavior, including the broken return count. These are reference
# observations, not a requirement to reproduce defects in the bounded adapter.
EXPECTED = """default|1.250000|digits=6|separator=.|return=0
setting|1.25|digits=2|separator=.|return=0
separator|-1,250|digits=3|separator=,|return=0
precision-restored|1,2500|digits=2|separator=,|return=0
zero-precision|1|digits=4|separator=,|return=0
general-fractional|12.50|digits=2|separator=.|return=0
general-fixed|125.00|digits=2|separator=.|return=0
general-exponent|1,25E+004|digits=2|separator=,|return=0
exponent|1.25e+002|digits=2|separator=.|return=0
upper-exponent|1.25E+002|digits=2|separator=.|return=0
padded|00000-1,25|digits=2|separator=,|return=0
left|-1,25     |digits=2|separator=,|return=0
zero|0,00|digits=2|separator=,|return=0
negative-zero|0,00|digits=2|separator=,|return=0
precision-cap|1.000000000000000|digits=6|separator=.|return=0
half-away|0.13|digits=2|separator=.|return=0
negative-half-away|-0.13|digits=2|separator=.|return=0
fraction-carry|1.00|digits=2|separator=.|return=0
negative-fraction-zero|-0.00|digits=2|separator=.|return=0
mixed|hi   /   hi/00000-1,25/A/%/101/-123/456|digits=2|separator=,|return=0
"""


# Reviewed defects in the originals, pinned to exact input values and precision.
# Preserve the corrected output, not their appended zero-place fraction or lost
# carry. Signed values, e/E/g and explicit/default precision are checked below.
CORRECTIONS = {
    ("0x1.47ae147ae147bp-8", 0): ("01E-001", "0E-001"),
    ("0x1.47ae147ae147cp-8", 0): ("01E-001", "0E-001"),
    ("0x1.8b0a3d70a3d7p+3", 0): ("12E+001", "1E+001"),
    ("0x1.8b0a3d70a3d71p+3", 0): ("12E+001", "1E+001"),
    ("0x1.8b0a3d70a3d72p+3", 0): ("12E+001", "1E+001"),
    ("0x1.8ffae147ae147p+6", 2): ("0.100E+002", "1.00E+002"),
    ("0x1.f3ffffffffff6p+9", 12): ("0.1000000000000E+003", "1.000000000000E+003"),
    ("0x1.f3ffffffffff7p+9", 12): ("0.1000000000000E+003", "1.000000000000E+003"),
    ("0x1.f3ffffffffff8p+9", 12): ("0.1000000000000E+003", "1.000000000000E+003"),
}


def candidate_expectation(trace, sweep):
    lines, corrected = [], 0
    for line in trace.splitlines():
        fields = line.split("|")
        match = re.fullmatch(r"sweep_(-?[^_]+)_p(\d+)_(.)_m([01])", fields[0])
        if sweep and match:
            value, precision, conversion, _ = match.groups()
            precision = int(precision)
            pair = CORRECTIONS.get((value.lstrip("-"), precision))
            if pair and conversion in ("eEg" if precision == 0 else "eE"):
                before, after = pair
                if conversion == "e":
                    before, after = before.replace("E", "e"), after.replace("E", "e")
                if value.startswith("-"):
                    before, after = "-" + before, "-" + after
                if fields[1] != before:
                    raise AssertionError(f"Historical correction input changed: {line}")
                fields[1] = after
                corrected += 1
        fields[-1] = "return=" + str(len(fields[1]))
        lines.append("|".join(fields))
    if corrected != (92 if sweep else 0):
        raise AssertionError(f"Unexpected corrected-case count: {corrected}")
    return "\n".join(lines) + "\n", corrected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path)
    parser.add_argument("--compiler", default="cc")
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument(
        "--sweep", action="store_true", help="Compare 5,300 cases with explicit defect corrections"
    )
    parser.add_argument(
        "--candidate", action="store_true", help="Also compare the bounded runtime parser"
    )
    parser.add_argument(
        "--candidate-parser", type=Path, help="Alternate parser for negative controls"
    )
    parser.add_argument(
        "--candidate-formatter", type=Path, help="Alternate numeric core for negative controls"
    )
    args = parser.parse_args()
    if (args.candidate_parser or args.candidate_formatter) and not args.candidate:
        parser.error("Candidate source overrides require --candidate")
    root = Path(__file__).resolve().parent
    results = []
    reference_trace = None
    sweep_flags = ["-DHPCLANG_DECIMAL_SWEEP"] if args.sweep else []
    with tempfile.TemporaryDirectory(prefix="hpclang-decimal-reference-") as temporary:
        for name, prefix, hashes in RELEASES:
            work = Path(temporary) / name
            work.mkdir()
            shutil.copyfile(root / "decimal-reference.h", work / "decimal-reference.h")
            for header in ("hpstdio.h", "hpstring.h", "hpstdlib.h", "hpmath.h", "hpgcc49.h"):
                (work / header).write_text('#include "decimal-reference.h"\n')
            path = args.archives / name
            with zipfile.ZipFile(path) if name.endswith(".zip") else tarfile.open(path) as archive:
                for member, digest in zip(FILES, hashes, strict=True):
                    if name.endswith(".zip"):
                        data = archive.read(prefix + member)
                    else:
                        with archive.extractfile(prefix + member) as source:
                            data = source.read()
                    if hashlib.sha256(data).hexdigest() != digest:
                        raise AssertionError(f"Historical source changed: {name}/{member}")
                    (work / Path(member).name).write_bytes(data)
            executable = work / "reference"
            subprocess.run(
                [
                    args.compiler,
                    "-std=gnu11",
                    "-O1",
                    "-g",
                    "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer",
                    *sweep_flags,
                    "-I",
                    str(work),
                    str(root / "decimal-reference.c"),
                    *(str(work / Path(member).name) for member in FILES),
                    "-lm",
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            trace = subprocess.check_output([str(executable)], text=True)
            if not trace.startswith(EXPECTED) or len(trace.splitlines()) != (
                5300 if args.sweep else 20
            ):
                raise AssertionError(f"{name}: decimal reference trace changed:\n{trace}")
            if reference_trace is not None and trace != reference_trace:
                raise AssertionError("Historical release traces differ")
            reference_trace = trace
            results.append(
                {
                    "archive": name,
                    "sources": dict(zip(FILES, hashes, strict=True)),
                    "trace": trace.splitlines(),
                }
            )
            print(f"PASS {name}: {len(trace.splitlines())} reference cases")
        if args.candidate:
            runtime = root.parents[1] / "runtime/core"
            candidate_parser = args.candidate_parser or runtime / "hpgcc-printf.c"
            candidate_formatter = args.candidate_formatter or runtime / "hpgcc-format.c"
            executable = Path(temporary) / "candidate"
            subprocess.run(
                [
                    args.compiler,
                    "-std=gnu11",
                    "-O1",
                    "-g",
                    "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer",
                    "-DHPCLANG_DECIMAL_CANDIDATE",
                    *sweep_flags,
                    "-I",
                    str(runtime),
                    str(root / "decimal-reference.c"),
                    str(candidate_formatter),
                    str(candidate_parser),
                    "-lm",
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            trace = subprocess.check_output([str(executable)], text=True)
            expected, corrected = candidate_expectation(reference_trace, args.sweep)
            if trace != expected:
                differences = [
                    (left, right)
                    for left, right in zip(expected.splitlines(), trace.splitlines())
                    if left != right
                ]
                raise AssertionError(f"Bounded candidate differs: {differences[:10]}")
            results.append(
                {
                    "candidate": "bounded runtime parser",
                    "trace": trace.splitlines(),
                    "parser_sha256": hashlib.sha256(candidate_parser.read_bytes()).hexdigest(),
                    "formatter_sha256": hashlib.sha256(
                        candidate_formatter.read_bytes()
                    ).hexdigest(),
                    "exact_output_cases": len(trace.splitlines()) - corrected,
                    "corrected_historical_outputs": corrected,
                }
            )
            print(
                f"PASS bounded candidate: {len(trace.splitlines()) - corrected} exact outputs, "
                f"{corrected} explicit defect corrections; correct return lengths"
            )
    args.report.write_text(
        json.dumps(
            {
                "scope": "Original formatter control flow with host integer/power/math support; not ARM parity",
                "results": results,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
