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
    dynamic_export_files=(
        tests/e2e/harness/run-direct.sh
        tests/e2e/harness/run-gallery.sh
        tests/e2e/harness/run-stock.sh
        tests/e2e/harness/run-stock-gallery-hp39.sh
        tests/e2e/harness/run-stock-gallery-hp48gii.sh
        tests/e2e/harness/run-stock-gallery-hp50g.sh
    )
    regular_shell_files=()
    dynamic_shell_files=()
    for path in "${shell_files[@]}"; do
        dynamic=0
        for allowed in "${dynamic_export_files[@]}"; do
            if [[ "$path" == "$allowed" ]]; then
                dynamic=1
                break
            fi
        done
        if ((dynamic)); then
            dynamic_shell_files+=("$path")
        else
            regular_shell_files+=("$path")
        fi
    done

    if ((${#regular_shell_files[@]})); then
        shellcheck -S warning "${regular_shell_files[@]}"
    fi
    if ((${#dynamic_shell_files[@]})); then
        # These loaders intentionally export variable names emitted by the
        # validated e2e-target-env.py helper. Keep SC2163 enabled everywhere else.
        shellcheck -S warning -e SC2163 "${dynamic_shell_files[@]}"
    fi
fi
