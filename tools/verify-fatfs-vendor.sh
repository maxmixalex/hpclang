#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR="$ROOT/third_party/fatfs"
WORK_INPUT="${1:-$ROOT/build/fatfs-official-verify}"
WORK="$(realpath -m "$WORK_INPUT")"

rm -rf "$WORK"
mkdir -p \
  "$WORK/download" \
  "$WORK/unpacked" \
  "$WORK/unicode" \
  "$WORK/canonical" \
  "$WORK/vendor-normalized" \
  "$WORK/diff"

BASE_URL="https://elm-chan.org/fsw/ff"
curl -fL --retry 3 --retry-delay 2 \
  -o "$WORK/download/ff16.zip" "$BASE_URL/arc/ff16.zip"
curl -fL --retry 3 --retry-delay 2 \
  -o "$WORK/download/ff16p1.diff" "$BASE_URL/patch/ff16p1.diff"
curl -fL --retry 3 --retry-delay 2 \
  -o "$WORK/download/ff16p2.diff" "$BASE_URL/patch/ff16p2.diff"
curl -fL --retry 3 --retry-delay 2 \
  -o "$WORK/download/ffunicode.zip" "$BASE_URL/patch/ffunicode.zip"

unzip -q "$WORK/download/ff16.zip" -d "$WORK/unpacked"
unzip -q "$WORK/download/ffunicode.zip" -d "$WORK/unicode"

for file in ff.c ff.h diskio.h; do
  test -f "$WORK/unpacked/source/$file"
done

UNICODE_SOURCE="$(find "$WORK/unicode" -type f -name ffunicode.c -print -quit)"
test -n "$UNICODE_SOURCE"
test -f "$UNICODE_SOURCE"

# The official archive stores source with CRLF while the official p1/p2 diffs
# are LF. Normalize text line endings only, then apply the official diffs with
# fuzz disabled. This establishes a deterministic source-version comparison
# without treating CRLF vs LF as a code-version difference.
python3 - "$WORK/unpacked/source/ff.c" "$WORK/canonical/ff.c" <<'PY'
from pathlib import Path
import sys
src = Path(sys.argv[1]).read_bytes()
Path(sys.argv[2]).write_bytes(src.replace(b"\r\n", b"\n"))
PY

(
  cd "$WORK/canonical"
  patch --batch --forward --fuzz=0 -p0 ff.c < "$WORK/download/ff16p1.diff"
  patch --batch --forward --fuzz=0 -p0 ff.c < "$WORK/download/ff16p2.diff"
)

for pair in \
  "$VENDOR/ff.c:$WORK/vendor-normalized/ff.c" \
  "$VENDOR/ff.h:$WORK/vendor-normalized/ff.h" \
  "$VENDOR/diskio.h:$WORK/vendor-normalized/diskio.h" \
  "$VENDOR/ffunicode.c:$WORK/vendor-normalized/ffunicode.c" \
  "$WORK/unpacked/source/ff.h:$WORK/canonical/ff.h" \
  "$WORK/unpacked/source/diskio.h:$WORK/canonical/diskio.h" \
  "$UNICODE_SOURCE:$WORK/canonical/ffunicode.c"
do
  src="${pair%%:*}"
  dst="${pair#*:}"
  python3 - "$src" "$dst" <<'PY'
from pathlib import Path
import sys
src = Path(sys.argv[1]).read_bytes()
Path(sys.argv[2]).write_bytes(src.replace(b"\r\n", b"\n"))
PY
done

status=0
for file in ff.c ff.h diskio.h ffunicode.c; do
  if ! cmp -s "$WORK/canonical/$file" "$WORK/vendor-normalized/$file"; then
    status=1
    diff -u "$WORK/canonical/$file" "$WORK/vendor-normalized/$file" \
      > "$WORK/diff/$file.diff" || true
  fi
done

{
  echo "FatFs official vendor verification"
  echo "base:       $BASE_URL/arc/ff16.zip"
  echo "patch-1:    $BASE_URL/patch/ff16p1.diff"
  echo "patch-2:    $BASE_URL/patch/ff16p2.diff"
  echo "unicode:    $BASE_URL/patch/ffunicode.zip"
  echo "comparison: CRLF/LF normalized, patch --fuzz=0"
  echo
  echo "download hashes:"
  sha256sum "$WORK"/download/*
  echo
  echo "canonical source hashes:"
  sha256sum "$WORK"/canonical/*
  echo
  echo "vendor normalized hashes:"
  sha256sum "$WORK"/vendor-normalized/*
} | tee "$WORK/verification.txt"

if (( status != 0 )); then
  echo "FAIL: vendored FatFs source differs from the official R0.16 + p1 + p2 + Unicode replacement baseline." >&2
  exit 1
fi

echo "PASS: vendored FatFs source matches the official R0.16 + p1 + p2 + Unicode replacement baseline."
