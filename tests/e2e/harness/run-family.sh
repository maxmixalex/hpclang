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
CAPTURE_STACK_POINTER="${HPCLANG_E2E_CAPTURE_STACK_POINTER:-0}"
case "$CAPTURE_STACK_POINTER" in
    0|1) ;;
    *) echo "HPCLANG_E2E_CAPTURE_STACK_POINTER must be 0 or 1" >&2; exit 2 ;;
esac

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

target_args=("$MODE" --family "$FAMILY" --names)
if [[ -n "${HPCLANG_E2E_TARGET:-}" ]]; then
    target_args+=(--target "$HPCLANG_E2E_TARGET")
fi
mapfile -t TARGETS < <(python3 "$ROOT/tools/e2e-target-matrix.py" "${target_args[@]}")

if (( ${#TARGETS[@]} == 0 )); then
    echo "no automated E2E targets for family: $FAMILY" >&2
    exit 2
fi

TARGETS_FILE="$ROOT/build/$MODE-$FAMILY-targets.txt"
mkdir -p "$ROOT/build"
printf '%s\n' "${TARGETS[@]}" > "$TARGETS_FILE"
printf 'E2E %s/%s targets: %s\n' "$MODE" "$FAMILY" "${TARGETS[*]}"

# The workflow matrix owns parallelism. Running another emulator per target
# here multiplies that load and lets wall-clock guest timers outrun guest CPU
# execution on a busy runner. Preserve every target and aggregate failures.
status=0
for target in "${TARGETS[@]}"; do
    if [[ "$CAPTURE_STACK_POINTER" == 1 ]]; then
        export HPCLANG_X50NG_STACK_POINTER="$ROOT/build/$target-frame-stack-pointer.jsonl"
        rm -f "$HPCLANG_X50NG_STACK_POINTER" "$ROOT/build/$target-frame-stack-pointer-budget.json"
    fi
    frames=""
    if [[ "$REQUIRE_KEYBOARD" == 1 ]]; then
        frames="$ROOT/build/$target-keyboard-frames"
        rm -rf "$frames"
        mkdir -p "$frames"
    fi

    if [[ "$MODE" == direct ]]; then
        command=(bash "$ROOT/tests/e2e/harness/run-direct.sh" "$target" "$APP_DIR")
    else
        command=(bash "$ROOT/tests/e2e/harness/run-stock.sh" "$target" "$APP_DIR" "$ASSETS_DIR")
    fi
    if HPCLANG_X50NG_KEYBOARD_DIR="$frames" "${command[@]}"; then
        printf 'PASS %s\n' "$target"
    else
        rc=$?
        printf 'FAIL %s (exit %d)\n' "$target" "$rc" >&2
        status=1
    fi

    frame_dir="$frames"
    if [[ -n "$frame_dir" ]]; then
        shopt -s nullglob
        keyboard_frames=("$frame_dir"/*.pgm)
        shopt -u nullglob
        if (( ${#keyboard_frames[@]} != 52 )); then
            printf 'FAIL %s keyboard progression: expected 52 frames, got %d\n' \
                "$target" "${#keyboard_frames[@]}" >&2
            status=1
        else
            merged="$ROOT/build/$target-keyboard-progress.pgm"
            python3 "$ROOT/tools/merge-pgm-grid.py" \
                "$merged" "${keyboard_frames[@]}"
            python3 "$ROOT/tools/pgm-to-png.py" "$merged" "$merged.png"
        fi
        rm -rf "$frame_dir"
    fi
done
exit "$status"
