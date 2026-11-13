#!/usr/bin/env bash
# Source after firmware preparation, immediately before the measured emulator.
# ROOT and TARGET belong to the Stock runner. Disabled runs remain unchanged.
case "${HPCLANG_E2E_CAPTURE_NATIVE_HEAP:-0}" in
    0) return 0 ;;
    1) ;;
    *) echo "HPCLANG_E2E_CAPTURE_NATIVE_HEAP must be 0 or 1" >&2; return 2 ;;
esac
export HPCLANG_X50NG_NATIVE_HEAP="$ROOT/build/$TARGET-native-heap.json"
heap_env="$ROOT/build/$TARGET-native-heap.env"
rm -f "$heap_env" "$HPCLANG_X50NG_NATIVE_HEAP" "$ROOT/build/$TARGET-native-heap-verified.json"
python3 "$ROOT/tests/e2e/harness/native-heap-profile.py" "$@" \
    --model "$TARGET" --shell-env --output "$heap_env" || return $?
# The generated file contains only three validated numeric exports.
source "$heap_env"
unset heap_env
