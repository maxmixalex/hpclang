#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 4 ]]; then
    echo "usage: $0 <tty> <send-directory> <debug-log> [--remote-host <RPL> | --remote-host-file <setup.json>] <file> [file ...]" >&2
    exit 2
fi

TTY="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SEND_DIR="$(realpath "$2")"
LOG="$(realpath -m "$3")"
shift 3

remote_commands=()
while [[ "${1:-}" == --remote-host || "${1:-}" == --remote-host-file ]]; do
    [[ $# -ge 3 && -n "$2" && "$2" != *,* ]] || {
        echo "remote setup requires a nonempty comma-free argument and at least one file" >&2
        exit 2
    }
    if [[ "$1" == --remote-host-file ]]; then
        setup_text="$(python3 "$ROOT/tools/stock-setup.py" "$2")"
        mapfile -t setup_commands <<< "$setup_text"
        remote_commands+=("${setup_commands[@]}")
    else
        remote_commands+=("$2")
    fi
    shift 2
done

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
    # The calculator is already in Kermit receive/server mode. C-Kermit
    # otherwise prefixes each SEND with its remote-shell auto-upload string
    # ("kermit -ir"), which is not protocol data and can poison the HP USB
    # receive state when it shares one bulk OUT packet with the Send-Init.
    "set protocol kermit {} {} {}"
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
    commands+=("if failure exit 1")
done

for command in "${remote_commands[@]}"; do
    commands+=("remote host $command" "if failure exit 1")
done
commands+=("finish" "if failure exit 1" "exit 0")
command_list="$(IFS=,; echo "${commands[*]}")"

exec "$KERMIT" \
    -Y -H \
    -l "$TTY" \
    -b 9600 \
    -8 -P -i \
    -C "$command_list"
