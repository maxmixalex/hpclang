#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/clean-hp39gs-probe}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
READELF="${READELF:-readelf}"
OBJDUMP="${OBJDUMP:-llvm-objdump}"

rm -rf "$OUT"
mkdir -p "$OUT"

COMMON=(
    --target=arm-none-eabi
    -march=armv4t
    -marm
    -mabi=apcs-gnu
    -mfloat-abi=soft
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

"$CC" "${COMMON[@]}" -c "$ROOT/runtime/hp39gs/loader.S" -o "$OUT/loader.o"
"$CC" "${COMMON[@]}" -O2 -fropi -frwpi     -c "$ROOT/runtime/hp39gs/runtime.c" -o "$OUT/runtime.o"
"$CC" "${COMMON[@]}" -O2 -fropi -frwpi     -c "$ROOT/tests/hp39gs-clean-smoke.c" -o "$OUT/smoke.o"

"$LD" -m armelf     -T "$ROOT/runtime/hp39gs/link.ld"     "$OUT/loader.o" "$OUT/runtime.o" "$OUT/smoke.o"     -o "$OUT/hp39gs-clean.elf"

echo "== final relocations =="
"$READELF" -r "$OUT/hp39gs-clean.elf"
if "$READELF" -r "$OUT/hp39gs-clean.elf" | grep -q 'R_ARM_'; then
    echo "unexpected final relocation" >&2
    exit 1
fi

echo
echo "== program headers =="
"$READELF" -l "$OUT/hp39gs-clean.elf"

echo
echo "== ABI attributes on C objects =="
"$READELF" -A "$OUT/runtime.o"
"$READELF" -A "$OUT/smoke.o"

echo
echo "== dependency audit =="
if "$READELF" -s "$OUT/hp39gs-clean.elf" | grep -E ' UND ' | grep -v ' 0: '; then
    echo "unexpected unresolved symbol" >&2
    exit 1
fi
echo "No unresolved target symbols."
echo "No HPGCC object, libgcc or GNU target tool is used by this probe."

echo
echo "== codegen spot check =="
"$OBJDUMP" -dr "$OUT/hp39gs-clean.elf" |     sed -n '/<_loader>/,/^$/p;/<hpclang_start>/,/^$/p;/<main>/,/^$/p'

echo
echo "PASS: clean hp39gs ARM image links using only Clang + LLD."
echo "Packaging and real-hardware validation are still pending."
