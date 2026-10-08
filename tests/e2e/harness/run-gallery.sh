#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <target> <gallery-build-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_DIR="$(realpath "$2")"
export TARGET

case "$TARGET" in
    hp39gplus|hp39gs|hp40gs) NATIVE_FACE_COUNT=3 ;;
    *) NATIVE_FACE_COUNT=2 ;;
esac

while IFS= read -r line; do
    env_name="${line%%=*}"
    env_value="${line#*=}"
    printf -v "$env_name" '%s' "$env_value"
    export "${env_name?}"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

export HPCLANG_X50NG_HARNESS=gallery
if [[ "$HPCLANG_E2E_FAMILY" == hp48 ]]; then
    export HPCLANG_X50NG_SRAM_SIZE=0x00040000
else
    export HPCLANG_X50NG_SRAM_SIZE=0x00080000
fi
unset HPCLANG_X50NG_UART0 || true

X50="$ROOT/build/x50ng/dist/x50ng"
STATE="$ROOT/build/$TARGET-state"
FRAMES="$ROOT/build/$TARGET-font-gallery-frames"
rm -rf "$STATE" "$FRAMES"
rm -f \
    "$ROOT/build/$TARGET-before.pgm" \
    "$ROOT/build/$TARGET-before.pgm.png" \
    "$ROOT/build/$TARGET-pass.pgm" \
    "$ROOT/build/$TARGET-pass.pgm.png" \
    "$ROOT/build/$TARGET-return.pgm" \
    "$ROOT/build/$TARGET-return.pgm.png" \
    "$ROOT/build/$TARGET-e2e.json" \
    "$ROOT/build/$TARGET-font-gallery.pgm" \
    "$ROOT/build/$TARGET-font-gallery.pgm.png"
mkdir -p "$STATE" "$FRAMES"

X50_ARGS=(
    --datadir="$STATE"
    --overwrite-config
)

case "$HPCLANG_E2E_FAMILY" in
    hp39)
        X50_ARGS+=(--style="$ROOT/build/x50ng/dist/style-40g.css")
        if [[ -n "${HPCLANG_E2E_NOR_PATH:-}" ]]; then
            python3 "$ROOT/tools/prepare-nor-fixture.py" \
                "$ROOT/$HPCLANG_E2E_NOR_PATH" "$STATE" \
                --compression "$HPCLANG_E2E_NOR_COMPRESSION" \
                --bytes "$HPCLANG_E2E_NOR_BYTES" \
                --state-bytes "${HPCLANG_E2E_NOR_STATE_BYTES:-$HPCLANG_E2E_NOR_BYTES}" \
                --sha256 "$HPCLANG_E2E_NOR_SHA256"
        else
            archive="$ROOT/build/$TARGET-$HPCLANG_E2E_ARCHIVE"
            if [[ ! -s "$archive" ]]; then
                curl -fL --retry 3 \
                    "https://www.hpcalc.org/hp39/pc/$HPCLANG_E2E_ARCHIVE" \
                    -o "$archive.tmp"
                mv "$archive.tmp" "$archive"
            fi
            unzip -p "$archive" "$HPCLANG_E2E_MEMBER" \
                > "$ROOT/build/$TARGET-firmware.bin"
            X50_ARGS+=(
                --flash-full
                --bootloader="$ROOT/build/x50ng/dist/firmware/boot-50g.bin"
                --firmware="$ROOT/build/$TARGET-firmware.bin"
            )
        fi
        ;;
    hp48)
        nor="$ROOT/build/$TARGET-nor.bin"
        if [[ ! -s "$nor" ]]; then
            curl -fL --retry 3 \
                https://blog.lyc8503.net/en/post/hp39gs-modify/48gii.bin \
                -o "$nor.tmp"
            mv "$nor.tmp" "$nor"
        fi
        printf '%s  %s\n' \
            4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937 \
            "$nor" |
            sha256sum -c -
        python3 "$ROOT/tools/prepare-hp48gii-nor.py" "$nor" "$STATE"
        X50_ARGS+=(
            --style="$ROOT/build/x50ng/dist/style-40g.css"
        )
        ;;
    hp4950)
        SD="$ROOT/build/$TARGET-sd"
        rm -rf "$SD"
        mkdir -p "$SD"
        X50_ARGS+=(
            --style="$ROOT/build/x50ng/dist/style-50g.css"
            --sd-dir="$SD"
            --flash
            --bootloader="$ROOT/build/x50ng/dist/firmware/$HPCLANG_E2E_BOOTLOADER"
            --firmware="$ROOT/build/x50ng/dist/firmware/hp4950v215/2MB_FIX/2MB_215f.bin"
        )
        ;;
    *)
        echo "unsupported Gallery family: $HPCLANG_E2E_FAMILY" >&2
        exit 2
        ;;
esac

export HPCLANG_X50NG_STAGE0="${HPCLANG_GALLERY_STAGE0:-$APP_DIR/launchers/$TARGET.bin}"
HPCLANG_X50NG_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_X50NG_RESULT_ADDR
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_GALLERY_DIR="$FRAMES"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"

timeout 330s "$X50" "${X50_ARGS[@]}"

if [[ "$HPCLANG_E2E_FAMILY" == hp39 ]]; then
    FLASH="$STATE/flash"
    [[ -s "$FLASH" ]] || {
        echo "missing x50ng Gallery flash backing: $FLASH" >&2
        exit 1
    }
    python3 "$ROOT/tests/e2e/harness/verify-hp39-native-font.py" \
        "$TARGET" "$FLASH" \
        --mini "$FRAMES/0000-sample.pgm" "$FRAMES/0000-text.pgm" \
        --main "$FRAMES/0001-sample.pgm" "$FRAMES/0001-text.pgm" \
        --editline "$FRAMES/0002-sample.pgm" "$FRAMES/0002-text.pgm"
fi

python3 "$ROOT/tests/e2e/harness/verify-gallery-content.py" \
    "$FRAMES" --native-face-count "$NATIVE_FACE_COUNT"

for pgm in "$ROOT"/build/"$TARGET"-*.pgm "$FRAMES"/*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

shopt -s nullglob
frames=("$FRAMES"/*.pgm)
shopt -u nullglob
if (( ${#frames[@]} == 0 )); then
    echo "Font Gallery produced no browsing frames" >&2
    exit 1
fi

MERGED="$ROOT/build/$TARGET-font-gallery.pgm"
python3 "$ROOT/tools/merge-pgm-grid.py" "$MERGED" "${frames[@]}"
python3 "$ROOT/tools/pgm-to-png.py" "$MERGED" "$MERGED.png"

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery.json"
