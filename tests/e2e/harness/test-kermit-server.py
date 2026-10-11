#!/usr/bin/env python3
"""Exercise the bundled Kermit server over a real PTY, without an emulator."""

from __future__ import annotations

import argparse
import os
import pty
import select
import subprocess
import tempfile
import time
import tty
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PARAMETERS = b"~* @-#Y3"


def check_bytes(body: bytes, kind: int) -> bytes:
    if kind == 1:
        total = sum(body)
        return bytes([32 + ((total + ((total & 192) >> 6)) & 63)])
    crc = 0
    for byte in body:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x8408 if crc & 1 else 0)
    return bytes([32 + (crc >> 12), 32 + ((crc >> 6) & 63), 32 + (crc & 63)])


def packet(kind: bytes, data: bytes = b"", sequence: int = 0, check: int = 1) -> bytes:
    body = bytes([len(data) + check + 34, sequence + 32]) + kind + data
    return b"\x01" + body + check_bytes(body, check) + b"\r"


def unquote(data: bytes) -> bytes:
    result = bytearray()
    index = 0
    while index < len(data):
        byte = data[index]
        index += 1
        if byte == ord("#"):
            if index == len(data):
                raise AssertionError("truncated control quote")
            byte = data[index]
            index += 1
            if 63 <= (byte & 127) <= 95:
                byte ^= 64
        result.append(byte)
    return bytes(result)


class Peer:
    def __init__(self, descriptor: int):
        self.descriptor = descriptor
        self.buffer = bytearray()
        self.acknowledged_data: bytes | None = None

    def send(self, wire: bytes) -> None:
        while wire:
            wire = wire[os.write(self.descriptor, wire) :]

    def receive(self, timeout: float = 3.0) -> bytes:
        deadline = time.monotonic() + timeout
        while True:
            if b"\r" in self.buffer:
                end = self.buffer.index(13) + 1
                wire = bytes(self.buffer[:end])
                del self.buffer[:end]
                if not wire.startswith(b"\x01") or len(wire) != wire[1] - 32 + 3:
                    raise AssertionError(f"invalid packet framing: {wire!r}")
                return wire
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.descriptor], [], [], remaining)[0]:
                return b""
            self.buffer.extend(os.read(self.descriptor, 4096))

    def expect(self, kind: bytes, check: int = 1, sequence: int = 0) -> bytes:
        wire = self.receive()
        # A retry can already be in flight when we send the ACK. Accept only
        # the exact previously acknowledged data packet (including its CRC),
        # and ACK it again without appending the payload twice.
        for _ in range(8):
            if wire != self.acknowledged_data:
                break
            self.send(packet(b"Y", sequence=wire[2] - 32, check=3))
            wire = self.receive()
        if not wire:
            raise AssertionError(f"server did not send {kind!r} within 3 seconds")
        if wire[2:4] != bytes([sequence + 32]) + kind:
            raise AssertionError(f"unexpected packet: {wire!r}")
        if wire[-check - 1 : -1] != check_bytes(wire[1 : -check - 1], check):
            raise AssertionError(f"invalid block check: {wire!r}")
        return wire[4 : -check - 1]

    def negotiate(self) -> None:
        self.send(packet(b"I", PARAMETERS))
        self.expect(b"Y")

    def fetch(
        self, name: bytes, expected: bytes, *, lose_data_ack: bool, delay_data_ack: float = 0
    ) -> None:
        self.acknowledged_data = None
        self.send(packet(b"R", name))
        self.expect(b"S")
        self.send(packet(b"Y", b"~* @-# 3"))
        self.expect(b"F", 3, 1)
        self.send(packet(b"Y", sequence=1, check=3))
        received = bytearray()
        sequence = 2
        # Stock's serial-disk receiver contracts decoded CRLF to LF even for
        # binary HP39BinB objects. Include the wire expansion in the bound, then
        # apply the receiver conversion across packet boundaries below.
        wire_length = len(expected) + expected.count(b"\n")
        while len(received) < wire_length:
            data = self.expect(b"D", 3, sequence)
            if lose_data_ack:
                # Deliberately lose an ACK. The retransmission must preserve
                # both sequence and bytes, rather than advancing the file.
                if self.expect(b"D", 3, sequence) != data:
                    raise AssertionError("retransmission changed the data")
                lose_data_ack = False
            if delay_data_ack:
                # Stock can remain busy longer than the old fixed timeout's
                # entire retry budget. Drain retries, but acknowledge only
                # after that interval, then verify the rest of the transfer.
                deadline = time.monotonic() + delay_data_ack
                retries = 0
                while (remaining := deadline - time.monotonic()) > 0:
                    wire = self.receive(remaining)
                    if wire and wire != packet(b"D", data, sequence, 3):
                        raise AssertionError(f"server abandoned delayed ACK: {wire!r}")
                    retries += bool(wire)
                if not 1 <= retries <= 4:
                    raise AssertionError(f"expected bounded retry backoff, got {retries} retries")
                delay_data_ack = 0
            received.extend(unquote(data))
            self.acknowledged_data = packet(b"D", data, sequence, 3)
            self.send(packet(b"Y", sequence=sequence, check=3))
            sequence = (sequence + 1) % 64
        if bytes(received).replace(b"\r\n", b"\n") != expected:
            raise AssertionError("binary payload differs from the served file")
        self.expect(b"Z", 3, sequence)
        self.send(packet(b"Y", sequence=sequence, check=3))
        sequence = (sequence + 1) % 64
        self.expect(b"B", 3, sequence)
        self.send(packet(b"Y", sequence=sequence, check=3))


def verify(tools: Path) -> None:
    if not (tools / "ckermit").is_file():
        raise SystemExit(f"missing bundled C-Kermit: {tools}")
    environment = dict(os.environ, PATH=f"{tools.resolve()}{os.pathsep}{os.environ['PATH']}")
    with tempfile.TemporaryDirectory(prefix="hpclang-kermit-test-") as directory:
        root = Path(directory)
        # The byte census alone lacks CRLF adjacency. These runs reproduce the
        # deterministic one-byte loss that made a valid Aplet unloadable while
        # the old binary-mode server reported successful delivery.
        payload = bytes(range(256)) * 2 + b"\r\n\r\r\n\n\r\r\r\r\n\n" * 16
        (root / "FIRST.BIN").write_bytes(payload)
        (root / "NEXT.BIN").write_bytes(b"next transaction\x00\xff")
        master, slave = pty.openpty()
        tty.setraw(master)
        tty.setraw(slave)
        try:
            with (root / "process.log").open("wb") as output:
                process = subprocess.Popen(
                    [
                        "bash",
                        str(ROOT / "tools/run-hp39-kermit-server.sh"),
                        os.ttyname(slave),
                        str(root),
                        str(root / "debug.log"),
                    ],
                    env=environment,
                    stdout=output,
                    stderr=subprocess.STDOUT,
                )
                try:
                    peer = Peer(master)
                    ready_deadline = time.monotonic() + 5
                    debug_log = root / "debug.log"
                    while not (
                        debug_log.exists() and b"myfillbuf calling read()" in debug_log.read_bytes()
                    ):
                        if process.poll() is not None or time.monotonic() >= ready_deadline:
                            raise AssertionError("server did not open the protocol reader")
                        time.sleep(0.01)
                    # An idle server must not enqueue unsolicited errors while
                    # the calculator is still booting or opening its USB port.
                    if peer.receive(1.2):
                        raise AssertionError("server sent a packet before the session")
                    if process.poll() is not None:
                        raise AssertionError("server exited before negotiation")
                    peer.negotiate()
                    # Model Stock missing the init reply notification. The
                    # server must recover while waiting for the next request.
                    peer.expect(b"N")
                    peer.negotiate()
                    peer.fetch(b"FIRST.BIN", payload, lose_data_ack=True)
                    if peer.receive(1.2):
                        raise AssertionError("server did not return to silent command wait")
                    peer.negotiate()
                    peer.fetch(
                        b"NEXT.BIN",
                        b"next transaction\x00\xff",
                        lose_data_ack=False,
                        delay_data_ack=15,
                    )
                    if peer.receive(1.2):
                        raise AssertionError("server did not return to silent command wait")
                    peer.negotiate()
                    peer.fetch(b"FIRST.BIN", payload, lose_data_ack=True)
                except Exception:
                    output.flush()
                    print((root / "process.log").read_text(errors="replace")[-2000:])
                    if (root / "debug.log").exists():
                        print((root / "debug.log").read_text(errors="replace")[-4000:])
                    raise
                finally:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
        finally:
            os.close(master)
            os.close(slave)
    print("PASS: Kermit idle, recovery, CRC, byte-exact Stock decoding, retries, and next file")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tools", type=Path, help="directory containing bundled ckermit")
    verify(parser.parse_args().tools)
