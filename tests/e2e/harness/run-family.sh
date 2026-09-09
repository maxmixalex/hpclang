#!/usr/bin/env bash
set -euo pipefail

if [[ $# < 3 || $# > 4 ]]; then
    echo "usage: $0 <direct|stock> <hp39|hp48|hp4950> <build-dir> [assets-dir]" >&2
    exit 2
fi

MODE="$1"
FAMILY="$2"
APP_DIR="$3"
ASSETS_DIR="${4:-}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"

case "$MODE" in
    direct)
        [[ $# == 3 ]] || { echo "Direct family runs do not take an assets directory" >&2; exit 2; }
        ;;
    stock)
        [[ $# == 4 ]] || { echo "Stock family runs require an assets directory" >&2; exit 2; }
        ;;
    *)
        echo "unsupported E2E mode: $MODE" >&2
        exit 2
        ;;
esac

case "$FAMILY" in
    hp39|hp48|hp4950) ;;
    *)
        echo "unsupported E2E family: $FAMILY" >&2
        exit 2
        ;;
esac

mapfile -t TARGETS < <(
    python3 - "$ROOT/tests/e2e/targets" "$FAMILY" <<'PY'
import json
import sys
from pathlib import Path

profiles = Path(sys.argv[1])
family = sys.argv[2]
targets = []

for path in profiles.glob("*.json"):
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("family") == family and data.get("automated_e2e", True):
        targets.append(str(data["target"]))

for target in sorted(targets):
    print(target)
PY
)

if (( ${#TARGETS[@]} == 0 )); then
    echo "no automated E2E targets for family: $FAMILY" >&2
    exit 2
fi

TARGETS_FILE="$ROOT/build/$MODE-$FAMILY-targets.txt"
mkdir -p "$ROOT/build"
printf '%s\n' "${TARGETS[@]}" > "$TARGETS_FILE"
printf 'E2E %s/%s targets: %s\n' "$MODE" "$FAMILY" "${TARGETS[*]}"

declare -a PIDS=()
for target in "${TARGETS[@]}"; do
    if [[ "$MODE" == direct ]]; then
        bash "$ROOT/tests/e2e/harness/run-direct.sh" "$target" "$APP_DIR" &
    else
        bash "$ROOT/tests/e2e/harness/run-stock.sh" "$target" "$APP_DIR" "$ASSETS_DIR" &
    fi
    PIDS+=("$!")
done

status=0
for i in "${!PIDS[@]}"; do
    if wait "${PIDS[$i]}"; then
        printf 'PASS %s\n' "${TARGETS[$i]}"
    else
        rc=$?
        printf 'FAIL %s (exit %d)\n' "${TARGETS[$i]}" "$rc" >&2
        status=1
    fi
done
exit "$status"
