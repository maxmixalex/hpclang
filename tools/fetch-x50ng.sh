#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
    echo "usage: $0 <dest> <frontend> <mode> [reserve-ram]" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$1"
FRONTEND="$2"
MODE="$3"
RESERVE="${4:-}"

PIN="003533d9eca3f6ded36c6ab80b8dbc0563e31d9a"

rm -rf "$DEST"
git clone --no-checkout https://codeberg.org/gwh/x50ng.git "$DEST"
git -C "$DEST" checkout "$PIN"
git -C "$DEST" submodule update --init --recursive

args=(
    "$ROOT/tools/prepare-x50ng.py"
    "$DEST"
    --frontend "$ROOT/$FRONTEND"
    --headless-header "$ROOT/tests/x50ng/headless-ui-init.h"
)

if [[ "$MODE" == "little-apple" ]]; then
    args+=(--little-apple)
elif [[ "$MODE" == "hp48gii" ]]; then
    args+=(--hp48gii)
elif [[ "$MODE" != "hp4950" ]]; then
    echo "unknown x50ng mode: $MODE" >&2
    exit 2
fi
if [[ "$RESERVE" == "reserve-ram" ]]; then
    args+=(--reserve-ram)
fi

python3 "${args[@]}"
