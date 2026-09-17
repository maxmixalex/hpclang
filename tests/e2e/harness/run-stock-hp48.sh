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
mkdir -p "$SEND"

curl -fL --retry 3 \
    https://blog.lyc8503.net/en/post/hp39gs-modify/48gii.bin \
    -o "$NOR"
printf '%s  %s\n' \
    4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937 \
    "$NOR" | sha256sum -c -
python3 "$ROOT/tools/prepare-hp48gii-nor.py" "$NOR" "$STATE"

cp "$ASSETS_DIR/L275_48" "$SEND/L275_48"
cp "$APP_DIR/launchers/$TARGET.hp" "$SEND/HPCLANG.HP"

X50_TTY="$RUNNER_TEMP/$TARGET-x50ng-uart"
KERMIT_TTY="$RUNNER_TEMP/$TARGET-kermit-uart"

socat -d -d -x -v \
    PTY,raw,echo=0,link="$X50_TTY" \
    PTY,raw,echo=0,link="$KERMIT_TTY" \
    2> "$ROOT/build/$TARGET-socat.log" &
SOCAT_PID=$!

cleanup() {
    kill "${X50_PID:-}" "$SOCAT_PID" 2>/dev/null || true
    wait "${X50_PID:-}" 2>/dev/null || true
    wait "$SOCAT_PID" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    [[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]] && break
    sleep 0.1
done
[[ -e "$X50_TTY" && -e "$KERMIT_TTY" ]]

export HPCLANG_X50NG_UART0="$X50_TTY"
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_STOCK_MODE=run
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp48gii-stock-run.seq"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock.json"
export HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"

# The current canonical launcher is roughly 40 KiB. At 9600 baud with the
# stock-compatible 80-byte Kermit packets and control-byte quoting it needs
# more than the historical 70 s transfer window. Keep the emulator alive past
# the conservative host-transfer window plus the final 60 s PASS monitor.
timeout 270s "$ROOT/build/x50ng/dist/x50ng" \
    --datadir="$STATE" \
    --overwrite-config \
    --style="$ROOT/build/x50ng/dist/style-40g.css" &
X50_PID=$!

sleep 35
timeout 110s "$ROOT/tools/run-hp48-kermit-send.sh" \
    "$KERMIT_TTY" "$SEND" "$ROOT/build/$TARGET-kermit-debug.log" \
    L275_48=L275 HPCLANG.HP=HPCLANG \
    > "$ROOT/build/$TARGET-kermit.out" 2>&1

set +e
wait "$X50_PID"
X50_STATUS=$?
set -e
X50_PID=

# x50ng serializes its final ARM/module state to config.lua during shutdown.
# Retain that text alongside the existing Kermit diagnostics so a failed stock
# return can be distinguished from an LCD-only settling failure without adding
# any live instrumentation to the emulator or payload.
if [[ -f "$STATE/config.lua" ]]; then
    cp "$STATE/config.lua" "$ROOT/build/$TARGET-kermit-state.log"
fi

if (( X50_STATUS != 0 )); then
    exit "$X50_STATUS"
fi

for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/runtime.json"
python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/stock-hp48.json"
