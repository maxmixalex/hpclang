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

mv "$BUILD/HPGCC000.000" "$OUT/hpclang-hpc1.apt"
cp "$SDK/data/LIB275L.L39" "$OUT/lib275.sys"

python3 - "$OUT" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])

def entry(kind: str, name: str, title: str) -> str:
    return f"{kind} {len(name)} {name}{len(title)} {title}"

directory = (
    "HP39AscA "
    + entry("B", "hpclang-hpc1.apt", "HPCLANG HPC1 TEST")
    + " "
    + entry("H", "lib275.sys", "LIB275L")
    + " "
)

(root / "HP39DIR.000").write_bytes(directory.encode("ascii"))
(root / "HP39DIR.CUR").write_bytes(b"HP39AscA A 11 HP39DIR.0004 Root")

(root / "TESTING.txt").write_text(
    "hpclang HP39 visible HPC1 smoke\n"
    "\n"
    "IMPORTANT: Emu48 HP39/40 cannot directly load an Aplet/object file.\n"
    "Use the emulated serial port, just like a real HP39-series calculator.\n"
    "\n"
    "Recommended Windows setup:\n"
    "  1. Extract this entire bundle into its own directory.\n"
    "  2. Create a virtual null-modem pair with com0com, for example COM11 <-> COM12.\n"
    "  3. In Emu48 configure the calculator wire/serial port as COM11.\n"
    "  4. Start HPGComm 2.0 (HP 38/39/40 connectivity kit), select COM12,\n"
    "     and browse to this extracted directory.\n"
    "  5. In the emulated HP39gs APLET library choose RECV, then the serial\n"
    "     disk-drive/HP39-40 serial receive path, and receive HPCLANG HPC1 TEST.\n"
    "  6. Launch HPCLANG HPC1 TEST.\n"
    "\n"
    "Expected screen:\n"
    "  HPC1 PASS\n"
    "  MODEL 00000002\n"
    "  RW <mapped byte count>\n"
    "  APLET TO EXIT\n"
    "\n"
    "Press APLET once to return to the calculator OS.\n"
    "\n"
    "Please record:\n"
    "  - whether the Aplet appears and launches;\n"
    "  - the exact screen contents (or a screenshot);\n"
    "  - whether APLET returns cleanly to the OS;\n"
    "  - any reset, freeze, ADDRESS ERROR, or unexpected display.\n"
)
PY
