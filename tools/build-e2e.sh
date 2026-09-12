#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <runtime|hardware|font-gallery> [out-dir]" >&2
    exit 2
fi

MODE="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${2:-$ROOT/build/e2e-$MODE}"
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
CC="$ATFE/bin/clang"
LD="$ATFE/bin/ld.lld"
NM="$ATFE/bin/llvm-nm"

E2E_SOURCE="$ROOT/tests/e2e"
E2E_TARGET="hpclang_e2e"
E2E_ELF="hpclang-e2e.elf"

case "$MODE" in
    runtime)
        E2E_CMAKE_ARGS=(
            -DHPCLANG_E2E_KEYBOARD=ON
            -DHPCLANG_E2E_STORAGE=ON
        )
        ;;
    hardware)
        E2E_CMAKE_ARGS=()
        ;;
    font-gallery)
        E2E_SOURCE="$ROOT/tests/e2e/font-gallery"
        E2E_TARGET="hpclang_font_gallery"
        E2E_ELF="hpclang-font-gallery.elf"
        E2E_CMAKE_ARGS=()
        ;;
    *)
        echo "unsupported E2E mode: $MODE" >&2
        exit 2
        ;;
esac

rm -rf "$OUT"
mkdir -p "$OUT/launchers"

cmake_args=(
    -S "$E2E_SOURCE"
    -B "$OUT/cmake"
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/hpclang-armv4t.cmake"
    -DHPCLANG_ROOT="$ROOT"
    -DCMAKE_BUILD_TYPE=MinSizeRel
    "${E2E_CMAKE_ARGS[@]}"
)

PATH="$ATFE/bin:$PATH" cmake "${cmake_args[@]}"
PATH="$ATFE/bin:$PATH" cmake --build "$OUT/cmake" --target "$E2E_TARGET" -j2

cp "$OUT/cmake/$E2E_ELF" "$OUT/e2e.elf"
python3 "$ROOT/tools/hpclang-pack.py" "$OUT/e2e.elf" "$OUT/e2e.hpc"

result_addr="$("$NM" -n "$OUT/e2e.elf" |
    awk '$3 == "hpclang_e2e_result" {print "0x"$1}')"
test -n "$result_addr"
printf '%s\n' "$result_addr" > "$OUT/e2e.result-address"

"$NM" "$OUT/e2e.elf" |
    awk '$2 == "T" && $3 == "hpclang_start" { found = 1 } END { exit !found }'
if "$ATFE/bin/llvm-readelf" -Ws "$OUT/e2e.elf" |
    grep -E ' GLOBAL .* UND '; then
    echo "unexpected undefined E2E symbols" >&2
    exit 1
fi

TARGET_FLAGS=(
    --target=arm-none-eabi
    -march=armv4t
    -mfpu=none
    -mfloat-abi=soft
    -mabi=aapcs
)
COMMON=(
    "${TARGET_FLAGS[@]}"
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

"$LD" -m armelf -r \
    "$OUT/bootstrap-mmu.o" "$OUT/bootstrap-run.o" \
    "$OUT/bootstrap-sync.o" "$OUT/bootstrap-enter.o" \
    -o "$OUT/bootstrap.o"
if "$NM" -u "$OUT/bootstrap.o" | grep .; then
    echo "unexpected undefined bootstrap symbols" >&2
    exit 1
fi

"$CC" "${COMMON[@]}" -Os -c "$ROOT/platform/armtoolbox/stage1.c" -o "$OUT/stage1.o"
"$CC" "${COMMON[@]}" -c "$ROOT/platform/armtoolbox/stage1-entry.S" -o "$OUT/stage1-entry.o"
"$LD" -m armelf -T "$ROOT/platform/armtoolbox/stage1.ld" \
    "$OUT/stage1-entry.o" "$OUT/stage1.o" \
    "$OUT/bootstrap-mmu.o" "$OUT/bootstrap-run.o" \
    "$OUT/bootstrap-sync.o" "$OUT/bootstrap-enter.o" \
    -o "$OUT/stage1.elf"
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

while read -r target model; do
    "$CC" "${COMMON[@]}" \
        -DHPCLANG_STAGE0_MODEL="$model" \
        -c "$ROOT/platform/armtoolbox/stage0.S" \
        -o "$OUT/launchers/$target.o"

    "$LD" -m armelf -T "$ROOT/platform/armtoolbox/stage0.ld" \
        "$OUT/launchers/$target.o" \
        "$OUT/stage0-bundle.o" \
        -o "$OUT/launchers/$target.elf"

    "$ATFE/bin/llvm-objcopy" -O binary \
        "$OUT/launchers/$target.elf" \
        "$OUT/launchers/$target.bin"

    python3 "$ROOT/tools/armtoolbox-wrap.py" \
        "$OUT/launchers/$target.bin" \
        "$OUT/launchers/$target.hp"
done < <(
    python3 - "$ROOT/tests/e2e/targets" <<'PY'
import json
import sys
from pathlib import Path

for path in sorted(Path(sys.argv[1]).glob("*.json")):
    data = json.loads(path.read_text(encoding="utf-8"))
    print(data["target"], data["model"])
PY
)

printf 'PASS: %s E2E application and all target launchers built.\n' "$MODE"
