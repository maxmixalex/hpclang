#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/atfe-probe}"
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
CC="$ATFE/bin/clang"
CXX="$ATFE/bin/clang++"
READELF="$ATFE/bin/llvm-readelf"

rm -rf "$OUT"
mkdir -p "$OUT"

TARGET=(
    --target=arm-none-eabi
    -march=armv4t
    -mthumb
    -mfpu=none
    -mfloat-abi=soft
    -mabi=aapcs
    -fno-exceptions
    -fno-rtti
)

COMMON=(
    "${TARGET[@]}"
    -ffreestanding
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-pic
    -fno-pie
    -ffunction-sections
    -fdata-sections
    -I"$ROOT/include"
)

echo "== selected ATfE multilib =="
"$CC" "${TARGET[@]}" -print-multi-directory
echo
"$CC" --target=arm-none-eabi -print-multi-lib | grep armv4t || true

"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/core/start.S" -o "$OUT/start.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/core/runtime.c" -o "$OUT/runtime.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/runtime/libc/sbrk.c" -o "$OUT/sbrk.o"
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-runtime-smoke.c" -o "$OUT/smoke.o"

"$CC" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/runtime.o" "$OUT/sbrk.o" "$OUT/smoke.o"     -o "$OUT/hpclang-atfe.elf"

echo
echo "== undefined global symbols =="
if "$READELF" -Ws "$OUT/hpclang-atfe.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined global symbols" >&2
    exit 1
fi
echo "none"

echo
echo "== resolved runtime helpers =="
"$READELF" -Ws "$OUT/hpclang-atfe.elf" |
    grep -E '(__aeabi_|memcpy|strlen|snprintf|malloc|sbrk)' |
    head -80

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hpclang-atfe.elf"     "$OUT/hpclang-atfe.hpc"

echo
echo "PASS: ATfE Armv4T compiler-rt + picolibc link into HPC1."


echo
echo "== libc++ Armv4T smoke =="

"$CXX" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-cxx-smoke.cpp" -o "$OUT/cxx-smoke.o"

"$CXX" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/runtime.o" "$OUT/sbrk.o" "$OUT/cxx-smoke.o"     -o "$OUT/hpclang-atfe-cxx.elf"

if "$READELF" -Ws "$OUT/hpclang-atfe-cxx.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined global C++ symbols" >&2
    exit 1
fi

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hpclang-atfe-cxx.elf"     "$OUT/hpclang-atfe-cxx.hpc"

echo "PASS: ATfE Armv4T libc++ links into HPC1."
