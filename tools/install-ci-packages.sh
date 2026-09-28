#!/usr/bin/env bash
set -euo pipefail

missing=()
for package in "$@"; do
    if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q 'ok installed'; then
        missing+=("$package")
    fi
done
((${#missing[@]})) || exit 0
privilege=()
if (( EUID != 0 )); then privilege=(sudo -E); fi
apt=(apt-get -o Acquire::Retries=4 -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20)
for attempt in 1 2 3; do
    if "${privilege[@]}" "${apt[@]}" update -qq &&
       "${privilege[@]}" "${apt[@]}" install -y --no-install-recommends "${missing[@]}"; then
        exit 0
    fi
    printf 'Package installation attempt %d failed\n' "$attempt" >&2
    sleep "$attempt"
done
exit 1
