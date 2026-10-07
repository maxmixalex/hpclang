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

# Stock can still be retiring its USB send when Kermit has already parsed the
# request checksum. Give it time to enter receive mode before replying; an
# immediate reply can arrive before Stock enables receive notifications.
# Use Kermit's packet pacing for both transports, not a model-specific UI wait.
# The bundled server also recovers an unanswered Init-Info exchange; pacing
# alone cannot guarantee that Stock observes every receive notification.
# Recover promptly from a lost notification, backing off consecutive timeouts
# in the bundled server rather than exhausting the retry limit in ten seconds.
exec "$KERMIT" \
    -Y -H \
    -l "$TTY" \
    -b 9600 \
    -8 -P -i \
    -C "set carrier-watch off,set modem type none,set flow none,set parity none,set control prefix all,set send packet-length 80,set send pause 50,set send timeout 1 dynamic 1 30,set file type binary,set handshake none,log debug $LOG,server"
