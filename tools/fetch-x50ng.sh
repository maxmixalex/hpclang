#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 ]]; then
    echo "usage: $0 <dest>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$1"
PIN="003533d9eca3f6ded36c6ab80b8dbc0563e31d9a"

rm -rf "$DEST"
git clone --no-checkout https://codeberg.org/gwh/x50ng.git "$DEST"
git -C "$DEST" checkout "$PIN"
git -C "$DEST" submodule update --init --recursive

python3 "$ROOT/tools/prepare-x50ng.py" \
    "$DEST" \
    --direct-frontend "$ROOT/tests/e2e/harness/x50ng/e2e-main.c" \
    --stock-frontend "$ROOT/tests/e2e/harness/x50ng/stock-launcher-main.c" \
    --selector "$ROOT/tests/e2e/harness/x50ng/main.c" \
    --headless-header "$ROOT/tests/e2e/harness/x50ng/headless-ui-init.h"
