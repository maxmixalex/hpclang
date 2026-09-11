#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 ]]; then
    echo "usage: $0 <dest>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$1"
PIN="003533d9eca3f6ded36c6ab80b8dbc0563e31d9a"
SOURCE="https://codeberg.org/gwh/x50ng.git"

clone_ok=0
for attempt in 1 2 3; do
    rm -rf "$DEST"
    if git clone --no-checkout "$SOURCE" "$DEST"; then
        clone_ok=1
        break
    fi
    echo "x50ng clone attempt $attempt/3 failed" >&2
    if (( attempt < 3 )); then
        sleep $(( attempt * 2 ))
    fi
done

if (( ! clone_ok )); then
    echo "unable to clone pinned x50ng source after 3 attempts" >&2
    exit 1
fi

git -C "$DEST" checkout "$PIN"

submodule_ok=0
for attempt in 1 2 3; do
    if git -C "$DEST" submodule update --init --recursive; then
        submodule_ok=1
        break
    fi
    echo "x50ng submodule update attempt $attempt/3 failed" >&2
    if (( attempt < 3 )); then
        sleep $(( attempt * 2 ))
    fi
done

if (( ! submodule_ok )); then
    echo "unable to initialize pinned x50ng submodules after 3 attempts" >&2
    exit 1
fi

python3 "$ROOT/tools/prepare-x50ng.py" \
    "$DEST" \
    --direct-frontend "$ROOT/tests/e2e/harness/x50ng/e2e-main.c" \
    --stock-frontend "$ROOT/tests/e2e/harness/x50ng/stock-launcher-main.c" \
    --selector "$ROOT/tests/e2e/harness/x50ng/main.c" \
    --headless-header "$ROOT/tests/e2e/harness/x50ng/headless-ui-init.h"