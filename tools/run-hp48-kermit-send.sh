#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 4 ]]; then
    echo "usage: $0 <tty> <send-directory> <debug-log> <file> [file ...]" >&2
    exit 2
fi

TTY="$1"
SEND_DIR="$(realpath "$2")"
LOG="$(realpath -m "$3")"
shift 3

if command -v ckermit >/dev/null 2>&1; then
    KERMIT="$(command -v ckermit)"
elif command -v kermit >/dev/null 2>&1; then
    KERMIT="$(command -v kermit)"
else
    echo "C-Kermit is not installed" >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG")"
cd "$SEND_DIR"

commands=(
    "set carrier-watch off"
    "set modem type none"
    "set flow none"
    "set parity none"
    "set control prefix all"
    "set send packet-length 80"
    "set file type binary"
    "set handshake none"
    "log debug $LOG"
)

for spec in "$@"; do
    file="${spec%%=*}"
    as_name=""
    if [[ "$spec" == *=* ]]; then
        as_name="${spec#*=}"
    fi

    [[ "$file" != */* ]] || {
        echo "send file must be relative to send-directory: $file" >&2
        exit 2
    }
    [[ -f "$file" ]] || {
        echo "missing send file: $SEND_DIR/$file" >&2
        exit 1
    }

    if [[ -n "$as_name" ]]; then
        commands+=("send /as-name:$as_name $file")
    else
        commands+=("send $file")
    fi
done

commands+=("finish" "exit")
command_list="$(IFS=,; echo "${commands[*]}")"

exec "$KERMIT" \
    -Y -H \
    -l "$TTY" \
    -b 9600 \
    -8 -P -i \
    -C "$command_list"
