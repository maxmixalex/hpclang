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
    "hpclang HP39 visible HPC1 return smoke\n"
    "\n"
    "REAL HP39GS OVER USB\n"
    "  1. Extract this entire bundle into its own directory.\n"
    "  2. Start the HP 39/40 Connectivity Kit (Conn3x) and connect the HP39gs\n"
    "     through its normal 5-pin mini-USB PC connection.\n"
    "  3. On the calculator open APLET, press RECV, and choose HP39/40 (USB).\n"
    "  4. Transfer LIB275L.000 once if library 275 is not already installed.\n"
    "  5. Repeat RECV -> HP39/40 (USB) and transfer HPGCC000.000.\n"
    "  6. Launch HPCLANG HPC1 TEST.\n"
    "\n"
    "Expected screen:\n"
    "  HPC1 PASS\n"
    "  MODEL 00000002\n"
    "  RW <mapped byte count>\n"
    "  AUTO EXIT\n"
    "\n"
    "Do not press a key on the PASS screen. The smoke should automatically\n"
    "return to the stock calculator OS after a short delay.\n"
    "\n"
    "Please record:\n"
    "  - the exact screen contents;\n"
    "  - whether it returns automatically;\n"
    "  - what screen/state it returns to;\n"
    "  - any reset, freeze, ADDRESS ERROR, corrupted LCD, or lost OS state.\n"
    "\n"
    "EMU48 MANUAL EXECUTION:\n"
    "  If Emu48 serial choices are all disabled, skip the manual emulator gate.\n"
    "  The automated E2E path will use a driverless in-memory serial peer.\n"
)
PY
