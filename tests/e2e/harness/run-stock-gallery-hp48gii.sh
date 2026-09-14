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
    export "$line"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

export HPCLANG_X50NG_HARNESS=stock
export HPCLANG_X50NG_SRAM_SIZE=0x00040000

NOR="$ROOT/build/$TARGET-stock-gallery-nor.bin"
STATE="$ROOT/build/$TARGET-stock-gallery-state"
SEND="$ROOT/build/$TARGET-stock-gallery-send"
rm -rf "$STATE" "$SEND"
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
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp48gii-stock-gallery-run.seq"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock-gallery"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock-gallery.json"
export HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"

# The launch/install path is identical to canonical HP48 Stock runtime E2E.
# Keep the emulator alive past the conservative 110 s host Kermit budget and
# the compact Gallery's post-launch monitor/return window.
timeout 270s "$ROOT/build/x50ng/dist/x50ng" \
    --datadir="$STATE" \
    --overwrite-config \
    --style="$ROOT/build/x50ng/dist/style-40g.css" &
X50_PID=$!

sleep 35
timeout 110s "$ROOT/tools/run-hp48-kermit-send.sh" \
    "$KERMIT_TTY" "$SEND" "$ROOT/build/$TARGET-stock-gallery-kermit-debug.log" \
    L275_48=L275 HPCLANG.HP=HPCLANG \
    > "$ROOT/build/$TARGET-stock-gallery-kermit.out" 2>&1

set +e
wait "$X50_PID"
X50_STATUS=$?
set -e
X50_PID=

# The font-specific HP48 gate already distinguishes payload/native-font evidence
# from the separate visible-return concern. Preserve all captures and build the
# Gallery composite even when x50ng reports the known post-PASS return-unsettled
# state; a status gate below still rejects every other failure.
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
status = state.get('status')
if not state.get('saw_pass'):
    raise SystemExit(f'HP48 Stock Gallery did not reach HPC1 PASS: {state}')
if status not in {'pass', 'return-unsettled'}:
    raise SystemExit(f'unexpected HP48 Stock Gallery status after x50ng exit {sys.argv[2]}: {status!r}')
print(f'PASS HP48 concern-specific Gallery state: {status}; x50ng exit={sys.argv[2]}')
PY_STATUS

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery-stock.json"
