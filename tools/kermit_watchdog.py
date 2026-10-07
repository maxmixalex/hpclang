#!/usr/bin/env python3
"""Bound Stock transfers by lack of acknowledged progress and an absolute ceiling."""

from __future__ import annotations

import argparse
import signal
import subprocess
import time
from pathlib import Path

# Shared by USB and UART. These are failure ceilings, never UI pacing delays.
IDLE_SECONDS = 90
TOTAL_SECONDS = 1200
POLL_SECONDS = 0.1


class Progress:
    """Observe C-Kermit's advancing send window, not traffic or retransmissions."""

    def __init__(self) -> None:
        self.pending = ""
        self.window = None

    def feed(self, chunk: str) -> bool:
        lines = (self.pending + chunk).split("\n")
        self.pending = lines.pop()
        advanced = False
        for line in lines:
            # C-Kermit emits this after accepting the in-window ACK and
            # advancing winlo (modulo 64). Resends do not advance the window.
            if line.startswith("input send rotated send window="):
                value = line.removeprefix("input send rotated send window=").strip()
                if value.isdigit() and 0 <= int(value) < 64:
                    window = int(value)
                    if window != self.window:
                        advanced = True
                        self.window = window
        return advanced


class Watchdog:
    def __init__(self) -> None:
        self.started = self.updated = time.monotonic()
        self.progress = Progress()

    def observe(self, chunk: str) -> None:
        if self.progress.feed(chunk):
            self.updated = time.monotonic()

    def failure(self) -> str | None:
        now = time.monotonic()
        if now - self.started >= TOTAL_SECONDS:
            return f"transfer exceeded absolute ceiling ({TOTAL_SECONDS}s)"
        if now - self.updated >= IDLE_SECONDS:
            return f"no acknowledged transfer progress for {IDLE_SECONDS}s"
        return None


def run(log: Path, command: list[str]) -> int:
    log.unlink(missing_ok=True)
    watchdog = Watchdog()
    position = 0
    with subprocess.Popen(command) as process:
        try:
            while process.poll() is None:
                try:
                    with log.open(encoding="latin-1") as stream:
                        stream.seek(position)
                        watchdog.observe(stream.read())
                        position = stream.tell()
                except FileNotFoundError:
                    pass
                reason = watchdog.failure()
                if reason:
                    print(f"Kermit watchdog: {reason}", flush=True)
                    return 124
                time.sleep(POLL_SECONDS)
            return process.returncode
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    sub.add_parser("timeout")
    run_parser = sub.add_parser("run")
    run_parser.add_argument("log", type=Path)
    run_parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.mode == "timeout":
        print(TOTAL_SECONDS)
        return 0
    if not args.command:
        parser.error("run requires a command")

    # Shell/CI cancellation must also reap the sender rather than orphan it.
    def terminate(signum: int, _frame: object) -> None:
        raise SystemExit(128 + signum)

    signal.signal(signal.SIGTERM, terminate)
    return run(args.log, args.command)


if __name__ == "__main__":
    raise SystemExit(main())
