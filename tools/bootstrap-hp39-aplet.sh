#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CACHE="${HP39_APLET_CACHE:-$ROOT/.cache/hp39-aplet}"
PKG="$CACHE/HPGCC-1.1-p1_with_39G_support_WinSetup.exe"
RAW="$CACHE/raw"
SDK="$CACHE/sdk"

URL='https://sourceforge.net/projects/hpgcc/files/hpgcc/HPGCC-1.1%20with%2039G%20support%20for%20Windows/HPGCC-1.1-p1_with_39G_support_WinSetup.exe/download'

mkdir -p "$CACHE"

if [[ -f "$SDK/.hpclang-prepared" &&
      -f "$SDK/src/hp2aplet.c" &&
      -f "$SDK/data/LIB275L.L39" ]]; then
    echo "$SDK"
    exit 0
fi

if [[ ! -f "$PKG" ]]; then
    curl -fL --retry 3 --retry-delay 2 "$URL" -o "$PKG"
fi

rm -rf "$RAW" "$SDK"
mkdir -p "$RAW" "$SDK"

innoextract -s -d "$RAW" "$PKG" >/dev/null
python3 "$ROOT/tools/prepare-hp39-aplet.py" "$RAW" "$SDK"
touch "$SDK/.hpclang-prepared"

echo "$SDK"
