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

TRANSFER="$ASSETS_DIR/$TARGET-stock-transfer"
KERMIT_LOG="$ROOT/build/$TARGET-kermit-debug.log"
HOST_GATE="$ROOT/build/$TARGET-kermit-complete"
rm -f "$KERMIT_LOG" "$HOST_GATE"

X50_TTY="$RUNNER_TEMP/$TARGET-x50ng-uart"
KERMIT_TTY="$RUNNER_TEMP/$TARGET-kermit-uart"

socat -d -d -x -v \
    PTY,raw,echo=0,link="$X50_TTY" \
    PTY,raw,echo=0,link="$KERMIT_TTY" \
    2> "$ROOT/build/$TARGET-socat.log" &
SOCAT_PID=$!

cleanup() {
    kill "${GATE_PID:-}" "${KERMIT_PID:-}" "$SOCAT_PID" 2>/dev/null || true
    wait "${GATE_PID:-}" 2>/dev/null || true
    wait "${KERMIT_PID:-}" 2>/dev/null || true
    wait "$SOCAT_PID" 2>/dev/null || true
    rm -f "$HOST_GATE"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    [[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]] && break
    sleep 0.1
done
[[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]]

bash "$ROOT/tools/run-hp39-kermit-server.sh" \
    "$KERMIT_TTY" \
    "$TRANSFER" \
    "$KERMIT_LOG" \
    > "$ROOT/build/$TARGET-kermit.out" 2>&1 &
KERMIT_PID=$!

FETCH_TIMEOUT="$(python3 "$ROOT/tools/wait-hp39-kermit-fetch.py" timeout "$TRANSFER")"
X50_TIMEOUT=$((FETCH_TIMEOUT + 90))
printf 'HP39 Stock lazy-fetch watchdog: fetch=%ss emulator=%ss bytes=%s\n' \
    "$FETCH_TIMEOUT" "$X50_TIMEOUT" \
    "$(( $(stat -c %s "$TRANSFER/HPGCC000.000") + $(stat -c %s "$TRANSFER/LIB275L.000") ))"

export HPCLANG_X50NG_UART0="$X50_TTY"
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_E2E_HOST_GATE_FILE="$HOST_GATE"
export HPCLANG_STOCK_MODE=run
HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_STOCK_RESULT_ADDR
if [[ "$TARGET" == hp40gs ]]; then
    SEQUENCE="$ROOT/tests/e2e/sequences/hp40gs-stock-run.seq"
else
    SEQUENCE="$ROOT/tests/e2e/sequences/hp39gs-stock-run.seq"
fi
# Sequence timing is gated by the actual Stock serial-disk completion marker.
# The dynamic timeout is only a watchdog ceiling for a genuinely stalled fetch;
# normal launch no longer depends on a guessed HPGCC000 transfer duration.
export HPCLANG_E2E_SEQUENCE="$SEQUENCE"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock.json"

python3 "$ROOT/tools/wait-hp39-kermit-fetch.py" wait \
    "$KERMIT_LOG" "$TRANSFER" "$HOST_GATE" \
    > "$ROOT/build/$TARGET-kermit-gate.log" 2>&1 &
GATE_PID=$!

STATE="$ROOT/build/$TARGET-state"
rm -rf "$STATE"
mkdir -p "$STATE"

X50_ARGS=(
    --datadir="$STATE"
    --overwrite-config
    --style="$ROOT/build/x50ng/dist/style-40g.css"
)

if [[ -n "${HPCLANG_E2E_NOR_PATH:-}" ]]; then
    python3 "$ROOT/tools/prepare-nor-fixture.py" \
        "$ROOT/$HPCLANG_E2E_NOR_PATH" "$STATE" \
        --compression "$HPCLANG_E2E_NOR_COMPRESSION" \
        --bytes "$HPCLANG_E2E_NOR_BYTES" \
        --state-bytes "${HPCLANG_E2E_NOR_STATE_BYTES:-$HPCLANG_E2E_NOR_BYTES}" \
        --sha256 "$HPCLANG_E2E_NOR_SHA256"
else
    FIRMWARE="$ROOT/build/$TARGET-2.22.bin"
    curl -fL --retry 3 \
        "https://www.hpcalc.org/hp39/pc/$HPCLANG_E2E_ARCHIVE" \
        -o "$ROOT/build/$HPCLANG_E2E_ARCHIVE"
    unzip -p \
        "$ROOT/build/$HPCLANG_E2E_ARCHIVE" \
        "$HPCLANG_E2E_MEMBER" \
        > "$FIRMWARE"
    X50_ARGS+=(
        --flash-full
        --bootloader="$ROOT/build/x50ng/dist/firmware/boot-50g.bin"
        --firmware="$FIRMWARE"
    )
fi

timeout "${X50_TIMEOUT}s" "$ROOT/build/x50ng/dist/x50ng" "${X50_ARGS[@]}"

wait "$GATE_PID"
GATE_PID=
kill "$KERMIT_PID" 2>/dev/null || true
wait "$KERMIT_PID" 2>/dev/null || true
KERMIT_PID=

for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/runtime.json"
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/stock-hp39.json"
