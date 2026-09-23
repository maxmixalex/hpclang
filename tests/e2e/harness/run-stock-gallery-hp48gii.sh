#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <gallery-build-dir> <assets-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET=hp48gii
APP_DIR="$(realpath "$1")"
ASSETS_DIR="$(realpath "$2")"
export TARGET

while IFS= read -r line; do
    env_name="${line%%=*}"
    env_value="${line#*=}"
    printf -v "$env_name" '%s' "$env_value"
    export "$env_name"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

export HPCLANG_X50NG_HARNESS=stock
export HPCLANG_X50NG_SRAM_SIZE=0x00040000

NOR="$ROOT/build/$TARGET-stock-gallery-nor.bin"
STATE="$ROOT/build/$TARGET-stock-gallery-state"
SEND="$ROOT/build/$TARGET-stock-gallery-send"
HOST_GATE="$ROOT/build/$TARGET-stock-gallery-kermit-complete"
rm -rf "$STATE" "$SEND"
rm -f "$HOST_GATE"
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

KERMIT_TIMEOUT="$(python3 "$ROOT/tools/hp48-kermit-watchdog.py" \
    "$SEND/L275_48" "$SEND/HPCLANG.HP")"
EMULATOR_TIMEOUT=$((35 + KERMIT_TIMEOUT + 120))
printf 'HP48 Stock Gallery transfer watchdog: kermit=%ss emulator=%ss bytes=%s\n' \
    "$KERMIT_TIMEOUT" "$EMULATOR_TIMEOUT" \
    "$(( $(stat -c %s "$SEND/L275_48") + $(stat -c %s "$SEND/HPCLANG.HP") ))"

X50_TTY="$RUNNER_TEMP/$TARGET-stock-gallery-x50ng-uart"
KERMIT_TTY="$RUNNER_TEMP/$TARGET-stock-gallery-kermit-uart"

socat -d -d -x -v \
    PTY,raw,echo=0,link="$X50_TTY" \
    PTY,raw,echo=0,link="$KERMIT_TTY" \
    2> "$ROOT/build/$TARGET-stock-gallery-socat.log" &
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

export HPCLANG_X50NG_UART0="$X50_TTY"
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_E2E_HOST_GATE_FILE="$HOST_GATE"
export HPCLANG_STOCK_MODE=run
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp48gii-stock-gallery-run.seq"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock-gallery"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock-gallery.json"
HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_STOCK_RESULT_ADDR

# UI automation resumes only after Kermit actually finishes; these dynamic
# limits are watchdog ceilings rather than synchronization delays.
timeout "${EMULATOR_TIMEOUT}s" "$ROOT/build/x50ng/dist/x50ng" \
    --datadir="$STATE" \
    --overwrite-config \
    --style="$ROOT/build/x50ng/dist/style-40g.css" &
X50_PID=$!

sleep 35
timeout "${KERMIT_TIMEOUT}s" "$ROOT/tools/run-hp48-kermit-send.sh" \
    "$KERMIT_TTY" "$SEND" "$ROOT/build/$TARGET-stock-gallery-kermit-debug.log" \
    L275_48=L275 HPCLANG.HP=HPCLANG \
    > "$ROOT/build/$TARGET-stock-gallery-kermit.out" 2>&1
: > "$HOST_GATE"
printf 'HP48 Stock Gallery Kermit completed; released host gate: %s\n' "$HOST_GATE"

set +e
wait "$X50_PID"
X50_STATUS=$?
set -e
X50_PID=

# Preserve all captures before applying the strict return gate so a regression
# still leaves useful diagnostics. HP48 Gallery requires the same settled Stock
# return contract as canonical Runtime E2E; return-unsettled is a failure.
for pgm in "$ROOT"/build/"$TARGET"-*.pgm; do
    [[ -f "$pgm" ]] || continue
    python3 "$ROOT/tools/pgm-to-png.py" "$pgm" "$pgm.png"
done

FRAMES=(
    "$ROOT/build/$TARGET-stock-gallery-native-mini.pgm"
    "$ROOT/build/$TARGET-stock-gallery-native-main.pgm"
    "$ROOT/build/$TARGET-stock-gallery-generated-control.pgm"
)

python3 - "${FRAMES[@]}" <<'PY'
from pathlib import Path
import sys

for raw in sys.argv[1:]:
    path = Path(raw)
    data = path.read_bytes()
    try:
        magic, geometry, maxval, pixels = data.split(b"\n", 3)
    except ValueError as exc:
        raise SystemExit(f"{path}: malformed PGM") from exc
    if magic != b"P5" or maxval != b"255":
        raise SystemExit(f"{path}: unexpected PGM header")
    width, height = map(int, geometry.split())
    pixels = pixels[: width * height]
    if len(pixels) != width * height:
        raise SystemExit(f"{path}: truncated PGM")
    if not any(pixel != 255 for pixel in pixels):
        raise SystemExit(f"{path}: blank Stock Gallery capture")
    print(f"PASS nonblank Stock Gallery capture: {path.name}")
PY

MERGED="$ROOT/build/$TARGET-font-gallery.pgm"
python3 "$ROOT/tools/merge-pgm-grid.py" "$MERGED" "${FRAMES[@]}"
python3 "$ROOT/tools/pgm-to-png.py" "$MERGED" "$MERGED.png"

python3 - "$ROOT/build/$TARGET-stock-gallery.json" "$X50_STATUS" <<'PY_STATUS'
import json
from pathlib import Path
import sys

state = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
x50_status = int(sys.argv[2])
if x50_status != 0:
    raise SystemExit(f'HP48 Stock Gallery x50ng exited with {x50_status}: {state}')
if not state.get('saw_pass'):
    raise SystemExit(f'HP48 Stock Gallery did not reach HPC1 PASS: {state}')
if state.get('status') != 'pass':
    raise SystemExit(f'HP48 Stock Gallery did not reach settled PASS: {state}')
if not state.get('returned') or not state.get('return_settled'):
    raise SystemExit(f'HP48 Stock Gallery did not settle after return: {state}')
print('PASS HP48 Stock Gallery: HPC1 PASS, settled Stock return, x50ng exit=0')
PY_STATUS

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery-stock.json"
