#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/build/e2e}"
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"

rm -rf "$OUT"
mkdir -p "$OUT"

args=(
    -S "$ROOT/tests/e2e"
    -B "$OUT/cmake"
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/hpclang-armv4t.cmake"
    -DHPCLANG_ROOT="$ROOT"
    -DCMAKE_BUILD_TYPE=MinSizeRel
)

if [[ "${HPCLANG_E2E_WAIT_FOR_ENTER:-0}" == "1" ]]; then
    args+=(-DHPCLANG_E2E_WAIT_FOR_ENTER=ON)
fi
if [[ "${HPCLANG_E2E_KEYBOARD:-0}" == "1" ]]; then
    args+=(-DHPCLANG_E2E_KEYBOARD=ON)
fi

PATH="$ATFE/bin:$PATH" cmake "${args[@]}"
PATH="$ATFE/bin:$PATH" cmake --build "$OUT/cmake" -j2

cp "$OUT/cmake/hpclang-e2e.elf" "$OUT/e2e.elf"
python3 "$ROOT/tools/hpclang-pack.py" "$OUT/e2e.elf" "$OUT/e2e.hpc"

result_addr="$("$ATFE/bin/llvm-nm" -n "$OUT/e2e.elf" |
    awk '$3 == "hpclang_e2e_result" {print "0x"$1}')"
test -n "$result_addr"
printf '%s\n' "$result_addr" > "$OUT/e2e.result-address"

"$ATFE/bin/llvm-nm" "$OUT/e2e.elf" | grep -q ' T hpclang_start$'
if "$ATFE/bin/llvm-readelf" -Ws "$OUT/e2e.elf" | grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined E2E symbols" >&2
    exit 1
fi

printf 'PASS: unified hpclang E2E consumer project builds as HPC1.\n'
