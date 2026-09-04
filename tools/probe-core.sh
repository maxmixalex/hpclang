#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/core-probe}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
READELF="${READELF:-llvm-readelf}"
OBJDUMP="${OBJDUMP:-llvm-objdump}"

rm -rf "$OUT"
mkdir -p "$OUT"

COMMON=(
    --target=arm-none-eabi
    -march=armv4t
    -mfloat-abi=soft
    -mabi=aapcs
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-pic
    -fno-pie
    -I"$ROOT/include"
)

"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/core/start.S" -o "$OUT/start.o"

"$CC" "${COMMON[@]}" -mthumb -Os     -c "$ROOT/runtime/core/runtime.c" -o "$OUT/runtime.o"

"$CC" "${COMMON[@]}" -mthumb -Os     -c "$ROOT/tests/core-smoke.c" -o "$OUT/smoke.o"

"$LD" -m armelf     -T "$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/runtime.o" "$OUT/smoke.o"     -o "$OUT/hpclang-v1.elf"

echo "== relocations =="
"$READELF" -r "$OUT/hpclang-v1.elf"

echo
echo "== program headers =="
"$READELF" -l "$OUT/hpclang-v1.elf"

echo
echo "== ARM attributes =="
"$READELF" -A "$OUT/runtime.o"
"$READELF" -A "$OUT/smoke.o"

echo
echo "== startup/interworking =="
"$OBJDUMP" -d "$OUT/hpclang-v1.elf" |
    sed -n '/<_start>/,/^$/p;/<hpclang_start>/,/^$/p;/<main>/,/^$/p'

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hpclang-v1.elf"     "$OUT/hpclang-v1.hpc"

echo
echo "PASS: unified hpclang v1 image built with AAPCS32 + Thumb-1 + LLD."
