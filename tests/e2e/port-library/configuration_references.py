"""Replay current C reference traversal on configuration_audit.py snapshots.

Requires a Linux C compiler and private native snapshots. This is not an
ordinary CI test or proof of complete reachability, relocation or startup safety.
Partial visitor output must be discarded whenever traversal fails.
"""

from pathlib import Path
import argparse
import ctypes as c
import sys
import json
import subprocess
import hashlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "port-recovery"))
from verify import native_words

src = Path(__file__).resolve().parents[3]
base = src / "runtime/core"
parser = argparse.ArgumentParser(
    description="Check actual C HOME-reference traversal against retained configuration audits."
)
parser.add_argument("--audits", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
out = args.output
out.mkdir(parents=True, exist_ok=True)
files = [
    base / n
    for n in (
        "rpl-reference.c",
        "rpl-backup.c",
        "rpl-port.c",
        "rpl-object-store.c",
        "rpl-object-span.c",
    )
]
subprocess.run(
    [
        "cc",
        "-shared",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Werror",
        *map(str, files),
        "-o",
        str(out / "reference.so"),
    ],
    check=True,
)
Read = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_uint8))


class Reader(c.Structure):
    _fields_ = [("read", Read), ("context", c.c_void_p)]


class Edge(c.Structure):
    _fields_ = [
        ("kind", c.c_int),
        ("field", c.c_uint32),
        ("target", c.c_uint32),
        ("extent", c.c_uint32),
        ("auxiliary", c.c_uint32),
    ]


Visit = c.CFUNCTYPE(c.c_int, c.c_void_p, c.POINTER(Edge))
fn = c.CDLL(str(out / "reference.so")).hpclang_rpl_reference_visit
fn.argtypes = [c.POINTER(Reader), c.c_uint32, c.c_uint32, c.c_uint32, Visit, c.c_void_p]
rows = []
for report in sorted(args.audits.glob("hp*.json")):
    fixture = json.loads(report.read_text())
    p = Path(fixture["after"])
    ram = (p / "sram").read_bytes()
    flash = (p / "flash").read_bytes()
    assert hashlib.sha256(ram).hexdigest() == fixture["after_sram_sha256"]
    assert hashlib.sha256(flash).hexdigest() == fixture["flash_sha256"]
    word = native_words(ram, flash)
    home = word(0x80711)
    count = word(home + 5, 3)
    assert count == fixture["after_home_count"] and 0 < count < 128
    expected = []
    zero = []
    for i in range(count):
        rec = home + 8 + 13 * i
        for offset in (3, 8):
            field = rec + offset
            target = word(field)
            if target:
                expected.append((7, field, target, 0, word(rec, 3)))
            else:
                zero.append(field)
    edges = []
    reads = []
    failure = None

    @Read
    def read(_, at, value):
        if at == failure:
            return 0
        reads.append(at)
        value[0] = word(at, 1)
        return 1

    @Visit
    def visit(_, p):
        e = p.contents
        edges.append((e.kind, e.field, e.target, e.extent, e.auxiliary))
        return 0

    reader = Reader(read, None)
    result = fn(c.byref(reader), home, 0x100000 - home, 0, visit, None)
    assert result == 0, (report.name, result)
    assert [e for e in edges if e[0] == 7] == expected
    assert all(e[0] in (0, 7) for e in edges)
    assert all(field + i in reads for field in zero for i in range(5))
    normal_edges = edges.copy()
    for field in zero:
        for offset in range(5):
            failure = field + offset
            edges.clear()
            # UNAVAILABLE must not be collapsed to a successful absent edge.
            assert fn(c.byref(reader), home, 0x100000 - home, 0, visit, None) == -1
    failure = None
    rows.append(
        dict(
            unavailable_null_nibble_rejections=5 * len(zero),
            fixture=report.name,
            attachment_edges=len(expected),
            null_fields=len(zero),
            sram_sha256=hashlib.sha256(ram).hexdigest(),
            edges=normal_edges,
        )
    )
    print(
        "PASS", report.name, "attachment edges", len(expected), "null fields", len(zero), flush=True
    )
assert rows, "No configuration audit reports found"
(out / "results.json").write_text(
    json.dumps(
        dict(
            cases=rows,
            sources={f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files},
            driver_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            binary_sha256=hashlib.sha256((out / "reference.so").read_bytes()).hexdigest(),
        ),
        indent=2,
    )
    + "\n"
)
