#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 ]]; then
    echo "usage: $0 <gallery-build-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET=hp50g
APP_DIR="$(realpath "$1")"
export TARGET

while IFS= read -r line; do
    env_name="${line%%=*}"
    env_value="${line#*=}"
    printf -v "$env_name" '%s' "$env_value"
    export "${env_name?}"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

export HPCLANG_X50NG_HARNESS=stock
export HPCLANG_X50NG_SRAM_SIZE=0x00080000
unset HPCLANG_X50NG_UART0 || true

STATE="$ROOT/build/$TARGET-stock-gallery-state"
SD="$ROOT/build/$TARGET-stock-gallery-sd"
CACHE="$ROOT/.cache/hpcalc"
rm -rf "$STATE" "$SD"
mkdir -p "$STATE" "$SD" "$CACHE"

if [[ ! -f "$CACHE/mlreg.zip" ]]; then
    curl -fL --retry 3 --retry-delay 2 \
        https://www.hpcalc.org/hp49/math/statistic/mlreg.zip \
        -o "$CACHE/mlreg.zip"
fi
unzip -p "$CACHE/mlreg.zip" SETUP.BIN > "$SD/SETUP.BIN"

COMMON_ARGS=(
    --datadir="$STATE"
    --overwrite-config
    --style="$ROOT/build/x50ng/dist/style-50g.css"
    --sd-dir="$SD"
    --bootloader="$ROOT/build/x50ng/dist/firmware/$HPCLANG_E2E_BOOTLOADER"
    --firmware="$ROOT/build/x50ng/dist/firmware/hp4950v215/2MB_FIX/2MB_215f.bin"
)

# Install ARMToolbox through the same stock path used by canonical runtime E2E.
export HPCLANG_STOCK_MODE=install
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp50g-stock-install.seq"
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock-gallery-install"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock-gallery-install.json"

timeout 45s "$ROOT/build/x50ng/dist/x50ng" \
    "${COMMON_ARGS[@]}" \
    --flash

rm -f "$SD/SETUP.BIN"
cp "$APP_DIR/launchers/$TARGET.hp" "$SD/HPCLANG.HP"

export HPCLANG_X50NG_LCD_HEIGHT="$HPCLANG_E2E_LCD_HEIGHT"
export HPCLANG_X50NG_BEFORE_PGM="$ROOT/build/$TARGET-before.pgm"
export HPCLANG_X50NG_PASS_PGM="$ROOT/build/$TARGET-pass.pgm"
export HPCLANG_X50NG_RETURN_PGM="$ROOT/build/$TARGET-return.pgm"
export HPCLANG_X50NG_E2E_JSON="$ROOT/build/$TARGET-e2e.json"
export HPCLANG_STOCK_MODE=run
export HPCLANG_E2E_SEQUENCE="$ROOT/tests/e2e/sequences/hp50g-stock-gallery-run.seq"
HPCLANG_STOCK_RESULT_ADDR="$(cat "$APP_DIR/e2e.result-address")"
export HPCLANG_STOCK_RESULT_ADDR
export HPCLANG_STOCK_PREFIX="$ROOT/build/$TARGET-stock-gallery"
export HPCLANG_STOCK_JSON="$ROOT/build/$TARGET-stock-gallery.json"

timeout 150s "$ROOT/build/x50ng/dist/x50ng" \
    "${COMMON_ARGS[@]}"

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

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery-stock.json"
