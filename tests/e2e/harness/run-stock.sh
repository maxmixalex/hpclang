#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 3 ]]; then
    echo "usage: $0 <target> <stock-build-dir> <assets-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"
export HPCLANG_E2E_FRAME_PREFIX="$ROOT/build/$TARGET-frame"
rm -f "$HPCLANG_E2E_FRAME_PREFIX"-*.pgm "$HPCLANG_E2E_FRAME_PREFIX"-*.lcd \
    "$HPCLANG_E2E_FRAME_PREFIX"-memory-*.json \
    "$HPCLANG_E2E_FRAME_PREFIX"-pool-*.json \
    "$HPCLANG_E2E_FRAME_PREFIX"-allocation-*.json \
    "$ROOT/build/$TARGET-native-release.json"

while IFS= read -r line; do
    env_name="${line%%=*}"
    env_value="${line#*=}"
    printf -v "$env_name" '%s' "$env_value"
    export "${env_name?}"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

if [[ -n "${HPCLANG_E2E_STOCK_TRANSPORT_OVERRIDE:-}" ]]; then
    HPCLANG_E2E_STOCK_TRANSPORT="$HPCLANG_E2E_STOCK_TRANSPORT_OVERRIDE"
    export HPCLANG_E2E_STOCK_TRANSPORT
fi
if [[ -f "$2/e2e.stock-media.json" &&
      ( "$HPCLANG_E2E_FAMILY" != hp4950 || "$HPCLANG_E2E_STOCK_TRANSPORT" != usb ) ]]; then
    echo "Raw-card fixtures require HP49g+/HP50g Stock USB installation" >&2
    exit 2
fi

export HPCLANG_X50NG_HARNESS=stock
if [[ "$HPCLANG_E2E_FAMILY" == hp48 ]]; then
    export HPCLANG_X50NG_SRAM_SIZE=0x00040000
else
    export HPCLANG_X50NG_SRAM_SIZE=0x00080000
fi

case "$HPCLANG_E2E_FAMILY" in
    hp39)
        exec "$ROOT/tests/e2e/harness/run-stock-hp39.sh" "$@"
        ;;
    hp48)
        exec "$ROOT/tests/e2e/harness/run-stock-hp48.sh" "$@"
        ;;
    hp4950)
        unset HPCLANG_X50NG_UART0 || true
        exec "$ROOT/tests/e2e/harness/run-stock-hp4950.sh" "$@"
        ;;
    *)
        echo "unsupported Stock family: $HPCLANG_E2E_FAMILY" >&2
        exit 2
        ;;
esac
