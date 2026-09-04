#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SDK="${HPGCC39_HOME:-}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
READELF="${READELF:-readelf}"
OBJDUMP="${OBJDUMP:-llvm-objdump}"

if [[ -z "$SDK" ]]; then
  echo "HPGCC39_HOME must point to the normalized HPGCC 1.1-p1 39G SDK." >&2
  exit 2
fi

for f in "$SDK/lib/MMUloader.o" "$SDK/lib/MMUglobals.o"; do
  [[ -f "$f" ]] || { echo "missing: $f" >&2; exit 2; }
done

OUT="${OUT:-$ROOT/build}"
rm -rf "$OUT"
mkdir -p "$OUT"

COMMON=(
  --target=arm-none-eabi
  -march=armv4t
  -marm
  -mabi=apcs-gnu
  -mfloat-abi=soft
)

"$CC" "${COMMON[@]}" -c "$ROOT/start.S" -o "$OUT/start.o"
"$CC" "${COMMON[@]}" -O2 -frwpi   -ffreestanding -fno-builtin -fno-stack-protector   -fno-unwind-tables -fno-asynchronous-unwind-tables   -c "$ROOT/main.c" -o "$OUT/main.o"

"$LD" -m armelf -T "$ROOT/hybrid.ld"   "$SDK/lib/MMUloader.o" "$SDK/lib/MMUglobals.o"   "$OUT/start.o" "$OUT/main.o"   -o "$OUT/hybrid.elf"

echo "== unresolved symbols =="
if "$READELF" -s "$OUT/hybrid.elf" | grep -E ' UND ' | grep -v ' 0: '; then
  echo "unexpected unresolved symbols" >&2
  exit 1
fi
echo "none"

echo
echo "== relevant relocations in input objects =="
"$READELF" -r "$SDK/lib/MMUglobals.o" | grep -E 'GOT_BREL|GOTOFF' || true
"$READELF" -r "$OUT/main.o" | grep -E 'SBREL' || true

echo
echo "== load segments =="
"$READELF" -l "$OUT/hybrid.elf"

echo
echo "== bridge and modern entry =="
"$OBJDUMP" -dr "$OUT/hybrid.elf" | sed -n '/<_start>/,/^$/p;/<hpclang_start>/,/^$/p'

# Structural assertions from the host-verified experiment.
"$OBJDUMP" -dr "$OUT/hybrid.elf" | grep -q 'mov.*r9, r10'
"$OBJDUMP" -dr "$OUT/hybrid.elf" | grep -q '\[r9, r'
"$OBJDUMP" -dr "$OUT/hybrid.elf" | grep -q '\[r10, r'

echo
echo "PASS: old r10/GOT and new r9/RWPI code coexist in one linked image."
echo "Hardware validation is still required before promoting this profile."
