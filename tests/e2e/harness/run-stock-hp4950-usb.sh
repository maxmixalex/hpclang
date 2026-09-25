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

STATE="$ROOT/build/$TARGET-state"
SEND="$ROOT/build/$TARGET-send"
HOST_GATE="$ROOT/build/$TARGET-kermit-complete"
KERMIT_LOG="$ROOT/build/$TARGET-kermit-debug.log"
rm -rf "$STATE" "$SEND"
rm -f "$HOST_GATE" "$KERMIT_LOG"
mkdir -p "$STATE" "$SEND"

printf 'HPCLANG-E2E' > "$SEND/ZZE2E.txt"
python3 "$ROOT/tools/hp-rpl-string-wrap.py"     "$SEND/ZZE2E.txt" "$SEND/ZZE2E.HP"
cp "$ASSETS_DIR/hp4950/SETUP.BIN" "$SEND/SETUP.BIN"
cp "$SEND/SETUP.BIN" "$ROOT/build/$TARGET-setup.bin"
cp "$APP_DIR/launchers/$TARGET.hp" "$SEND/HPCLANG.HP"

KERMIT_TIMEOUT="$(python3 "$ROOT/tools/hp48-kermit-watchdog.py"     "$SEND/ZZE2E.HP" "$SEND/SETUP.BIN" "$SEND/HPCLANG.HP")"
EMULATOR_TIMEOUT=$((20 + KERMIT_TIMEOUT + 120))
printf 'HP49/50 Stock USB watchdog: kermit=%ss emulator=%ss bytes=%s\n'     "$KERMIT_TIMEOUT" "$EMULATOR_TIMEOUT"     "$(( $(stat -c %s "$SEND/ZZE2E.HP") + $(stat -c %s "$SEND/SETUP.BIN") + $(stat -c %s "$SEND/HPCLANG.HP") ))"

X50_TTY="$RUNNER_TEMP/$TARGET-x50ng-usb"
KERMIT_TTY="$RUNNER_TEMP/$TARGET-kermit-usb"

socat -d -d -x -v     PTY,raw,echo=0,link="$X50_TTY"     PTY,raw,echo=0,link="$KERMIT_TTY"     2> "$ROOT/build/$TARGET-socat.log" &
SOCAT_PID=$!

cleanup() {
    kill "${X50_PID:-}" "$SOCAT_PID" 2>/dev/null || true
    wait "${X50_PID:-}" 2>/dev/null || true
    wait "$SOCAT_PID" 2>/dev/null || true
    rm -f "$HOST_GATE"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    [[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]] && break
    sleep 0.1
done
[[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]]

export HPCLANG_X50NG_USB0="$X50_TTY"
unset HPCLANG_X50NG_UART0 || true
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_E2E_HOST_GATE_FILE="$HOST_GATE"
export HPCLANG_STOCK_MODE=run
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp4950-stock-usb-run.seq"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock.json"
HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_STOCK_RESULT_ADDR

COMMON_ARGS=(
    --datadir="$STATE"
    --overwrite-config
    --style="$ROOT/build/x50ng/dist/style-50g.css"
    --bootloader="$ROOT/build/x50ng/dist/firmware/$HPCLANG_E2E_BOOTLOADER"
    --firmware="$ROOT/build/x50ng/dist/firmware/hp4950v215/2MB_FIX/2MB_215f.bin"
    --flash
)

timeout "${EMULATOR_TIMEOUT}s" "$ROOT/build/x50ng/dist/x50ng"     "${COMMON_ARGS[@]}" &
X50_PID=$!

# HP48's stable Stock USB path has the host Kermit active before the
# calculator launches its on-device Kermit process. Do the same here, but
# gate on the deterministic post-I/O-configuration capture rather than a
# guessed sleep. Starting only after "Awaiting Server Cmd." lets both peers
# enter the initial-send state at once and they repeatedly exchange S packets.
HOST_READY="$ROOT/build/$TARGET-stock-after-io-config.pgm"
for _ in $(seq 1 300); do
    [[ -s "$HOST_READY" ]] && break
    kill -0 "$X50_PID" 2>/dev/null || break
    sleep 0.1
done
[[ -s "$HOST_READY" ]] || {
    echo "HP49/50 Stock USB I/O-config capture was not produced" >&2
    exit 1
}

timeout "${KERMIT_TIMEOUT}s" "$ROOT/tools/run-hp48-kermit-send.sh"     "$KERMIT_TTY" "$SEND" "$KERMIT_LOG"     ZZE2E.HP=ZZE2E SETUP.BIN=SETUP HPCLANG.HP=HPCLANG     > "$ROOT/build/$TARGET-kermit.out" 2>&1
: > "$HOST_GATE"
printf 'HP49/50 Stock USB Kermit completed; released host gate: %s\n' "$HOST_GATE"

wait "$X50_PID"
X50_PID=

for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/runtime.json"
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/stock-hp4950-usb.json"
