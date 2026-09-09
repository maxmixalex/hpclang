#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/e2e}"
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
CC="$ATFE/bin/clang"
LD="$ATFE/bin/ld.lld"
NM="$ATFE/bin/llvm-nm"

rm -rf "$OUT"
mkdir -p "$OUT"

cmake_args=(
    -S "$ROOT/tests/e2e"
    -B "$OUT/cmake"
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/hpclang-armv4t.cmake"
    -DHPCLANG_ROOT="$ROOT"
    -DCMAKE_BUILD_TYPE=MinSizeRel
)
if [[ "${HPCLANG_E2E_WAIT_FOR_ENTER:-0}" == "1" ]]; then
    cmake_args+=(-DHPCLANG_E2E_WAIT_FOR_ENTER=ON)
fi
if [[ "${HPCLANG_E2E_KEYBOARD:-0}" == "1" ]]; then
    cmake_args+=(-DHPCLANG_E2E_KEYBOARD=ON)
fi

PATH="$ATFE/bin:$PATH" cmake "${cmake_args[@]}"
PATH="$ATFE/bin:$PATH" cmake --build "$OUT/cmake" -j2

cp "$OUT/cmake/hpclang-e2e.elf" "$OUT/e2e.elf"
python3 "$ROOT/tools/hpclang-pack.py" "$OUT/e2e.elf" "$OUT/e2e.hpc"

result_addr="$("$NM" -n "$OUT/e2e.elf" |
    awk '$3 == "hpclang_e2e_result" {print "0x"$1}')"
test -n "$result_addr"
printf '%s\n' "$result_addr" > "$OUT/e2e.result-address"

"$NM" "$OUT/e2e.elf" | grep -q ' T hpclang_start$'
if "$ATFE/bin/llvm-readelf" -Ws "$OUT/e2e.elf" |
    grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined E2E symbols" >&2
    exit 1
fi

TARGET=(
    --target=arm-none-eabi
    -march=armv4t
    -mfpu=none
    -mfloat-abi=soft
    -mabi=aapcs
)
COMMON=(
    "${TARGET[@]}"
    -marm
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-pic
    -fno-pie
    -ffunction-sections
    -fdata-sections
    -I"$ROOT/include"
)

"$CC" "${COMMON[@]}" -Os -c "$ROOT/runtime/bootstrap/mmu.c" -o "$OUT/bootstrap-mmu.o"
"$CC" "${COMMON[@]}" -Os -c "$ROOT/runtime/bootstrap/run.c" -o "$OUT/bootstrap-run.o"
"$CC" "${COMMON[@]}" -c "$ROOT/runtime/bootstrap/sync.S" -o "$OUT/bootstrap-sync.o"
"$CC" "${COMMON[@]}" -c "$ROOT/runtime/bootstrap/enter.S" -o "$OUT/bootstrap-enter.o"

"$LD" -m armelf -r     "$OUT/bootstrap-mmu.o" "$OUT/bootstrap-run.o"     "$OUT/bootstrap-sync.o" "$OUT/bootstrap-enter.o"     -o "$OUT/bootstrap.o"
if "$NM" -u "$OUT/bootstrap.o" | grep .; then
    echo "unexpected undefined bootstrap symbols" >&2
    exit 1
fi

"$CC" "${COMMON[@]}" -Os -c "$ROOT/platform/armtoolbox/stage1.c" -o "$OUT/stage1.o"
"$CC" "${COMMON[@]}" -c "$ROOT/platform/armtoolbox/stage1-entry.S" -o "$OUT/stage1-entry.o"
"$LD" -m armelf -T "$ROOT/platform/armtoolbox/stage1.ld"     "$OUT/stage1-entry.o" "$OUT/stage1.o"     "$OUT/bootstrap-mmu.o" "$OUT/bootstrap-run.o"     "$OUT/bootstrap-sync.o" "$OUT/bootstrap-enter.o"     -o "$OUT/stage1.elf"
if "$NM" -u "$OUT/stage1.elf" | grep .; then
    echo "unexpected undefined stage1 symbols" >&2
    exit 1
fi
"$ATFE/bin/llvm-objcopy" -O binary "$OUT/stage1.elf" "$OUT/stage1.bin"

cat > "$OUT/stage0-bundle.S" <<EOF
.section .rodata.hpclang.bundle,"a",%progbits
.align 2
.global __stage1_blob_start
.global __stage1_blob_end
.global __stage1_blob_size
.global __hpc1_blob_start
.global __hpc1_blob_end
__stage1_blob_start:
.incbin "$OUT/stage1.bin"
__stage1_blob_end:
.set __stage1_blob_size, __stage1_blob_end - __stage1_blob_start
.align 2
__hpc1_blob_start:
.incbin "$OUT/e2e.hpc"
__hpc1_blob_end:
EOF
"$CC" "${COMMON[@]}" -c "$OUT/stage0-bundle.S" -o "$OUT/stage0-bundle.o"

printf 'PASS: unified hpclang E2E project and launcher bundle build.\n'
