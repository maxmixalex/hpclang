#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <stage0.hp> <output-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INPUT="$(realpath "$1")"
OUT="$(realpath -m "$2")"
SDK="${HP39_APLET_HOME:-$ROOT/.cache/hp39-aplet/sdk}"
BUILD="$ROOT/build/hp39-aplet"

rm -rf "$BUILD" "$OUT"
mkdir -p "$BUILD" "$OUT"

cc -O2 "$SDK/src/hp2aplet.c" -o "$BUILD/hp2aplet"

cat > "$BUILD/hp39prj.dat" <<'EOF'
HP39 Aplet Project
HPCLANG HPC1 TEST
hpclang v1 visible runtime and bootstrap smoke
N
EOF

(
    cd "$BUILD"
    HPGCC="$SDK/data" ./hp2aplet "$INPUT"
)

# Preserve the stock HPGCC39 transfer names. Conn3x/HPGComm and real HP39gs
# hardware have decades of evidence for these names; semantic renaming adds no
# value at the calculator boundary.
mv "$BUILD/HPGCC000.000" "$OUT/HPGCC000.000"
cp "$SDK/data/LIB275L.L39" "$OUT/LIB275L.000"

python3 - "$OUT" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])

def entry(kind: str, name: str, title: str) -> str:
    return f"{kind} {len(name)} {name}{len(title)} {title}"

directory = (
    "HP39AscA "
    + entry("B", "HPGCC000.000", "HPCLANG HPC1 TEST")
    + " "
    + entry("H", "LIB275L.000", "LIB275L")
    + " "
)

(root / "HP39DIR.000").write_bytes(directory.encode("ascii"))
(root / "HP39DIR.CUR").write_bytes(b"HP39AscA A 11 HP39DIR.0004 Root")

(root / "TESTING.txt").write_text(
    "hpclang HP39 immediate-return smoke\n"
    "\n"
    "Transfer HPGCC000.000 to the real HP39gs over the normal Conn3x USB path.\n"
    "LIB275L.000 only needs to be installed once.\n"
    "\n"
    "Launch HPCLANG HPC1 TEST.\n"
    "\n"
    "PASS: it immediately returns to the stock APLET library and the calculator\n"
    "      remains fully responsive.\n"
    "FAIL: it freezes, resets, shows ADDRESS ERROR, corrupts the LCD, or returns\n"
    "      with persistent OS-state corruption.\n"
    "\n"
    "This build intentionally has no diagnostic page, no delay, no keyboard/GPIO\n"
    "polling and no IRQ/CPSR manipulation. Entry into HPC1 was already confirmed\n"
    "on real hardware with MODEL 00000002 and RW 00014000.\n"
)
PY
