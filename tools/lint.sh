#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

missing=()
command -v ruff >/dev/null 2>&1 || missing+=(ruff)
command -v shellcheck >/dev/null 2>&1 || missing+=(shellcheck)
if ((${#missing[@]})); then
    printf 'missing lint tool(s): %s\n' "${missing[*]}" >&2
    exit 2
fi

mapfile -t python_files < <(
    git ls-files '*.py' \
        ':!:third_party/**' \
        ':!:runtime/fonts/generated/**'
)
if ((${#python_files[@]})); then
    ruff check "${python_files[@]}"
fi

mapfile -t shell_files < <(git ls-files '*.sh')
if ((${#shell_files[@]})); then
    shellcheck -S warning "${shell_files[@]}"
fi
