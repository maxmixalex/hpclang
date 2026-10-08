#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <hp39gplus|hp39gs|hp40gs> <gallery-build-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
APP_DIR="$(realpath "$2")"
export TARGET
case "$TARGET" in
    hp39gplus|hp39gs|hp40gs) ;;
    *) echo "unsupported HP39 Stock Gallery target: $TARGET" >&2; exit 2 ;;
esac

ASSETS="$ROOT/build/$TARGET-gallery-assets"
bash "$ROOT/tools/bootstrap-hp39-aplet.sh" >/dev/null
bash "$ROOT/tools/package-hp39-aplet.sh" \
    "$APP_DIR/launchers/$TARGET.hp" "$ASSETS/$TARGET-stock-transfer" "$TARGET"

export HPCLANG_E2E_REQUIRE_KEYBOARD=0
export HPCLANG_E2E_STOCK_TRANSPORT_OVERRIDE=usb
export HPCLANG_E2E_PAYLOAD_SEQUENCE="$ROOT/tests/e2e/sequences/hp39-stock-gallery-run.seq"
bash "$ROOT/tests/e2e/harness/run-stock.sh" "$TARGET" "$APP_DIR" "$ASSETS"
STATE="$ROOT/build/$TARGET-state"

FLASH="$STATE/flash"
[[ -s "$FLASH" ]] || {
    echo "missing x50ng Stock flash backing: $FLASH" >&2
    exit 1
}

python3 "$ROOT/tests/e2e/harness/verify-hp39-native-font.py" \
    "$TARGET" "$FLASH" \
    --mini \
        "$ROOT/build/$TARGET-stock-gallery-native-mini.pgm" \
        "$ROOT/build/$TARGET-stock-gallery-native-mini-other.pgm" \
    --main \
        "$ROOT/build/$TARGET-stock-gallery-native-main.pgm" \
        "$ROOT/build/$TARGET-stock-gallery-native-main-other.pgm" \
    --editline \
        "$ROOT/build/$TARGET-stock-gallery-native-editline.pgm" \
        "$ROOT/build/$TARGET-stock-gallery-native-editline-other.pgm"

FRAMES=(
    "$ROOT/build/$TARGET-stock-gallery-native-mini.pgm"
    "$ROOT/build/$TARGET-stock-gallery-native-main.pgm"
    "$ROOT/build/$TARGET-stock-gallery-native-editline.pgm"
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
    raise SystemExit("HP39 Stock Gallery mini/main/editline/control captures are not distinct")
PY

MERGED="$ROOT/build/$TARGET-font-gallery.pgm"
python3 "$ROOT/tools/merge-pgm-grid.py" "$MERGED" "${FRAMES[@]}"
python3 "$ROOT/tools/pgm-to-png.py" "$MERGED" "$MERGED.png"

python3 "$ROOT/tests/e2e/verify.py" "$ROOT/tests/e2e/specs/font-gallery-stock.json"
