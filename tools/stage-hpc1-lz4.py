#!/usr/bin/env python3
"""Pack one HPC1 image body for launcher-private LZ4 staging.

The public HPC1 format is unchanged.  HPZ1 is an internal ARMToolbox staging
container: it keeps the original 16-byte HPC1 header verbatim and compresses
only the image body as one raw LZ4 block.  stage1 expands that block directly
into its final temporary code pages.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

HPC1_HEADER = struct.Struct("<4sIII")
HPZ1_HEADER = struct.Struct("<4s4sIIII")
HPZ1_MAGIC = b"HPZ1"
MIN_MATCH = 4
LAST_LITERALS = 5
MATCH_FIND_LIMIT = 12
MAX_OFFSET = 0xFFFF


def _emit_length(out: bytearray, value: int) -> None:
    while value >= 255:
        out.append(255)
        value -= 255
    out.append(value)


def compress_lz4_block(data: bytes) -> bytes:
    """Return a deterministic raw LZ4 block using a greedy latest-match pass."""
    out = bytearray()
    latest: dict[int, int] = {}
    size = len(data)
    anchor = 0
    pos = 0
    match_end = max(0, size - LAST_LITERALS)
    match_start_end = max(0, size - MATCH_FIND_LIMIT)

    while pos + MIN_MATCH <= size and pos <= match_start_end:
        key = int.from_bytes(data[pos : pos + MIN_MATCH], "little")
        previous = latest.get(key)
        latest[key] = pos
        if previous is None or pos - previous > MAX_OFFSET:
            pos += 1
            continue

        maximum = match_end - pos
        if maximum < MIN_MATCH:
            pos += 1
            continue

        match = MIN_MATCH
        while match < maximum and data[previous + match] == data[pos + match]:
            match += 1

        literal_length = pos - anchor
        encoded_match_length = match - MIN_MATCH
        token_at = len(out)
        out.append(0)
        out[token_at] = (
            min(literal_length, 15) << 4
        ) | min(encoded_match_length, 15)

        if literal_length >= 15:
            _emit_length(out, literal_length - 15)
        out.extend(data[anchor:pos])
        out.extend(struct.pack("<H", pos - previous))
        if encoded_match_length >= 15:
            _emit_length(out, encoded_match_length - 15)

        end = pos + match
        update = pos + 1
        update_end = min(end, match_start_end + 1)
        while update < update_end and update + MIN_MATCH <= size:
            key = int.from_bytes(data[update : update + MIN_MATCH], "little")
            latest[key] = update
            update += 1

        pos = end
        anchor = end

    literal_length = size - anchor
    out.append(min(literal_length, 15) << 4)
    if literal_length >= 15:
        _emit_length(out, literal_length - 15)
    out.extend(data[anchor:])
    return bytes(out)


def _read_length(data: bytes, pos: int, base: int) -> tuple[int, int]:
    value = base
    if base == 15:
        while True:
            if pos >= len(data):
                raise ValueError("truncated LZ4 length")
            byte = data[pos]
            pos += 1
            value += byte
            if byte != 255:
                break
    return value, pos


def decompress_lz4_block(data: bytes, output_size: int) -> bytes:
    """Reference decoder used to self-check every generated HPZ1 blob."""
    output = bytearray()
    pos = 0

    while pos < len(data):
        token = data[pos]
        pos += 1
        literal_length, pos = _read_length(data, pos, token >> 4)
        if pos + literal_length > len(data):
            raise ValueError("truncated LZ4 literals")
        if len(output) + literal_length > output_size:
            raise ValueError("LZ4 literals exceed expected output")
        output.extend(data[pos : pos + literal_length])
        pos += literal_length

        if pos == len(data):
            break
        if pos + 2 > len(data):
            raise ValueError("truncated LZ4 offset")
        offset = data[pos] | (data[pos + 1] << 8)
        pos += 2
        if offset == 0 or offset > len(output):
            raise ValueError("invalid LZ4 offset")

        match_length, pos = _read_length(data, pos, token & 0x0F)
        match_length += MIN_MATCH
        if len(output) + match_length > output_size:
            raise ValueError("LZ4 match exceeds expected output")

        source = len(output) - offset
        for _ in range(match_length):
            output.append(output[source])
            source += 1

    if len(output) != output_size:
        raise ValueError(
            f"LZ4 output size mismatch: got {len(output)}, expected {output_size}"
        )
    return bytes(output)


def pack(image: bytes) -> bytes:
    if len(image) < HPC1_HEADER.size:
        raise ValueError("input is shorter than an HPC1 header")

    magic, image_size, entry_offset, rw_static_size = HPC1_HEADER.unpack_from(image)
    if magic != b"HPC1":
        raise ValueError("input is not an HPC1 image")

    body = image[HPC1_HEADER.size :]
    if len(body) != image_size:
        raise ValueError(
            f"HPC1 image_size mismatch: header={image_size}, body={len(body)}"
        )

    compressed = compress_lz4_block(body)
    if decompress_lz4_block(compressed, image_size) != body:
        raise ValueError("internal LZ4 verification failed")
    if len(compressed) >= len(body):
        raise ValueError("LZ4 staging did not reduce the HPC1 body")

    return HPZ1_HEADER.pack(
        HPZ1_MAGIC,
        magic,
        image_size,
        entry_offset,
        rw_static_size,
        len(compressed),
    ) + compressed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    image = args.input.read_bytes()
    staged = pack(image)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(staged)

    packed_size = len(staged) - HPZ1_HEADER.size
    body_size = len(image) - HPC1_HEADER.size
    print(
        f"{args.output}: raw_body={body_size} bytes, "
        f"lz4={packed_size} bytes, staged={len(staged)} bytes, "
        f"ratio={packed_size / body_size:.3f}"
    )


if __name__ == "__main__":
    main()
