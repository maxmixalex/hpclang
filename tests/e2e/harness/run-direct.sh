#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <target> <runtime-build-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_DIR="$(realpath "$2")"
export TARGET

while IFS= read -r line; do
    export "$line"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

export HPCLANG_X50NG_HARNESS=direct
if [[ "$HPCLANG_E2E_FAMILY" == hp48 ]]; then
    export HPCLANG_X50NG_SRAM_SIZE=0x00040000
else
    export HPCLANG_X50NG_SRAM_SIZE=0x00080000
fi
unset HPCLANG_X50NG_UART0 || true

X50="$ROOT/build/x50ng/dist/x50ng"
STATE="$ROOT/build/$TARGET-state"
rm -rf "$STATE"
mkdir -p "$STATE"

X50_ARGS=(
    --datadir="$STATE"
    --overwrite-config
)

case "$HPCLANG_E2E_FAMILY" in
    hp39)
        curl -fL --retry 3 \
            "https://www.hpcalc.org/hp39/pc/$HPCLANG_E2E_ARCHIVE" \
            -o "$ROOT/build/$HPCLANG_E2E_ARCHIVE"
        unzip -p \
            "$ROOT/build/$HPCLANG_E2E_ARCHIVE" \
            "$HPCLANG_E2E_MEMBER" \
            > "$ROOT/build/$TARGET-firmware.bin"
        X50_ARGS+=(
            --style="$ROOT/build/x50ng/dist/style-40g.css"
            --flash-full
            --bootloader="$ROOT/build/x50ng/dist/firmware/boot-50g.bin"
            --firmware="$ROOT/build/$TARGET-firmware.bin"
        )
        ;;
    hp48)
        curl -fL --retry 3 \
            https://blog.lyc8503.net/en/post/hp39gs-modify/48gii.bin \
            -o "$ROOT/build/hp48gii-nor.bin"
        printf '%s  %s\n' \
            4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937 \
            "$ROOT/build/hp48gii-nor.bin" |
            sha256sum -c -
        python3 "$ROOT/tools/prepare-hp48gii-nor.py" \
            "$ROOT/build/hp48gii-nor.bin" "$STATE"
        X50_ARGS+=(
            --style="$ROOT/build/x50ng/dist/style-40g.css"
        )
        ;;
    hp4950)
        SD="$ROOT/build/$TARGET-sd"
        rm -rf "$SD"
        mkdir -p "$SD/ZZHPDIR"
        printf 'HPCLANG-STORAGE\n' > "$SD/HPCLANG.TST"
        printf 'OLD-CONTENT-MUST-BE-TRUNCATED\n' > "$SD/HPCLANG.OUT"
        X50_ARGS+=(
            --style="$ROOT/build/x50ng/dist/style-50g.css"
            --sd-dir="$SD"
            --flash
            --bootloader="$ROOT/build/x50ng/dist/firmware/$HPCLANG_E2E_BOOTLOADER"
            --firmware="$ROOT/build/x50ng/dist/firmware/hp4950v215/2MB_FIX/2MB_215f.bin"
        )
        ;;
    *)
        echo "unsupported Direct family: $HPCLANG_E2E_FAMILY" >&2
        exit 2
        ;;
esac

export HPCLANG_X50NG_STAGE0="$APP_DIR/launchers/$TARGET.bin"
export HPCLANG_X50NG_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"

timeout 35s "$X50" "${X50_ARGS[@]}"

for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/runtime.json"
