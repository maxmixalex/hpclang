#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mode=format
if [[ "${1:-}" == "--check" ]]; then
    mode=check
    shift
fi

missing=()
command -v clang-format >/dev/null 2>&1 || missing+=(clang-format)
command -v ruff >/dev/null 2>&1 || missing+=(ruff)
if ((${#missing[@]})); then
    printf 'missing formatter tool(s): %s\n' "${missing[*]}" >&2
    exit 2
fi

declare -a requested=("$@")
declare -a native_files=()
declare -a python_files=()

classify() {
    local path="$1"
    [[ -f "$path" ]] || return 0
    case "$path" in
        third_party/*|runtime/fonts/generated/*|build/*|.cache/*)
            return 0
            ;;
        *.c|*.h|*.cc|*.cpp|*.cxx|*.hpp)
            native_files+=("$path")
            ;;
        *.py)
            python_files+=("$path")
            ;;
    esac
}

if ((${#requested[@]})); then
    for path in "${requested[@]}"; do
        classify "$path"
    done
else
    while IFS= read -r path; do
        classify "$path"
    done < <(git ls-files)
fi

if ((${#native_files[@]})); then
    if [[ "$mode" == check ]]; then
        clang-format --dry-run --Werror "${native_files[@]}"
    else
        clang-format -i "${native_files[@]}"
    fi
fi

if ((${#python_files[@]})); then
    if [[ "$mode" == check ]]; then
        ruff format --check "${python_files[@]}"
    else
        ruff format "${python_files[@]}"
    fi
fi
