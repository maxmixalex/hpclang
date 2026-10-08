#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 ]]; then
    echo "usage: $0 <tools-directory>" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$(realpath -m "$1")"
PIN="cbe8041bc40c334426a5982254882bd4f13ba1a7"
SOURCE="https://codeload.github.com/KermitProject/ckermit/tar.gz/$PIN"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/hpclang-ckermit.XXXXXX")"
trap 'rm -rf -- "$WORK"' EXIT

curl --fail --location --retry 3 --retry-all-errors "$SOURCE" -o "$WORK/source.tar.gz"
mkdir "$WORK/source"
tar -xzf "$WORK/source.tar.gz" -C "$WORK/source" --strip-components=1
# I-packet negotiation must leave the server receiving commands with an active
# packet timer. Upstream tinit(0) clears that state and disables recovery.
git -C "$WORK/source" apply "$ROOT/tests/e2e/harness/ckermit-server-command-wait.patch"
# Retain rapid first recovery without exhausting retries on a late ACK.
git -C "$WORK/source" apply "$ROOT/tests/e2e/harness/ckermit-send-backoff.patch"
# Fast local links need subsecond samples; retransmitted ACKs are ambiguous.
git -C "$WORK/source" apply "$ROOT/tests/e2e/harness/ckermit-rtt-sampling.patch"
python3 "$ROOT/tests/e2e/harness/test-kermit-timers.py" "$WORK/source/ckcfn2.c"
make -C "$WORK/source" -j2 linux CC=gcc
mkdir -p "$DEST"
install -m 755 "$WORK/source/wermit" "$DEST/kermit"
install -m 644 "$WORK/source/COPYING.TXT" "$DEST/kermit-COPYING.txt"
ln -sfn kermit "$DEST/ckermit"
