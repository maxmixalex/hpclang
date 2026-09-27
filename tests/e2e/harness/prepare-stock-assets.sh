#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <stock-build-dir> <assets-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
APP_DIR="$(realpath "$1")"
OUT="$(realpath -m "$2")"

rm -rf "$OUT"
mkdir -p "$OUT/hp39gplus-stock-transfer" "$OUT/hp39gs-stock-transfer" "$OUT/hp40gs-stock-transfer" "$OUT/hp4950"

# bootstrap-hp39-aplet.sh returns immediately when the prepared SDK cache is
# present. innoextract is therefore only a cache-miss/bootstrap dependency,
# not a normal Stock E2E runtime dependency.
bash "$ROOT/tools/bootstrap-hp39-aplet.sh" >/dev/null

for target in hp39gplus hp39gs hp40gs; do
    bash "$ROOT/tools/package-hp39-aplet.sh" \
        "$APP_DIR/launchers/$target.hp" \
        "$OUT/$target-stock-transfer" \
        "$target"
done

cp "$ROOT/.cache/hp39-aplet/raw/app/ARMToolbox/L275_48.LIB" \
    "$OUT/L275_48"

mkdir -p "$ROOT/.cache/hpcalc"
if [[ ! -f "$ROOT/.cache/hpcalc/mlreg.zip" ]]; then
    curl -fL --retry 3 --retry-delay 2 \
        https://www.hpcalc.org/hp49/math/statistic/mlreg.zip \
        -o "$ROOT/.cache/hpcalc/mlreg.zip"
fi
unzip -p "$ROOT/.cache/hpcalc/mlreg.zip" SETUP.BIN \
    > "$OUT/hp4950/SETUP.BIN"

sha256sum "$OUT/L275_48" "$OUT/hp4950/SETUP.BIN"
