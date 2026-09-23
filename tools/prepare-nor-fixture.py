#!/usr/bin/env python3
"""Install a checked full-NOR fixture as x50ng persistent flash state."""

from __future__ import annotations

import argparse
import hashlib
import lzma
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixture", type=Path)
    parser.add_argument("state_dir", type=Path)
    parser.add_argument("--compression", choices=("none", "xz"), default="none")
    parser.add_argument("--bytes", type=int, required=True)
    parser.add_argument(
        "--state-bytes",
        type=int,
        default=None,
        help="optional emulator backing size; bytes beyond the verified image are 0xff",
    )
    parser.add_argument("--sha256", required=True)
    args = parser.parse_args()

    packed = args.fixture.read_bytes()
    data = lzma.decompress(packed) if args.compression == "xz" else packed

    if len(data) != args.bytes:
        raise SystemExit(
            f"{args.fixture}: expected {args.bytes} bytes after decode, got {len(data)}"
        )

    digest = hashlib.sha256(data).hexdigest()
    if digest.lower() != args.sha256.lower():
        raise SystemExit(
            f"{args.fixture}: SHA-256 mismatch: {digest} != {args.sha256.lower()}"
        )

    if data[:0x4000] == b"\xff" * 0x4000:
        raise SystemExit(f"{args.fixture}: bootloader area is blank")

    state_bytes = args.state_bytes if args.state_bytes is not None else len(data)
    if state_bytes < len(data):
        raise SystemExit(
            f"{args.fixture}: state backing {state_bytes} is smaller than image {len(data)}"
        )
    state = data + b"\xff" * (state_bytes - len(data))

    args.state_dir.mkdir(parents=True, exist_ok=True)
    (args.state_dir / "flash").write_bytes(state)
    print(f"flash={args.state_dir / 'flash'}")
    print(f"image_bytes={len(data)}")
    print(f"state_bytes={len(state)}")
    print(f"sha256={digest}")


if __name__ == "__main__":
    main()
