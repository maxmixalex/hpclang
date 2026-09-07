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
cd "$SERVE"

exec "$KERMIT" \
    -Y -H \
    -l "$TTY" \
    -b 9600 \
    -8 -P -i \
    -C "set carrier-watch off,set modem type none,set flow none,set parity none,set control prefix all,set send packet-length 80,set file type binary,set handshake none,log debug $LOG,server"
