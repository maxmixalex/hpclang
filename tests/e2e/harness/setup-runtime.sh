#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: $0 <direct|stock> <hp39|hp48|hp4950>" >&2
    exit 2
fi

MODE="$1"
FAMILY="$2"
packages=(
    libglib2.0-0t64
    libgtk-4-1
    libncurses6
    liblua5.4-0
)

if [[ "$MODE" == stock && ( "$FAMILY" == hp39 || "$FAMILY" == hp48 ) ]]; then
    packages+=(ckermit socat)
fi

if (( ${#packages[@]} == 0 )); then
    exit 0
fi

missing=()
for package in "${packages[@]}"; do
    dpkg-query -W -f='${Status}' "$package" 2>/dev/null |
        grep -q 'ok installed' || missing+=("$package")
done

if (( ${#missing[@]} )); then
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends "${missing[@]}"
fi
