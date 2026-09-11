#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <direct|stock> <hp39|hp48|hp4950>" >&2
    exit 2
fi

MODE="$1"
FAMILY="$2"
case "$MODE:$FAMILY" in
    direct:hp39|direct:hp48|direct:hp4950|stock:hp39|stock:hp48|stock:hp4950) ;;
    *)
        echo "unsupported E2E runtime selection: $MODE $FAMILY" >&2
        exit 2
        ;;
esac

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
DIST="${HPCLANG_X50NG_DIST:-$ROOT/build/x50ng/dist}"
LIBDIR="$DIST/runtime-lib"
TOOLDIR="$DIST/tools"

[[ -x "$DIST/x50ng" ]] || {
    echo "missing universal x50ng binary: $DIST/x50ng" >&2
    exit 1
}
[[ -d "$LIBDIR" ]] || {
    echo "missing portable x50ng runtime libraries: $LIBDIR" >&2
    exit 1
}
[[ -f "$DIST/.hpclang-portable" ]] || {
    echo "x50ng artifact is not marked as a portable hpclang E2E bundle" >&2
    exit 1
}

RUNTIME_LD_LIBRARY_PATH="$LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if env LD_LIBRARY_PATH="$RUNTIME_LD_LIBRARY_PATH" ldd "$DIST/x50ng" | tee "$ROOT/build/x50ng-runtime.ldd" | grep -q 'not found'; then
    echo "unresolved universal x50ng runtime dependency" >&2
    exit 1
fi

if [[ "$MODE" == stock && ( "$FAMILY" == hp39 || "$FAMILY" == hp48 ) ]]; then
    [[ -x "$TOOLDIR/kermit" ]] || {
        echo "portable E2E bundle is missing C-Kermit" >&2
        exit 1
    }
    [[ -x "$TOOLDIR/socat" ]] || {
        echo "portable E2E bundle is missing socat" >&2
        exit 1
    }
    for tool in "$TOOLDIR/kermit" "$TOOLDIR/socat"; do
        if env LD_LIBRARY_PATH="$RUNTIME_LD_LIBRARY_PATH" ldd "$tool" | grep -q 'not found'; then
            echo "unresolved portable runtime dependency for $tool" >&2
            exit 1
        fi
    done
fi

if [[ -n "${GITHUB_ENV:-}" ]]; then
    printf 'LD_LIBRARY_PATH=%s\n' "$RUNTIME_LD_LIBRARY_PATH" >> "$GITHUB_ENV"
fi
if [[ -n "${GITHUB_PATH:-}" && -d "$TOOLDIR" ]]; then
    printf '%s\n' "$TOOLDIR" >> "$GITHUB_PATH"
fi

printf 'portable x50ng ready: mode=%s family=%s\n' "$MODE" "$FAMILY"
