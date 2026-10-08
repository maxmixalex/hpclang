#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
: "${ATFE:?Set ATFE to the pinned Arm Toolchain for Embedded directory}"
cmake -S tests/unit -B build/analysis-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
analysis_args=(--host-build build/analysis-host)
for project in tests/e2e/CMakeLists.txt tests/e2e/*/CMakeLists.txt; do
    source="${project%/CMakeLists.txt}"
    name="$(basename "$source")"
    PATH="$ATFE/bin:$PATH" cmake -S "$source" -B "build/analysis-$name" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/hpclang-armv4t.cmake" \
        -DHPCLANG_ROOT="$ROOT" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    analysis_args+=(--arm-build "build/analysis-$name")
done
python3 tools/analyze.py "$ATFE/bin/clang-check" "${analysis_args[@]}"
