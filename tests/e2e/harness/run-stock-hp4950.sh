#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 3 ]]; then
    echo "usage: $0 <target> <stock-build-dir> <assets-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_DIR="$(realpath "$2")"
ASSETS_DIR="$(realpath "$3")"
export TARGET

STATE="$ROOT/build/$TARGET-state"
SD="$ROOT/build/$TARGET-sd"
rm -rf "$STATE" "$SD"
mkdir -p "$STATE" "$SD"

cp "$ASSETS_DIR/hp4950/SETUP.BIN" "$SD/SETUP.BIN"

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

timeout 45s "$ROOT/build/x50ng/dist/x50ng"     "${COMMON_ARGS[@]}"     --flash

mv "$SD/SETUP.BIN" "$ROOT/build/$TARGET-setup.bin"
cp "$APP_DIR/launchers/$TARGET.hp" "$SD/HPCLANG.HP"
printf 'HPCLANG-STORAGE\n' > "$SD/HPCLANG.TST"
printf 'OLD-CONTENT-MUST-BE-TRUNCATED\n' > "$SD/HPCLANG.OUT"

export HPCLANG_STOCK_MODE=run
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp50g-stock-run.seq"
export HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-run"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-run.json"

timeout 45s "$ROOT/build/x50ng/dist/x50ng"     "${COMMON_ARGS[@]}"

for pgm in "$ROOT"/build/"$TARGET"-install-*.pgm "$ROOT"/build/"$TARGET"-run-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/stock-hp4950.json"
