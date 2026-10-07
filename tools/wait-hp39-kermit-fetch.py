#!/usr/bin/env python3
"""Wait for the HP39/40 Stock serial-disk lazy fetch to finish.

Synchronization uses acknowledged Kermit transactions for the directory and
LIB275L.000. By the time Stock requests and completes that file, the larger
HPGCC000.000 and its named-program dependencies have already completed.
Acknowledged packet progress refreshes the idle watchdog; an independent
absolute ceiling bounds the whole transfer. Neither limit paces UI actions.
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path

from kermit_watchdog import TOTAL_SECONDS, Watchdog

TARGET_FILE = "LIB275L.000"
POLL_SECONDS = 0.1


class Completions:
    """Match a file's successful EOF followed by its transaction acknowledgement."""

    def __init__(self) -> None:
        self.pending = ""
        self.filename = ""
        self.eof = False

    def feed(self, chunk: str) -> list[str]:
        text = self.pending + chunk
        lines = text.split("\n")
        self.pending = lines.pop()
        completed = []
        for line in lines:
            line = line.rstrip("\r")
            if line.startswith("openi name[") and line.endswith("]"):
                self.filename = line[len("openi name[") : -1]
                self.eof = False
            elif line == "<sseof>Y success=1":
                self.eof = True
            elif line.startswith("sseot justone="):
                if self.filename and self.eof:
                    completed.append(self.filename)
                self.filename = ""
                self.eof = False
        return completed


def wait_for_fetch(log_path: Path, marker: Path) -> int:
    watchdog = Watchdog()
    position = 0
    parser = Completions()
    gates = {
        "HP39DIR.000": Path(f"{marker}.directory"),
        TARGET_FILE: marker,
    }
    failed = Path(f"{marker}.failed")
    marker.parent.mkdir(parents=True, exist_ok=True)
    for path in (*gates.values(), failed):
        path.unlink(missing_ok=True)

    print(
        f"waiting for directory and payload transactions: ceiling={TOTAL_SECONDS}s log={log_path}",
        flush=True,
    )
    while True:
        try:
            size = log_path.stat().st_size
            if size < position:
                position = 0
                parser = Completions()
            with log_path.open("r", encoding="latin-1") as stream:
                stream.seek(position)
                chunk = stream.read()
                position = stream.tell()
        except FileNotFoundError:
            chunk = ""
        watchdog.observe(chunk)
        for name in parser.feed(chunk):
            if name in gates:
                gates[name].write_text("complete\n", encoding="ascii")
                print(f"Kermit completed {name} transaction; released {gates[name]}", flush=True)
                if name == TARGET_FILE:
                    return 0
        if reason := watchdog.failure():
            break
        time.sleep(POLL_SECONDS)

    phase = "payload" if gates["HP39DIR.000"].exists() else "directory"
    message = f"{phase} transaction failed: {reason}"
    failed.write_text(message + "\n", encoding="ascii")
    print(message, flush=True)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("timeout")

    wait_parser = sub.add_parser("wait")
    wait_parser.add_argument("debug_log", type=Path)
    wait_parser.add_argument("marker", type=Path)

    args = parser.parse_args()
    if args.command == "timeout":
        print(TOTAL_SECONDS)
        return 0
    return wait_for_fetch(
        args.debug_log,
        args.marker,
    )


if __name__ == "__main__":
    raise SystemExit(main())
