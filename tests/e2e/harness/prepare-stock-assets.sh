#!/usr/bin/env bash
set -euo pipefail

if (( $# < 2 || $# > 3 )); then
    echo "usage: $0 <stock-build-dir> <assets-dir> [hp39,hp48,hp4950]" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
APP_DIR="$(realpath "$1")"
OUT="$(realpath -m "$2")"
FAMILIES="${3:-hp39,hp48,hp4950}"

has_family() {
    case ",$FAMILIES," in
        *",$1,"*) return 0 ;;
        *) return 1 ;;
    esac
}

for family in hp39 hp48 hp4950; do
    if [[ ",$FAMILIES," == *",$family,"* ]]; then
        continue
    fi
done

rm -rf "$OUT"
mkdir -p "$OUT"

# HP39 and HP48 both consume authoritative assets from the HPGCC 1.1
# 39G-support kit. The prepared cache makes this a no-op on normal runs.
if has_family hp39 || has_family hp48; then
    bash "$ROOT/tools/bootstrap-hp39-aplet.sh" >/dev/null
fi

if has_family hp39; then
    mkdir -p "$OUT/hp39gs-stock-transfer" "$OUT/hp40gs-stock-transfer"
    for target in hp39gs hp40gs; do
        bash "$ROOT/tools/package-hp39-aplet.sh" \
            "$APP_DIR/launchers/$target.hp" \
            "$OUT/$target-stock-transfer" \
            "$target"
    done
fi

if has_family hp48; then
    cp "$ROOT/.cache/hp39-aplet/raw/app/ARMToolbox/L275_48.LIB" \
        "$OUT/L275_48"
fi

if has_family hp4950; then
    mkdir -p "$OUT/hp4950" "$ROOT/.cache/hpcalc"
    if [[ ! -f "$ROOT/.cache/hpcalc/mlreg.zip" ]]; then
        curl -fL --retry 3 --retry-delay 2 \
            https://www.hpcalc.org/hp49/math/statistic/mlreg.zip \
            -o "$ROOT/.cache/hpcalc/mlreg.zip"
    fi
    unzip -p "$ROOT/.cache/hpcalc/mlreg.zip" SETUP.BIN \
        > "$OUT/hp4950/SETUP.BIN"
fi

declare -a checks=()
has_family hp48 && checks+=("$OUT/L275_48")
has_family hp4950 && checks+=("$OUT/hp4950/SETUP.BIN")
if (( ${#checks[@]} != 0 )); then
    sha256sum "${checks[@]}"
fi
