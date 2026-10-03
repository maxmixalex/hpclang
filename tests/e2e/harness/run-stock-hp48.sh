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

NOR="$ROOT/build/$TARGET-nor.bin"
STATE="$ROOT/build/$TARGET-state"
SEND="$ROOT/build/$TARGET-send"
HOST_GATE="$ROOT/build/$TARGET-kermit-complete"
# Each launch starts from the NOR fixture, never a previous emulator snapshot.
rm -rf "$STATE" "$SEND"
mkdir -p "$SEND"
rm -f "$HOST_GATE"
rm -f "$ROOT/build/$TARGET-"*.pgm "$ROOT/build/$TARGET-"*.pgm.png \
    "$ROOT/build/$TARGET-stock.json" "$ROOT/build/$TARGET-e2e.json"

curl -fL --retry 3 \
    https://blog.lyc8503.net/en/post/hp39gs-modify/48gii.bin \
    -o "$NOR"
printf '%s  %s\n' \
    4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937 \
    "$NOR" | sha256sum -c -
python3 "$ROOT/tools/prepare-hp48gii-nor.py" "$NOR" "$STATE"

printf 'HPCLANG-E2E' > "$SEND/ZZE2E.txt"
python3 "$ROOT/tools/hp-rpl-string-wrap.py" \
    "$SEND/ZZE2E.txt" "$SEND/ZZE2E.HP"
cp "$ASSETS_DIR/L275_48" "$SEND/L275_48"
cp "$APP_DIR/launchers/$TARGET.hp" "$SEND/HPCLANG.HP"

# Focused applications may supply additional named Stock objects.
EXTRA_FILES=()
EXTRA_SEND=()
for fixture in "$APP_DIR/stock-fixtures/"*.hp; do
    [[ -f "$fixture" ]] || continue
    filename="$(basename "$fixture")"
    cp "$fixture" "$SEND/$filename"
    EXTRA_FILES+=("$SEND/$filename")
    EXTRA_SEND+=("$filename=${filename%.hp}")
done

KERMIT_TIMEOUT="$(python3 "$ROOT/tools/hp48-kermit-watchdog.py" \
    "$SEND/ZZE2E.HP" "$SEND/L275_48" "$SEND/HPCLANG.HP" "${EXTRA_FILES[@]}")"
EMULATOR_TIMEOUT=$((35 + KERMIT_TIMEOUT + 120))
TRANSPORT="${HPCLANG_E2E_STOCK_TRANSPORT:-uart}"
case "$TRANSPORT" in
    uart) SERVER_READY="$ROOT/build/$TARGET-stock-server-serial.pgm" ;;
    usb) SERVER_READY="$ROOT/build/$TARGET-stock-server-usb.pgm" ;;
    *) echo "unsupported HP48 Stock transport: $TRANSPORT" >&2; exit 2 ;;
esac
printf 'HP48 Stock %s transfer watchdog: kermit=%ss emulator=%ss\n' \
    "$TRANSPORT" "$KERMIT_TIMEOUT" "$EMULATOR_TIMEOUT"

X50_TTY="$RUNNER_TEMP/$TARGET-x50ng-$TRANSPORT"
KERMIT_TTY="$RUNNER_TEMP/$TARGET-kermit-$TRANSPORT"

socat -d -d -x -v \
    PTY,raw,echo=0,link="$X50_TTY" \
    PTY,raw,echo=0,link="$KERMIT_TTY" \
    2> "$ROOT/build/$TARGET-socat.log" &
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

if [[ "$TRANSPORT" == usb ]]; then
    export HPCLANG_X50NG_USB0="$X50_TTY"
    unset HPCLANG_X50NG_UART0 || true
else
    export HPCLANG_X50NG_UART0="$X50_TTY"
    unset HPCLANG_X50NG_USB0 || true
fi
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_E2E_HOST_GATE_FILE="$HOST_GATE"
export HPCLANG_STOCK_MODE=run
if [[ "$TRANSPORT" == usb ]]; then
    export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp48gii-stock-usb-run.seq"
else
    export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp48gii-stock-run.seq"
fi
# Interactive payloads append actions after the canonical install and launch.
if [[ -n "${HPCLANG_E2E_PAYLOAD_SEQUENCE:-}" ]]; then
    cat "$HPCLANG_E2E_SEQUENCE" "$HPCLANG_E2E_PAYLOAD_SEQUENCE" \
        > "$ROOT/build/$TARGET-stock-sequence.seq"
    export HPCLANG_E2E_SEQUENCE="$ROOT/build/$TARGET-stock-sequence.seq"
fi
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock.json"
HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_STOCK_RESULT_ADDR

# The UI sequence is no longer paced by an assumed transfer duration. x50ng
# stops at host-gate until the Kermit process below completes successfully.
# The dynamic timeouts are only watchdog ceilings for a genuinely stalled run.
timeout "${EMULATOR_TIMEOUT}s" "$ROOT/build/x50ng/dist/x50ng" \
    --datadir="$STATE" \
    --overwrite-config \
    --style="$ROOT/build/x50ng/dist/style-40g.css" &
X50_PID=$!

# Wait for the emulator's server milestone, not elapsed host time. Early USB
# bytes can latch EP3 before Stock starts receiving when emulation is slow.
while [[ ! -s "$SERVER_READY" ]] && kill -0 "$X50_PID" 2>/dev/null; do
    sleep 0.1
done
[[ -s "$SERVER_READY" ]] || {
    echo "HP48 Stock server-ready capture was not produced" >&2
    exit 1
}
# Store extras first: Stock's newest-first menu must keep HPCLANG and L275 on F1/F2.
timeout "${KERMIT_TIMEOUT}s" "$ROOT/tools/run-hp48-kermit-send.sh" \
    "$KERMIT_TTY" "$SEND" "$ROOT/build/$TARGET-kermit-debug.log" \
    --remote-host "-95 CF -95 FC?" \
    "${EXTRA_SEND[@]}" ZZE2E.HP=ZZE2E L275_48=L275 HPCLANG.HP=HPCLANG \
    > "$ROOT/build/$TARGET-kermit.out" 2>&1
: > "$HOST_GATE"
printf 'HP48 Stock Kermit completed; released host gate: %s\n' "$HOST_GATE"

wait "$X50_PID"
X50_PID=

for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/runtime.json"
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/stock-hp48.json"
