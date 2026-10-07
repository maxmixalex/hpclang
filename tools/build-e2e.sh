#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
    echo "usage: $0 <runtime|storage|hardware|system-services|object-mutation|object-composite|object-catalog|font-gallery|font-gallery-stock> [out-dir] [all|hp39|hp48|hp4950]" >&2
    exit 2
fi

MODE="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${2:-$ROOT/build/e2e-$MODE}"
FAMILY="${3:-all}"
case "$FAMILY" in
    all|hp39|hp48|hp4950) ;;
    *) echo "unsupported E2E family: $FAMILY" >&2; exit 2 ;;
esac
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
CC="$ATFE/bin/clang"
LD="$ATFE/bin/ld.lld"
NM="$ATFE/bin/llvm-nm"

# The exhaustive Gallery exceeds the v1 code window when linked monolithically.
# If no explicit shard index is supplied, build a deterministic bundle of
# shards and collect their raw target launchers into the ordinary artifact.
# An explicit HPCLANG_GALLERY_SHARD_INDEX selects the inner single-shard path.
if [[ "$MODE" == font-gallery && -z "${HPCLANG_GALLERY_SHARD_INDEX+x}" ]]; then
    GALLERY_BUNDLE_COUNT="${HPCLANG_GALLERY_SHARD_COUNT:-20}"
    if [[ ! "$GALLERY_BUNDLE_COUNT" =~ ^[1-9][0-9]*$ ]]; then
        echo "HPCLANG_GALLERY_SHARD_COUNT must be a positive integer" >&2
        exit 2
    fi

    rm -rf "$OUT"
    mkdir -p "$OUT/launchers"
    representative_size=-1
    expected_result=""

    for (( shard_index = 0; shard_index < GALLERY_BUNDLE_COUNT; ++shard_index )); do
        printf -v shard_tag '%02d' "$shard_index"
        shard_out="$OUT/.shard-$shard_tag"
        echo "=== Direct Gallery shard $shard_index/$GALLERY_BUNDLE_COUNT ==="
        HPCLANG_GALLERY_SHARD_INDEX="$shard_index" \
        HPCLANG_GALLERY_SHARD_COUNT="$GALLERY_BUNDLE_COUNT" \
            "$0" font-gallery "$shard_out" "$FAMILY"

        shard_result="$(cat "$shard_out/e2e.result-address")"
        if [[ -z "$expected_result" ]]; then
            expected_result="$shard_result"
        elif [[ "$shard_result" != "$expected_result" ]]; then
            echo "Gallery shard result address changed: $shard_result != $expected_result" >&2
            exit 1
        fi

        for launcher in "$shard_out"/launchers/*.bin; do
            base="$(basename "$launcher" .bin)"
            cp "$launcher" "$OUT/launchers/$base-shard-$shard_tag.bin"
        done

        shard_size="$(stat -c '%s' "$shard_out/e2e.hpc")"
        if (( shard_size > representative_size )); then
            representative_size="$shard_size"
            cp "$shard_out/e2e.elf" "$OUT/e2e.elf"
            cp "$shard_out/e2e.hpc" "$OUT/e2e.hpc"
            cp "$shard_out/e2e.result-address" "$OUT/e2e.result-address"
        fi
        printf '%s %s %s\n' "$shard_tag" "$shard_size" "$shard_result" \
            >> "$OUT/e2e.gallery-shards.txt"
        rm -rf "$shard_out"
    done

    printf 'PASS: font-gallery bundle built with %s shards; largest HPC1 container %s bytes.\n' \
        "$GALLERY_BUNDLE_COUNT" "$representative_size"
    exit 0
fi

E2E_SOURCE="$ROOT/tests/e2e"
E2E_TARGET="hpclang_e2e"
E2E_ELF="hpclang-e2e.elf"
WRAP_LAUNCHERS=1

case "$MODE" in
    runtime)
        E2E_CMAKE_ARGS=(
            -DHPCLANG_E2E_KEYBOARD=ON
        )
        ;;
    storage)
        E2E_CMAKE_ARGS=(
            -DHPCLANG_E2E_STORAGE=ON
        )
        ;;
    hardware)
        E2E_CMAKE_ARGS=()
        ;;
    system-services)
        E2E_SOURCE="$ROOT/tests/e2e/system-services"
        E2E_TARGET="hpclang_system_services_e2e"
        E2E_ELF="hpclang-system-services-e2e.elf"
        E2E_CMAKE_ARGS=()
        ;;
    object-mutation|object-composite|object-catalog)
        E2E_SOURCE="$ROOT/tests/e2e/$MODE"
        E2E_TARGET="hpclang_${MODE//-/_}_e2e"
        E2E_ELF="hpclang-$MODE-e2e.elf"
        E2E_CMAKE_ARGS=(-DHPCLANG_E2E_FAMILY="$FAMILY")
        ;;
    font-gallery)
        E2E_SOURCE="$ROOT/tests/e2e/font-gallery"
        E2E_TARGET="hpclang_font_gallery"
        E2E_ELF="hpclang-font-gallery.elf"
        GALLERY_SHARD_INDEX="${HPCLANG_GALLERY_SHARD_INDEX:-0}"
        GALLERY_SHARD_COUNT="${HPCLANG_GALLERY_SHARD_COUNT:-1}"
        E2E_CMAKE_ARGS=(
            -DHPCLANG_GALLERY_PROFILE=full
            -DHPCLANG_GALLERY_KEY_HANDSHAKE=ON
            -DHPCLANG_GALLERY_SHARD_INDEX="$GALLERY_SHARD_INDEX"
            -DHPCLANG_GALLERY_SHARD_COUNT="$GALLERY_SHARD_COUNT"
        )
        WRAP_LAUNCHERS=0
        ;;
    font-gallery-stock)
        E2E_SOURCE="$ROOT/tests/e2e/font-gallery"
        E2E_TARGET="hpclang_font_gallery"
        E2E_ELF="hpclang-font-gallery.elf"
        E2E_CMAKE_ARGS=(-DHPCLANG_GALLERY_PROFILE=stock-smoke)
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
"$CC" "${COMMON[@]}" -c "$ROOT/platform/armtoolbox/hp39-portal.S" -o "$OUT/stage1-hp39-portal.o"
"$CC" -E -P -x c "$ROOT/platform/armtoolbox/stage1.ld" -o "$OUT/stage1.ld"
"$LD" -m armelf -T "$OUT/stage1.ld" \
    "$OUT/stage1-entry.o" "$OUT/stage1.o" "$OUT/stage1-hp39-portal.o" \
    "$OUT/bootstrap-mmu.o" "$OUT/bootstrap-run.o" \
    "$OUT/bootstrap-sync.o" "$OUT/bootstrap-enter.o" \
    -o "$OUT/stage1.elf"
if "$NM" -u "$OUT/stage1.elf" | grep .; then
    echo "unexpected undefined stage1 symbols" >&2
    exit 1
fi
"$ATFE/bin/llvm-objcopy" -O binary "$OUT/stage1.elf" "$OUT/stage1.bin"

stage1_memory_size="$("$NM" "$OUT/stage1.elf" |
    awk '$3 == "__stage1_size" {print "0x"$1}')"
[[ "$stage1_memory_size" =~ ^0x[0-9a-fA-F]+$ ]]
if (( $(stat -c '%s' "$OUT/stage1.bin") > stage1_memory_size )); then
    echo "stage1 file exceeds its reserved memory extent" >&2
    exit 1
fi

cat > "$OUT/stage0-bundle.S" <<EOF
.section .rodata.hpclang.bundle,"a",%progbits
.align 2
.global __stage1_blob_start
.global __stage1_blob_end
.global __stage1_blob_size
.global __stage1_memory_size
.set __stage1_memory_size, $stage1_memory_size
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
    stage0_extra_defs=()
    if [[ "$MODE" == runtime &&
          ( "$target" == hp39gplus || "$target" == hp39gs || "$target" == hp40gs ) ]]; then
        # Runtime Stock E2E alone proves the HP39 ARMSYS reverse edge. Direct
        # launchers share this binary but skip the probe because their
        # synthetic ARMSYS block deliberately has no live Saturn Read[] map.
        stage0_extra_defs=(-DHPCLANG_STAGE0_ARMSYS_PROBE=1)
    fi

    "$CC" "${COMMON[@]}" \
        -DHPCLANG_STAGE0_MODEL="$model" \
        "${stage0_extra_defs[@]}" \
        -c "$ROOT/platform/armtoolbox/stage0.S" \
        -o "$OUT/launchers/$target.o"

    "$LD" -m armelf -T "$ROOT/platform/armtoolbox/stage0.ld" \
        "$OUT/launchers/$target.o" \
        "$OUT/stage0-bundle.o" \
        -o "$OUT/launchers/$target.elf"

    "$ATFE/bin/llvm-objcopy" -O binary \
        "$OUT/launchers/$target.elf" \
        "$OUT/launchers/$target.bin"

    if (( WRAP_LAUNCHERS )); then
        python3 "$ROOT/tools/armtoolbox-wrap.py" \
            "$OUT/launchers/$target.bin" \
            "$OUT/launchers/$target.hp"
    fi
done < <(
    python3 - "$ROOT/tests/e2e/targets" "$FAMILY" <<'PY'
import json
import sys
from pathlib import Path

for path in sorted(Path(sys.argv[1]).glob("*.json")):
    data = json.loads(path.read_text(encoding="utf-8"))
    if sys.argv[2] == "all" or data["family"] == sys.argv[2]:
        print(data["target"], data["model"])
PY
)

if [[ "$MODE" == font-gallery ]]; then
    # The Gallery frontend provides a synthetic injection mapping of 224
    # pages. The frontend reserves three of them around the stage0 image.
    gallery_launcher_limit=$(( (224 - 3) * 4096 ))
    for launcher in "$OUT"/launchers/*.bin; do
        launcher_size="$(stat -c '%s' "$launcher")"
        if (( launcher_size > gallery_launcher_limit )); then
            echo "Direct Gallery launcher $(basename "$launcher") is ${launcher_size} bytes; limit is ${gallery_launcher_limit}" >&2
            exit 1
        fi
    done
    printf 'index=%s\ncount=%s\n' \
        "$GALLERY_SHARD_INDEX" "$GALLERY_SHARD_COUNT" \
        > "$OUT/e2e.gallery-shard"
fi

if [[ "$MODE" == object-mutation ]]; then
    python3 "$ROOT/tests/e2e/object-mutation/fixtures.py" "$OUT/stock-fixtures"
fi

if [[ ( "$FAMILY" == all || "$FAMILY" == hp39 ) &&
      -f "$ROOT/tests/e2e/$MODE/stock-lifecycle.json" ]]; then
    cp "$ROOT/tests/e2e/$MODE/stock-lifecycle.json" "$OUT/e2e.stock-lifecycle.json"
fi

if (( WRAP_LAUNCHERS )); then
    printf 'PASS: %s E2E application and wrapped target launchers built.\n' "$MODE"
else
    printf 'PASS: %s E2E application and raw Direct target launchers built.\n' "$MODE"
fi
