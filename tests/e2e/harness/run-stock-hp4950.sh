#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 3 ]]; then
    echo "usage: $0 <target> <runtime-build-dir> <assets-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_DIR="$(realpath "$2")"
ASSETS_DIR="$(realpath "$3")"
TRANSPORT="${HPCLANG_E2E_STOCK_TRANSPORT:-sd}"
STORAGE_WITNESS="${HPCLANG_E2E_STORAGE_WITNESS:-0}"
export TARGET

if [[ "$TRANSPORT" == usb ]]; then
    exec "$ROOT/tests/e2e/harness/run-stock-hp4950-usb.sh" "$@"
fi
[[ "$TRANSPORT" == sd ]] || {
    echo "unsupported HP49/50 Stock transport: $TRANSPORT" >&2
    exit 2
}

case "$STORAGE_WITNESS" in
    0|1) ;;
    *)
        echo "HPCLANG_E2E_STORAGE_WITNESS must be 0 or 1" >&2
        exit 2
        ;;
esac

STATE="$ROOT/build/$TARGET-state"
SD="$ROOT/build/$TARGET-sd"
# Retained local runs must not mix prior USB/SD success evidence into this run.
rm -f "$ROOT/build/$TARGET-"*.pgm "$ROOT/build/$TARGET-"*.pgm.png \
    "$ROOT/build/$TARGET-install.json" "$ROOT/build/$TARGET-run.json" \
    "$ROOT/build/$TARGET-stock.json" "$ROOT/build/$TARGET-e2e.json"
rm -rf "$STATE" "$SD"
mkdir -p "$STATE" "$SD"

cp "$ASSETS_DIR/hp4950/SETUP.BIN" "$SD/SETUP.BIN"

export HPCLANG_X50NG_SD_DIR="$SD"

COMMON_ARGS=(
    --datadir="$STATE"
    --overwrite-config
    --style="$ROOT/build/x50ng/dist/style-50g.css"
    --sd-dir="$SD"
    --bootloader="$ROOT/build/x50ng/dist/firmware/$HPCLANG_E2E_BOOTLOADER"
    --firmware="$ROOT/build/x50ng/dist/firmware/hp4950v215/2MB_FIX/2MB_215f.bin"
)

export HPCLANG_STOCK_MODE=install
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp50g-stock-install.seq"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-install"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-install.json"

timeout 45s "$ROOT/build/x50ng/dist/x50ng" \
    "${COMMON_ARGS[@]}" \
    --flash

mv "$SD/SETUP.BIN" "$ROOT/build/$TARGET-setup.bin"
cp "$APP_DIR/launchers/$TARGET.hp" "$SD/HPCLANG.HP"
printf 'HPCLANG-STORAGE\n' > "$SD/HPCLANG.TST"
printf 'OLD-CONTENT-MUST-BE-TRUNCATED\n' > "$SD/HPCLANG.OUT"

if [[ "$STORAGE_WITNESS" == 1 ]]; then
    source "$ROOT/tests/e2e/harness/storage-media.sh"
    export TZ=UTC
    touch -d '2024-01-02 03:04:06 UTC' "$SD/HPCLANG.TST"
    prepare_storage_media "$SD"
fi

# x50ng's VVFAT backend indexes --sd-dir when the run-phase emulator opens
# the card. Create the named-directory witness only after ARMToolbox install
# has completed, but before this second x50ng process starts. The storage-only
# sequence below accounts for the extra root entry while the canonical Runtime
# launch choreography remains unchanged.
if [[ "$STORAGE_WITNESS" == 1 ]]; then
    mkdir -p "$SD/ZZHPDIR"
fi

export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_STOCK_MODE=run
if [[ "$STORAGE_WITNESS" == 1 ]]; then
    export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp50g-stock-storage-run.seq"
else
    export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp50g-stock-run.seq"
fi
HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_STOCK_RESULT_ADDR
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-run"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-run.json"

timeout 45s "$ROOT/build/x50ng/dist/x50ng" \
    "${COMMON_ARGS[@]}"
if [[ "$STORAGE_WITNESS" == 1 ]]; then
    verify_storage_media
fi

for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/runtime.json"
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/stock-hp4950.json"
