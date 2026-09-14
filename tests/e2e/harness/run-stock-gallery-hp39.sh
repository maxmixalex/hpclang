#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <hp39gs|hp40gs> <gallery-build-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_DIR="$(realpath "$2")"
export TARGET

case "$TARGET" in
    hp39gs|hp40gs) ;;
    *)
        echo "unsupported HP39 Stock Gallery target: $TARGET" >&2
        exit 2
        ;;
esac

while IFS= read -r line; do
    export "$line"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

export HPCLANG_X50NG_HARNESS=stock
export HPCLANG_X50NG_SRAM_SIZE=0x00080000

TRANSFER="$ROOT/build/$TARGET-stock-gallery-transfer"
STATE="$ROOT/build/$TARGET-stock-gallery-state"
FIRMWARE="$ROOT/build/$TARGET-stock-gallery-2.22.bin"
rm -rf "$TRANSFER" "$STATE"

# Reuse the reviewed HPGCC39 packaging path used by canonical Stock runtime
# E2E. The compact Gallery HPC1 differs; the stock APLET/Library transport does
# not.
bash "$ROOT/tools/bootstrap-hp39-aplet.sh" >/dev/null
bash "$ROOT/tools/package-hp39-aplet.sh" \
    "$APP_DIR/launchers/$TARGET.hp" \
    "$TRANSFER" \
    "$TARGET"

archive="$ROOT/build/$TARGET-$HPCLANG_E2E_ARCHIVE"
if [[ ! -s "$archive" ]]; then
    curl -fL --retry 3 \
        "https://www.hpcalc.org/hp39/pc/$HPCLANG_E2E_ARCHIVE" \
        -o "$archive.tmp"
    mv "$archive.tmp" "$archive"
fi
unzip -p "$archive" "$HPCLANG_E2E_MEMBER" > "$FIRMWARE"

X50_TTY="$RUNNER_TEMP/$TARGET-stock-gallery-x50ng-uart"
KERMIT_TTY="$RUNNER_TEMP/$TARGET-stock-gallery-kermit-uart"

socat -d -d -x -v \
    PTY,raw,echo=0,link="$X50_TTY" \
    PTY,raw,echo=0,link="$KERMIT_TTY" \
    2> "$ROOT/build/$TARGET-stock-gallery-socat.log" &
SOCAT_PID=$!

cleanup() {
    kill "${KERMIT_PID:-}" "$SOCAT_PID" 2>/dev/null || true
    wait "${KERMIT_PID:-}" 2>/dev/null || true
    wait "$SOCAT_PID" 2>/dev/null || true
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
    "$ROOT/build/$TARGET-stock-gallery-kermit-debug.log" \
    > "$ROOT/build/$TARGET-stock-gallery-kermit.out" 2>&1 &
KERMIT_PID=$!

export HPCLANG_X50NG_UART0="$X50_TTY"
export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_STOCK_MODE=run
export HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/$TARGET-stock-gallery-run.seq"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock-gallery"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock-gallery.json"

mkdir -p "$STATE"
timeout 510s "$ROOT/build/x50ng/dist/x50ng" \
    --datadir="$STATE" \
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

FRAMES=(
    "$ROOT/build/$TARGET-stock-gallery-native-mini.pgm"
    "$ROOT/build/$TARGET-stock-gallery-native-main.pgm"
    "$ROOT/build/$TARGET-stock-gallery-generated-control.pgm"
)

python3 - "${FRAMES[@]}" <<'PY'
from pathlib import Path
import hashlib
import sys

digests = []
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
    digests.append(hashlib.sha256(data).hexdigest())
    print(f"PASS nonblank HP39 Stock Gallery capture: {path.name}")

if len(set(digests)) != len(digests):
    raise SystemExit("HP39 Stock Gallery mini/main/control captures are not distinct")
PY

MERGED="$ROOT/build/$TARGET-font-gallery.pgm"
python3 "$ROOT/tools/merge-pgm-grid.py" "$MERGED" "${FRAMES[@]}"
python3 "$ROOT/tools/pgm-to-png.py" "$MERGED" "$MERGED.png"

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery-stock.json"
