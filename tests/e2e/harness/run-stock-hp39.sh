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
export TARGET

FIRMWARE="$ROOT/build/$TARGET-2.22.bin"
curl -fL --retry 3 \
    "https://www.hpcalc.org/hp39/pc/$HPCLANG_E2E_ARCHIVE" \
    -o "$ROOT/build/$HPCLANG_E2E_ARCHIVE"
unzip -p \
    "$ROOT/build/$HPCLANG_E2E_ARCHIVE" \
    "$HPCLANG_E2E_MEMBER" \
    > "$FIRMWARE"

X50_TTY="$RUNNER_TEMP/$TARGET-x50ng-uart"
KERMIT_TTY="$RUNNER_TEMP/$TARGET-kermit-uart"

python3 "$ROOT/tests/e2e/harness/serial-pace.py" \
    --x50-link "$X50_TTY" \
    --kermit-link "$KERMIT_TTY" \
    --baud 9600 \
    --log "$ROOT/build/$TARGET-socat.log" &
PACE_PID=$!

cleanup() {
    kill "${KERMIT_PID:-}" "$PACE_PID" 2>/dev/null || true
    wait "${KERMIT_PID:-}" 2>/dev/null || true
    wait "$PACE_PID" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    [[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]] && break
    kill -0 "$PACE_PID" 2>/dev/null
    sleep 0.1
done
[[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]]

bash "$ROOT/tools/run-hp39-kermit-server.sh" \
    "$KERMIT_TTY" \
    "$ASSETS_DIR/$TARGET-stock-transfer" \
    "$ROOT/build/$TARGET-kermit-debug.log" \
    > "$ROOT/build/$TARGET-kermit.out" 2>&1 &
KERMIT_PID=$!

export HPCLANG_X50NG_UART0="$X50_TTY"
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_STOCK_MODE=run
export HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
if [[ "$TARGET" == hp40gs ]]; then
    SEQUENCE="$ROOT/tests/e2e/sequences/hp40gs-stock-run.seq"
else
    SEQUENCE="$ROOT/tests/e2e/sequences/hp39gs-stock-run.seq"
fi
export HPCLANG_E2E_SEQUENCE="$SEQUENCE"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock.json"

mkdir -p "$ROOT/build/$TARGET-state"
timeout 255s "$ROOT/build/x50ng/dist/x50ng" \
    --datadir="$ROOT/build/$TARGET-state" \
    --overwrite-config \
    --style="$ROOT/build/x50ng/dist/style-40g.css" \
    --flash-full \
    --bootloader="$ROOT/build/x50ng/dist/firmware/boot-50g.bin" \
    --firmware="$FIRMWARE"

kill "$KERMIT_PID" 2>/dev/null || true
wait "$KERMIT_PID" 2>/dev/null || true
KERMIT_PID=

for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/runtime.json"
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/stock-hp39.json"
