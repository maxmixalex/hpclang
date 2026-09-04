#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "usage: $0 <stage0.hp> <output-dir> [hp39gs|hp40gs]" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INPUT="$(realpath "$1")"
OUT="$(realpath -m "$2")"
TARGET="${3:-hp39gs}"

case "$TARGET" in
    hp39gs)
        TARGET_LABEL="HP39gs"
        MODEL_ID=2
        ;;
    hp40gs)
        TARGET_LABEL="HP40gs"
        MODEL_ID=3
        ;;
    *)
        echo "unsupported HP39/40 hardware-validation target: $TARGET" >&2
        exit 2
        ;;
esac

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

python3 - "$OUT" "$TARGET_LABEL" "$MODEL_ID" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
target = sys.argv[2]
model = sys.argv[3]

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
    f"hpclang {target} canonical runtime validation\n"
    "\n"
    f"Expected hpclang model id: {model}.\n"
    f"Transfer HPGCC000.000 to the real {target} over Conn3x USB.\n"
    "LIB275L.000 only needs to be installed once.\n"
    "\n"
    "Launch HPCLANG HPC1 TEST. The program immediately returns to the stock OS\n"
    "after leaving one of these result lines in the framebuffer:\n"
    "\n"
    "  RUNTIME PASS\n"
    "\n"
    "or:\n"
    "\n"
    "  RUNTIME FAIL\n"
    "  CODE xx\n"
    "\n"
    "After seeing the result, press APLET. A valid return must open the normal\n"
    "APLET library. The stale result pixels are expected until the stock UI\n"
    "redraws the LCD.\n"
    "\n"
    "Coverage: boot info, initialized globals, BSS, .tdata/.tbss TLS, init-array\n"
    "constructor, compiler-rt 32/64-bit division and soft-float, picolibc\n"
    "memcpy/memset/strlen/strcmp, malloc/free and snprintf.\n"
)
PY
