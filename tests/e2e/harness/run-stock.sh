#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 3 ]]; then
    echo "usage: $0 <target> <stock-build-dir> <assets-dir>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="$1"

while IFS= read -r line; do
    export "$line"
done < <(python3 "$ROOT/tools/e2e-target-env.py" "$ROOT/tests/e2e/targets/$TARGET.json")

case "$HPCLANG_E2E_FAMILY" in
    hp39)
        exec "$ROOT/tests/e2e/harness/run-stock-hp39.sh" "$@"
        ;;
    hp48)
        exec "$ROOT/tests/e2e/harness/run-stock-hp48.sh" "$@"
        ;;
    hp4950)
        exec "$ROOT/tests/e2e/harness/run-stock-hp4950.sh" "$@"
        ;;
    *)
        echo "unsupported Stock family: $HPCLANG_E2E_FAMILY" >&2
        exit 2
        ;;
esac
