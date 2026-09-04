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
"$CC" "${COMMON[@]}" -Os     -c "$ROOT/tests/atfe-platform-stubs.c" -o "$OUT/platform-stubs.o"

"$CXX" "${TARGET[@]}"     -nostartfiles     -Wl,--gc-sections     -Wl,-T,"$ROOT/runtime/core/link.ld"     "$OUT/start.o" "$OUT/runtime.o" "$OUT/sbrk.o" "$OUT/platform-stubs.o" "$OUT/cxx-smoke.o"     -o "$OUT/hpclang-atfe-cxx.elf"

if "$READELF" -Ws "$OUT/hpclang-atfe-cxx.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined global C++ symbols" >&2
    exit 1
fi

python3 "$ROOT/tools/hpclang-pack.py"     "$OUT/hpclang-atfe-cxx.elf"     "$OUT/hpclang-atfe-cxx.hpc"

echo "PASS: ATfE Armv4T libc++ links into HPC1."

echo
echo "== common bootstrap ARMv4T link =="

"$CC" "${COMMON[@]}" -marm -Os     -c "$ROOT/runtime/bootstrap/mmu.c" -o "$OUT/bootstrap-mmu.o"
"$CC" "${COMMON[@]}" -marm -Os     -c "$ROOT/runtime/bootstrap/run.c" -o "$OUT/bootstrap-run.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/bootstrap/sync.S" -o "$OUT/bootstrap-sync.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/runtime/bootstrap/enter.S" -o "$OUT/bootstrap-enter.o"

"$ATFE/bin/ld.lld" -m armelf -r     "$OUT/bootstrap-mmu.o"     "$OUT/bootstrap-run.o"     "$OUT/bootstrap-sync.o"     "$OUT/bootstrap-enter.o"     -o "$OUT/bootstrap.o"

if "$ATFE/bin/llvm-nm" -u "$OUT/bootstrap.o" | grep .; then
    echo "unexpected undefined bootstrap symbols" >&2
    exit 1
fi

echo "PASS: common ARMv4T bootstrap links without platform/runtime dependencies."

echo
echo "== stock ARMToolbox fixed stage1 =="

"$CC" "${COMMON[@]}" -marm -Os     -c "$ROOT/platform/armtoolbox/stage1.c" -o "$OUT/stage1.o"
"$CC" "${COMMON[@]}" -marm     -c "$ROOT/platform/armtoolbox/stage1-entry.S" -o "$OUT/stage1-entry.o"

"$ATFE/bin/ld.lld" -m armelf     -T "$ROOT/platform/armtoolbox/stage1.ld"     "$OUT/stage1-entry.o"     "$OUT/stage1.o"     "$OUT/bootstrap-mmu.o"     "$OUT/bootstrap-run.o"     "$OUT/bootstrap-sync.o"     "$OUT/bootstrap-enter.o"     -o "$OUT/armtoolbox-stage1.elf"

if "$ATFE/bin/llvm-nm" -u "$OUT/armtoolbox-stage1.elf" | grep .; then
    echo "unexpected undefined stage1 symbols" >&2
    exit 1
fi

"$ATFE/bin/llvm-objcopy" -O binary     "$OUT/armtoolbox-stage1.elf"     "$OUT/armtoolbox-stage1.bin"

"$READELF" -h "$OUT/armtoolbox-stage1.elf" |
    grep -E 'Entry point address|Machine'
"$READELF" -l "$OUT/armtoolbox-stage1.elf" |
    grep -E 'LOAD|Entry point'

echo "PASS: stock ARMToolbox stage1 is fixed-address, non-PIC and self-contained."
