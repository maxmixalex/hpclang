#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
: "${ATFE:?Set ATFE to the pinned Arm Toolchain for Embedded directory}"
cmake -S tests/unit -B build/analysis-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
for source in tests/e2e tests/e2e/font-gallery; do
    name="$(basename "$source")"
    PATH="$ATFE/bin:$PATH" cmake -S "$source" -B "build/analysis-$name" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/hpclang-armv4t.cmake" \
        -DHPCLANG_ROOT="$ROOT" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
done
python3 tools/analyze.py "$ATFE/bin/clang-check" \
    --host-build build/analysis-host --arm-build build/analysis-e2e --arm-build build/analysis-font-gallery
