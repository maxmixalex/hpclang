#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "usage: $0 <tty> <served-directory> <debug-log>" >&2
    exit 2
fi

TTY="$1"
SERVE="$(realpath "$2")"
LOG="$(realpath -m "$3")"

if command -v ckermit >/dev/null 2>&1; then
    KERMIT="$(command -v ckermit)"
elif command -v kermit >/dev/null 2>&1; then
    KERMIT="$(command -v kermit)"
else
    echo "C-Kermit is not installed" >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG")"

# Stock's serial-disk import also contracts doubled backslashes, including in
# HP39BinB data. Quote every backslash in a private transfer copy. Preserve the
# original Aplet/library files for installation tools that do not use this path.
STAGED="$(mktemp -d)"
SERVER_PID=""
cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$STAGED"
}
trap cleanup EXIT
trap 'exit 143' TERM
trap 'exit 130' INT
python3 - "$SERVE" "$STAGED" <<'PY'
from pathlib import Path
import sys

source, destination = map(Path, sys.argv[1:])
for path in source.iterdir():
    if path.is_file():
        (destination / path.name).write_bytes(path.read_bytes().replace(b"\\", b"\\\\"))
PY
cd "$STAGED"

# Stock can still be retiring its USB send when Kermit has already parsed the
# request checksum. Give it time to enter receive mode before replying; an
# immediate reply can arrive before Stock enables receive notifications.
# Use Kermit's packet pacing for both transports, not a model-specific UI wait.
# The bundled server also recovers an unanswered Init-Info exchange; pacing
# alone cannot guarantee that Stock observes every receive notification.
# Recover promptly from a lost notification, backing off consecutive timeouts
# in the bundled server rather than exhausting the retry limit in ten seconds.
# HP39's serial-disk client removes CR from each decoded CRLF, including inside
# HP39BinB objects. Send in transparent eight-bit text mode with LF as the local
# line ending: Kermit inserts one CR before every LF and Stock removes that CR.
# Manual mode prevents filename/content heuristics from disabling this inverse
# conversion. The source bundle itself remains byte-for-byte unchanged.
"$KERMIT" \
    -Y -H \
    -l "$TTY" \
    -b 9600 \
    -8 -P \
    -C "set carrier-watch off,set modem type none,set flow none,set parity none,set control prefix all,set send packet-length 80,set send pause 50,set send timeout 1 dynamic 1 30,set transfer mode manual,set file type text,set file eol lf,set file bytesize 8,set transfer character-set transparent,set handshake none,log debug $LOG,server" &
SERVER_PID=$!
wait "$SERVER_PID"
SERVER_PID=""
