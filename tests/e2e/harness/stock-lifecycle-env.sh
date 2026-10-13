#!/usr/bin/env bash
# Sourced by each Stock transport after ROOT, APP_DIR and target environment.
LIFECYCLE="$APP_DIR/e2e.stock-lifecycle.json"
unset HPCLANG_STOCK_REQUIRE_OBJECT_PHASE || true
if [[ -f "$LIFECYCLE" ]]; then
    # Keep validation outside a conditional command substitution so malformed
    # contracts fail the harness instead of silently disabling lifecycle tests.
    LIFECYCLE_FAMILY="$(python3 "$ROOT/tests/e2e/harness/stock-lifecycle.py" \
        "$LIFECYCLE" --has-family "$HPCLANG_E2E_FAMILY")"
    if [[ "$LIFECYCLE_FAMILY" == 1 ]]; then
        export HPCLANG_STOCK_REQUIRE_OBJECT_PHASE=1
    else
        LIFECYCLE=
    fi
fi
