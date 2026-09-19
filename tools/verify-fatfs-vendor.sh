#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR="$ROOT/third_party/fatfs"
WORK="${1:-$ROOT/build/fatfs-official-verify}"

rm -rf "$WORK"
mkdir -p "$WORK/download" "$WORK/unpacked" "$WORK/patched"

BASE_URL="https://elm-chan.org/fsw/ff"
curl -fL --retry 3 --retry-delay 2 \
  -o "$WORK/download/ff16.zip" "$BASE_URL/arc/ff16.zip"
curl -fL --retry 3 --retry-delay 2 \
  -o "$WORK/download/ff16p1.diff" "$BASE_URL/patch/ff16p1.diff"
curl -fL --retry 3 --retry-delay 2 \
  -o "$WORK/download/ff16p2.diff" "$BASE_URL/patch/ff16p2.diff"

unzip -q "$WORK/download/ff16.zip" -d "$WORK/unpacked"

for file in ff.c ff.h diskio.h ffunicode.c; do
  test -f "$WORK/unpacked/source/$file"
done

cp "$WORK/unpacked/source/ff.c" "$WORK/patched/ff.c"
(
  cd "$WORK/patched"
  patch --batch --forward -p0 < "$WORK/download/ff16p1.diff"
  patch --batch --forward -p0 < "$WORK/download/ff16p2.diff"
)

cmp "$WORK/patched/ff.c" "$VENDOR/ff.c"
cmp "$WORK/unpacked/source/ff.h" "$VENDOR/ff.h"
cmp "$WORK/unpacked/source/diskio.h" "$VENDOR/diskio.h"
cmp "$WORK/unpacked/source/ffunicode.c" "$VENDOR/ffunicode.c"

{
  echo "FatFs official vendor verification"
  echo "source: $BASE_URL/arc/ff16.zip"
  echo "patch1: $BASE_URL/patch/ff16p1.diff"
  echo "patch2: $BASE_URL/patch/ff16p2.diff"
  echo
  sha256sum \
    "$WORK/download/ff16.zip" \
    "$WORK/download/ff16p1.diff" \
    "$WORK/download/ff16p2.diff" \
    "$WORK/patched/ff.c" \
    "$WORK/unpacked/source/ff.h" \
    "$WORK/unpacked/source/diskio.h" \
    "$WORK/unpacked/source/ffunicode.c"
} | tee "$WORK/verification.txt"

echo "PASS: vendored FatFs core is byte-identical to official R0.16 + patch-1 + patch-2."
