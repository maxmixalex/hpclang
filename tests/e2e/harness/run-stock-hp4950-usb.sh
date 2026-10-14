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

source "$ROOT/tests/e2e/harness/stock-lifecycle-env.sh"

STATE="$ROOT/build/$TARGET-state"
SEND="$ROOT/build/$TARGET-send"
HOST_GATE="$ROOT/build/$TARGET-kermit-complete"
KERMIT_LOG="$ROOT/build/$TARGET-kermit-debug.log"
SERVER_READY="$ROOT/build/$TARGET-stock-server-usb.pgm"
rm -rf "$STATE" "$SEND"
rm -f "$HOST_GATE" "$KERMIT_LOG" "$SERVER_READY"
# A retained development build directory must not supply stale readiness or
# screenshot/result evidence from an earlier run.
rm -f "$ROOT/build/$TARGET-"*.pgm "$ROOT/build/$TARGET-"*.pgm.png \
    "$ROOT/build/$TARGET-stock.json" "$ROOT/build/$TARGET-e2e.json"
mkdir -p "$STATE" "$SEND"

printf 'HPCLANG-E2E' > "$SEND/ZZE2E.txt"
python3 "$ROOT/tools/hp-rpl-string-wrap.py"     "$SEND/ZZE2E.txt" "$SEND/ZZE2E.HP"
cp "$ASSETS_DIR/hp4950/SETUP.BIN" "$SEND/SETUP.BIN"
cp "$SEND/SETUP.BIN" "$ROOT/build/$TARGET-setup.bin"
cp "$APP_DIR/launchers/$TARGET.hp" "$SEND/HPCLANG.HP"

# Focused applications may supply additional named Stock objects.
EXTRA_SEND=()
SETUP_ARGS=()
if [[ -f "$APP_DIR/e2e.stock-setup.json" ]]; then
    SETUP_ARGS=(--remote-host-file "$APP_DIR/e2e.stock-setup.json")
fi
for fixture in "$APP_DIR/stock-fixtures/"*.hp; do
    [[ -f "$fixture" ]] || continue
    filename="$(basename "$fixture")"
    cp "$fixture" "$SEND/$filename"
    EXTRA_SEND+=("$filename=${filename%.hp}")
done

KERMIT_TIMEOUT="$(python3 "$ROOT/tools/kermit_watchdog.py" timeout)"
EMULATOR_TIMEOUT=$((20 + KERMIT_TIMEOUT + 120))
printf 'HP49/50 Stock USB watchdog: kermit=%ss emulator=%ss\n' \
    "$KERMIT_TIMEOUT" "$EMULATOR_TIMEOUT"

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
# Reuse the canonical install/launch sequence for interactive payloads.
# Suffix offsets share the post-host-gate clock with the launch steps.
if [[ -n "${HPCLANG_E2E_PAYLOAD_SEQUENCE:-}" ]]; then
    cat "$HPCLANG_E2E_SEQUENCE" "$HPCLANG_E2E_PAYLOAD_SEQUENCE" \
        > "$ROOT/build/$TARGET-stock-sequence.seq"
    export HPCLANG_E2E_SEQUENCE="$ROOT/build/$TARGET-stock-sequence.seq"
fi
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
unset HPCLANG_STOCK_MEDIA_IMAGE || true
if [[ -f "$APP_DIR/e2e.stock-media.json" ]]; then
    export HPCLANG_STOCK_MEDIA_IMAGE="$ROOT/build/$TARGET-stock-media.img"
    python3 "$ROOT/tools/e2e-stock-media.py" prepare \
        "$APP_DIR/e2e.stock-media.json" "$HPCLANG_STOCK_MEDIA_IMAGE"
    COMMON_ARGS+=(--sd-dir="$HPCLANG_STOCK_MEDIA_IMAGE")
fi

timeout "${EMULATOR_TIMEOUT}s" "$ROOT/build/x50ng/dist/x50ng"     "${COMMON_ARGS[@]}" &
X50_PID=$!

# Do not put protocol bytes into EP3 before Stock has entered its Kermit
# server.  A packet delivered while the server is still in the I/O menu can
# leave OUT_PKT_RDY latched after an otherwise harmless idle-state interrupt,
# preventing the later retransmission from becoming a new endpoint event.
for _ in $(seq 1 400); do
    [[ -s "$SERVER_READY" ]] && break
    kill -0 "$X50_PID" 2>/dev/null || break
    sleep 0.1
done
[[ -s "$SERVER_READY" ]] || {
    echo "HP49/50 Stock USB server-ready capture was not produced" >&2
    exit 1
}

# Store extras first: Stock's newest-first menu must keep HPCLANG and SETUP on F1/F2.
python3 "$ROOT/tools/kermit_watchdog.py" run "$KERMIT_LOG" \
    "$ROOT/tools/run-hp48-kermit-send.sh" \
    "$KERMIT_TTY" "$SEND" "$KERMIT_LOG" --remote-host "-95 CF -95 FC?" \
    "${SETUP_ARGS[@]}" \
    "${EXTRA_SEND[@]}" ZZE2E.HP=ZZE2E SETUP.BIN=SETUP HPCLANG.HP=HPCLANG \
    > "$ROOT/build/$TARGET-kermit.out" 2>&1
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
if [[ -f "$LIFECYCLE" ]]; then
    python3 "$ROOT/tests/e2e/harness/stock-lifecycle.py" "$LIFECYCLE"
fi
if [[ -n "${HPCLANG_STOCK_MEDIA_IMAGE:-}" ]]; then
    python3 "$ROOT/tools/e2e-stock-media.py" verify \
        "$APP_DIR/e2e.stock-media.json" "$HPCLANG_STOCK_MEDIA_IMAGE"
fi
