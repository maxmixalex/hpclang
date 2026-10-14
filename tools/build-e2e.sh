#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 4 ]]; then
    echo "usage: $0 <runtime|storage|hardware|system-services|hpgcc-system|display|ui|notebook|notebook-backup|notebook-export|sd-media-manual|object-basics|object-mutation|object-composite|object-catalog|font-gallery|font-gallery-stock> [out-dir] [all|hp39|hp48|hp4950] [universal|model]" >&2
    exit 2
fi

MODE="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${2:-$ROOT/build/e2e-$MODE}"
FAMILY="${3:-all}"
BUILD_MODEL="${4:-universal}"
case "$FAMILY" in
    all|hp39|hp48|hp4950) ;;
    *) echo "unsupported E2E family: $FAMILY" >&2; exit 2 ;;
esac
if [[ "$BUILD_MODEL" != universal ]]; then
    target_args=(--target "$BUILD_MODEL")
    if [[ "$FAMILY" != all ]]; then
        target_args+=(--family "$FAMILY")
    fi
    python3 "$ROOT/tools/e2e-target-matrix.py" direct "${target_args[@]}" >/dev/null
fi
ATFE="${ATFE:?set ATFE to the extracted Arm Toolchain for Embedded root}"
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
            "$0" font-gallery "$shard_out" "$FAMILY" "$BUILD_MODEL"

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
    system-services|hpgcc-system|display|ui|notebook|notebook-backup|notebook-export|object-basics|sd-media-manual)
        E2E_SOURCE="$ROOT/tests/e2e/$MODE"
        E2E_TARGET="hpclang_${MODE//-/_}_e2e"
        E2E_ELF="hpclang-$MODE-e2e.elf"
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
    -DHPCLANG_BUILD_MODEL="$BUILD_MODEL"
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

LAUNCHER_ARGS=()
if (( ! WRAP_LAUNCHERS )); then
    LAUNCHER_ARGS+=(--raw-only)
fi
if [[ "$MODE" == runtime ]]; then
    LAUNCHER_ARGS+=(--hp39-armsys-probe)
fi
mapfile -t LAUNCHER_MODELS < <(
    python3 - "$ROOT/tests/e2e/targets" "$FAMILY" "$BUILD_MODEL" <<'PY'
import json
import sys
from pathlib import Path

for path in sorted(Path(sys.argv[1]).glob("*.json")):
    data = json.loads(path.read_text(encoding="utf-8"))
    if (sys.argv[2] == "all" or data["family"] == sys.argv[2]) and (
        sys.argv[3] == "universal" or data["target"] == sys.argv[3]
    ):
        print(data["target"])
PY
)
python3 "$ROOT/tools/build-launchers.py" "$OUT/e2e.hpc" "$OUT/launchers" \
    --tool-bin "$ATFE/bin" --models "${LAUNCHER_MODELS[@]}" "${LAUNCHER_ARGS[@]}"

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
    cp "$ROOT/tests/e2e/object-mutation/stock-setup.json" "$OUT/e2e.stock-setup.json"
fi

if [[ -f "$ROOT/tests/e2e/$MODE/stock-lifecycle.json" ]]; then
    cp "$ROOT/tests/e2e/$MODE/stock-lifecycle.json" "$OUT/e2e.stock-lifecycle.json"
fi
if [[ -f "$ROOT/tests/e2e/$MODE/stock-media.json" ]]; then
    cp "$ROOT/tests/e2e/$MODE/stock-media.json" "$OUT/e2e.stock-media.json"
fi

if (( WRAP_LAUNCHERS )); then
    printf 'PASS: %s E2E application and wrapped target launchers built.\n' "$MODE"
else
    printf 'PASS: %s E2E application and raw Direct target launchers built.\n' "$MODE"
fi
