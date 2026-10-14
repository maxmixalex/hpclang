"""Exercise the production launcher decoder against independent zlib streams."""

from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import zlib


def main():
    executable = sys.argv[1]
    rng = random.Random(734)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "image.bin"

        def check(packed, size, expected=None):
            path.write_bytes(packed)
            result = subprocess.run([executable, str(path), str(size)], capture_output=True)
            if expected is None:
                assert result.returncode == 1, (result.returncode, result.stderr)
            else:
                assert result.returncode == 0, (result.returncode, result.stderr)
                assert result.stdout == expected

        for data in (
            b"abc" * 4096,
            bytes(range(256)) * 100,
            rng.randbytes(10000),
            rng.randbytes(40000) + bytes(40000),
        ):
            header = struct.pack("<4sIII", b"HPC1", len(data), 0, 4256)
            check(header + data, len(data) + 31, data + bytes(31))
            check(header + data, len(data) - 1)
            check(header + data[:-1], len(data))
            for strategy in (zlib.Z_DEFAULT_STRATEGY, zlib.Z_FIXED, zlib.Z_HUFFMAN_ONLY):
                encoder = zlib.compressobj(level=9, wbits=-15, strategy=strategy)
                packed = encoder.compress(data) + encoder.flush()
                if len(packed) >= len(data):
                    continue
                compressed = b"HPCZ" + header[4:] + packed
                check(compressed, len(data) + 31, data + bytes(31))
                check(compressed, len(data) - 1)
                for cut in (0, 1, len(packed) // 2, len(packed) - 1):
                    check(compressed[: 16 + cut], len(data))
                check(
                    compressed[:4] + struct.pack("<I", len(data) + 1) + compressed[8:],
                    len(data) + 1,
                )
                check(compressed[:4] + struct.pack("<I", len(data) - 1) + compressed[8:], len(data))
        for size in range(17):
            check(bytes(size), 64)
        for magic, size, entry, rw in (
            (b"BAD!", 1, 0, 0),
            (b"HPC1", 0, 0, 0),
            (b"HPCZ", 0xFFFFFFFF, 0, 0),
            (b"HPCZ", 100, 100, 0),
            (b"HPCZ", 100, 0, 0xFFFFFFFF),
        ):
            check(struct.pack("<4sIII", magic, size, entry, rw) + b"x", 128)
        for size in range(1, 129):
            check(
                struct.pack("<4sIII", b"HPCZ", 1024, 0, 0) + bytes([7]) + rng.randbytes(size), 1024
            )
    print("PASS: launcher raw/compressed images, independent streams and invalid bounds")


if __name__ == "__main__":
    main()
