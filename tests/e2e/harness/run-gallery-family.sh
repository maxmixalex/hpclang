#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <hp39|hp48|hp4950> <build-dir>" >&2
    exit 2
fi

FAMILY="$1"
APP_DIR="$2"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"

case "$FAMILY" in
    hp39|hp48|hp4950) ;;
    *) echo "unsupported Gallery family: $FAMILY" >&2; exit 2 ;;
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
    echo "no automated Gallery targets for family: $FAMILY" >&2
    exit 2
fi

TARGETS_FILE="$ROOT/build/gallery-$FAMILY-targets.txt"
mkdir -p "$ROOT/build"
printf '%s\n' "${TARGETS[@]}" > "$TARGETS_FILE"
printf 'Gallery %s targets: %s\n' "$FAMILY" "${TARGETS[*]}"

declare -a PIDS=()
for target in "${TARGETS[@]}"; do
    bash "$ROOT/tests/e2e/harness/run-gallery-shards.sh" "$target" "$APP_DIR" &
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
