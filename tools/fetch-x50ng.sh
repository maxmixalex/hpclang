#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
    echo "usage: $0 <dest> <frontend> <family> [reserve-ram] [host-uart0]" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$1"
FRONTEND="$2"
FAMILY="$3"
shift 3

PIN="003533d9eca3f6ded36c6ab80b8dbc0563e31d9a"

rm -rf "$DEST"
git clone --no-checkout https://codeberg.org/gwh/x50ng.git "$DEST"
git -C "$DEST" checkout "$PIN"
git -C "$DEST" submodule update --init --recursive

args=(
    "$ROOT/tools/prepare-x50ng.py"
    "$DEST"
    --frontend "$ROOT/$FRONTEND"
    --headless-header "$ROOT/tests/e2e/harness/x50ng/headless-ui-init.h"
)

if [[ "$FAMILY" == "hp39" ]]; then
    args+=(--hp39)
elif [[ "$FAMILY" == "hp48" ]]; then
    args+=(--hp48)
elif [[ "$FAMILY" != "hp4950" ]]; then
    echo "unknown x50ng family: $FAMILY" >&2
    exit 2
fi

for option in "$@"; do
    case "$option" in
        reserve-ram) args+=(--reserve-ram) ;;
        host-uart0) args+=(--host-uart0) ;;
        *)
            echo "unknown x50ng preparation option: $option" >&2
            exit 2
            ;;
    esac
done

python3 "${args[@]}"
