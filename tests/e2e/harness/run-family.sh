#!/usr/bin/env bash
set -euo pipefail

if (( $# < 3 || $# > 4 )); then
    echo "usage: $0 <direct|stock> <hp39|hp48|hp4950> <build-dir> [assets-dir]" >&2
    exit 2
fi

MODE="$1"
FAMILY="$2"
APP_DIR="$3"
ASSETS_DIR="${4:-}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
REQUIRE_KEYBOARD="${HPCLANG_E2E_REQUIRE_KEYBOARD:-1}"

case "$REQUIRE_KEYBOARD" in
    0|1) ;;
    *)
        echo "HPCLANG_E2E_REQUIRE_KEYBOARD must be 0 or 1" >&2
        exit 2
        ;;
esac

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
    python3 - "$ROOT/tests/e2e/targets" "$FAMILY" "$MODE" <<'PY'
import json
import sys
from pathlib import Path

profiles = Path(sys.argv[1])
family = sys.argv[2]
mode = sys.argv[3]
targets = []

for path in profiles.glob("*.json"):
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("family") != family or not data.get("automated_e2e", True):
        continue
    if mode == "stock" and not data.get("automated_stock_e2e", True):
        continue
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
declare -a KEYBOARD_FRAME_DIRS=()
for target in "${TARGETS[@]}"; do
    frames=""
    if [[ "$REQUIRE_KEYBOARD" == 1 ]]; then
        frames="$ROOT/build/$target-keyboard-frames"
        rm -rf "$frames"
        mkdir -p "$frames"
    fi

    if [[ "$MODE" == direct ]]; then
        if [[ "$REQUIRE_KEYBOARD" == 1 ]]; then
            HPCLANG_X50NG_KEYBOARD_DIR="$frames" \
                bash "$ROOT/tests/e2e/harness/run-direct.sh" "$target" "$APP_DIR" &
        else
            bash "$ROOT/tests/e2e/harness/run-direct.sh" "$target" "$APP_DIR" &
        fi
    else
        if [[ "$REQUIRE_KEYBOARD" == 1 ]]; then
            HPCLANG_X50NG_KEYBOARD_DIR="$frames" \
                bash "$ROOT/tests/e2e/harness/run-stock.sh" "$target" "$APP_DIR" "$ASSETS_DIR" &
        else
            bash "$ROOT/tests/e2e/harness/run-stock.sh" "$target" "$APP_DIR" "$ASSETS_DIR" &
        fi
    fi
    KEYBOARD_FRAME_DIRS+=("$frames")
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

    frame_dir="${KEYBOARD_FRAME_DIRS[$i]}"
    if [[ -n "$frame_dir" ]]; then
        shopt -s nullglob
        keyboard_frames=("$frame_dir"/*.pgm)
        shopt -u nullglob
        if (( ${#keyboard_frames[@]} != 52 )); then
            printf 'FAIL %s keyboard progression: expected 52 frames, got %d\n' \
                "${TARGETS[$i]}" "${#keyboard_frames[@]}" >&2
            status=1
        else
            merged="$ROOT/build/${TARGETS[$i]}-keyboard-progress.pgm"
            python3 "$ROOT/tools/merge-pgm-grid.py" \
                "$merged" "${keyboard_frames[@]}"
            python3 "$ROOT/tools/pgm-to-png.py" "$merged" "$merged.png"
        fi
        rm -rf "$frame_dir"
    fi
done
exit "$status"
