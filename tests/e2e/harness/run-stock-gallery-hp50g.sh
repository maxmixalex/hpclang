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

ASSETS="$ROOT/build/$TARGET-gallery-assets"
CACHE="$ROOT/.cache/hpcalc"
mkdir -p "$ASSETS/hp4950" "$CACHE"
if [[ ! -f "$CACHE/mlreg.zip" ]]; then
    curl -fL --retry 3 --retry-delay 2 \
        https://www.hpcalc.org/hp49/math/statistic/mlreg.zip \
        -o "$CACHE/mlreg.zip"
fi
unzip -p "$CACHE/mlreg.zip" SETUP.BIN > "$ASSETS/hp4950/SETUP.BIN"

export HPCLANG_E2E_REQUIRE_KEYBOARD=0
export HPCLANG_E2E_STOCK_TRANSPORT_OVERRIDE=usb
export HPCLANG_E2E_PAYLOAD_SEQUENCE="$ROOT/tests/e2e/sequences/hp50g-stock-gallery-run.seq"
bash "$ROOT/tests/e2e/harness/run-stock.sh" "$TARGET" "$APP_DIR" "$ASSETS"

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
