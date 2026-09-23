#!/usr/bin/env python3
"""Wait for the HP39/40 Stock serial-disk lazy fetch to finish.

Synchronization is based on Kermit's successful EOF acknowledgement for
LIB275L.000. By the time Stock requests and completes that file, the larger
HPGCC000.000 transfer has already completed. The size-derived duration is only
a watchdog ceiling; it is never used to pace calculator UI actions.
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path

TARGET_FILE = "LIB275L.000"
EFFECTIVE_BYTES_PER_SECOND = 128
WATCHDOG_MARGIN_SECONDS = 90
POLL_SECONDS = 0.1


def watchdog_seconds(directory: Path) -> int:
    total = sum(
        (directory / name).stat().st_size
        for name in ("HPGCC000.000", TARGET_FILE)
    )
    return (
        total + EFFECTIVE_BYTES_PER_SECOND - 1
    ) // EFFECTIVE_BYTES_PER_SECOND + WATCHDOG_MARGIN_SECONDS


def wait_for_fetch(log_path: Path, directory: Path, marker: Path) -> int:
    timeout = watchdog_seconds(directory)
    deadline = time.monotonic() + timeout
    position = 0
    buffer = ""
    saw_target = False
    open_token = f"openi name[{TARGET_FILE}]"
    success_token = "<sseof>Y success=1"

    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.unlink(missing_ok=True)

    print(
        f"waiting for {TARGET_FILE} completion: "
        f"watchdog={timeout}s log={log_path}",
        flush=True,
    )

    while time.monotonic() < deadline:
        try:
            size = log_path.stat().st_size
            if size < position:
                position = 0
                buffer = ""
                saw_target = False
            with log_path.open("r", encoding="latin-1") as stream:
                stream.seek(position)
                chunk = stream.read()
                position = stream.tell()
        except FileNotFoundError:
            chunk = ""

        if chunk:
            buffer = (buffer + chunk)[-16384:]
            if not saw_target:
                index = buffer.find(open_token)
                if index >= 0:
                    saw_target = True
                    buffer = buffer[index + len(open_token):]
                    print(f"Kermit opened {TARGET_FILE}", flush=True)
            if saw_target and success_token in buffer:
                marker.write_text("complete\n", encoding="ascii")
                print(
                    f"Kermit completed {TARGET_FILE}; released host gate: "
                    f"{marker}",
                    flush=True,
                )
                return 0

        time.sleep(POLL_SECONDS)

    print(
        f"timed out waiting for successful {TARGET_FILE} EOF "
        f"after {timeout}s",
        flush=True,
    )
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)

    timeout_parser = sub.add_parser("timeout")
    timeout_parser.add_argument("transfer_directory", type=Path)

    wait_parser = sub.add_parser("wait")
    wait_parser.add_argument("debug_log", type=Path)
    wait_parser.add_argument("transfer_directory", type=Path)
    wait_parser.add_argument("marker", type=Path)

    args = parser.parse_args()
    if args.command == "timeout":
        print(watchdog_seconds(args.transfer_directory))
        return 0
    return wait_for_fetch(
        args.debug_log,
        args.transfer_directory,
        args.marker,
    )


if __name__ == "__main__":
    raise SystemExit(main())
