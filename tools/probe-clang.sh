#!/usr/bin/env bash
set -euo pipefail

CC="${CC:-clang}"
OBJDUMP="${OBJDUMP:-llvm-objdump}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/probe}"

mkdir -p "$OUT"

COMMON=(
  --target=arm-none-eabi
  -march=armv4t
  -marm
  -ffreestanding
  -fno-builtin
  -fno-stack-protector
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
  -ffunction-sections
  -fdata-sections
)

"$CC" "${COMMON[@]}" -c "$ROOT/tests/smoke.c" -o "$OUT/smoke.o"

echo "compiler:"
"$CC" --version | head -n 1

echo
echo "object:"
file "$OUT/smoke.o"

if command -v "$OBJDUMP" >/dev/null 2>&1; then
  echo
  "$OBJDUMP" -d "$OUT/smoke.o"
fi

echo
echo "PASS: stock Clang emitted an ARMv4T little-endian freestanding object."
echo "NOTE: this is a compiler smoke test only; it is not an HPGCC ABI compatibility test."
