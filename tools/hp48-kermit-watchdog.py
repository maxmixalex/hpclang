#!/usr/bin/env python3
"""Compute a watchdog ceiling for HP48 Stock Kermit sends.

The UI sequence is synchronized by a host-completion marker, not by this
number.  The budget only prevents a genuinely stalled serial transfer from
occupying a CI runner indefinitely.
"""

from __future__ import annotations

import os
import sys

BAUD = 9600
SERIAL_BITS_PER_BYTE = 10  # 8N1
MAX_CONTROL_QUOTING_EXPANSION = 2
PROTOCOL_MARGIN_SECONDS = 60


def main() -> int:
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <file> [file ...]", file=sys.stderr)
        return 2

    total = 0
    for raw in sys.argv[1:]:
        try:
            total += os.path.getsize(raw)
        except OSError as exc:
            print(f"{raw}: {exc}", file=sys.stderr)
            return 1

    wire_bits = total * SERIAL_BITS_PER_BYTE * MAX_CONTROL_QUOTING_EXPANSION
    seconds = (wire_bits + BAUD - 1) // BAUD + PROTOCOL_MARGIN_SECONDS
    print(seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
