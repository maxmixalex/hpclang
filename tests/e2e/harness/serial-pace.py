#!/usr/bin/env python3
"""Bridge two PTYs while pacing Kermit->calculator bytes at a real baud rate."""

from __future__ import annotations

import argparse
import errno
import os
import pty
import selectors
import signal
import time
import tty


def make_endpoint(link: str) -> tuple[int, int, str]:
    master, slave = pty.openpty()
    tty.setraw(slave)
    slave_name = os.ttyname(slave)
    try:
        os.unlink(link)
    except FileNotFoundError:
        pass
    os.symlink(slave_name, link)
    os.set_blocking(master, False)
    return master, slave, slave_name


def safe_read(fd: int, size: int = 4096) -> bytes:
    try:
        return os.read(fd, size)
    except BlockingIOError:
        return b""
    except OSError as exc:
        if exc.errno == errno.EIO:
            return b""
        raise


def flush_unpaced(fd: int, buffer: bytearray) -> None:
    while buffer:
        try:
            sent = os.write(fd, buffer)
        except BlockingIOError:
            return
        except OSError as exc:
            if exc.errno == errno.EIO:
                return
            raise
        if sent <= 0:
            return
        del buffer[:sent]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Bidirectional PTY bridge with paced Kermit->calculator traffic."
    )
    parser.add_argument("--x50-link", required=True)
    parser.add_argument("--kermit-link", required=True)
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--log", required=True)
    args = parser.parse_args()
    if args.baud <= 0:
        parser.error("--baud must be positive")

    for path in (args.x50_link, args.kermit_link, args.log):
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)

    x_master, x_slave, x_name = make_endpoint(args.x50_link)
    k_master, k_slave, k_name = make_endpoint(args.kermit_link)
    selector = selectors.DefaultSelector()
    selector.register(x_master, selectors.EVENT_READ, "x50")
    selector.register(k_master, selectors.EVENT_READ, "kermit")

    k_to_x = bytearray()
    x_to_k = bytearray()
    # HP39/40 stock serial is 8N1: one start bit, eight data bits, one stop bit.
    seconds_per_byte = 10.0 / float(args.baud)
    next_k_to_x = time.monotonic()
    running = True
    counts = {"kermit_rx": 0, "x50_tx": 0, "x50_rx": 0, "kermit_tx": 0}

    def stop(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    with open(args.log, "w", encoding="utf-8", buffering=1) as log:
        log.write(
            f"serial pace bridge: baud={args.baud} byte_interval_us="
            f"{seconds_per_byte * 1_000_000:.3f}\n"
        )
        log.write(f"x50={args.x50_link} -> {x_name}\n")
        log.write(f"kermit={args.kermit_link} -> {k_name}\n")

        try:
            while running:
                now = time.monotonic()
                timeout = 0.05
                if k_to_x:
                    timeout = min(timeout, max(0.0, next_k_to_x - now))

                for key, _mask in selector.select(timeout):
                    data = safe_read(key.fd)
                    if not data:
                        continue
                    if key.data == "kermit":
                        k_to_x.extend(data)
                        counts["kermit_rx"] += len(data)
                    else:
                        x_to_k.extend(data)
                        counts["x50_rx"] += len(data)

                flush_unpaced(k_master, x_to_k)
                counts["kermit_tx"] = counts["x50_rx"] - len(x_to_k)

                now = time.monotonic()
                if k_to_x and now >= next_k_to_x:
                    try:
                        sent = os.write(x_master, k_to_x[:1])
                    except BlockingIOError:
                        sent = 0
                    except OSError as exc:
                        if exc.errno == errno.EIO:
                            sent = 0
                        else:
                            raise
                    if sent == 1:
                        del k_to_x[:1]
                        counts["x50_tx"] += 1
                        # Do not catch up with a burst after host scheduling stalls.
                        next_k_to_x = now + seconds_per_byte
        finally:
            log.write(
                "stopped: "
                + " ".join(f"{name}={value}" for name, value in counts.items())
                + f" queued_kermit_to_x50={len(k_to_x)}"
                + f" queued_x50_to_kermit={len(x_to_k)}\n"
            )
            selector.close()
            for link in (args.x50_link, args.kermit_link):
                try:
                    os.unlink(link)
                except FileNotFoundError:
                    pass
            for fd in (x_master, x_slave, k_master, k_slave):
                try:
                    os.close(fd)
                except OSError:
                    pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
