#!/usr/bin/env python3
"""Build the nested classic-RPL directory used by the Stock mutation witness."""

from pathlib import Path
import sys


def word(value: int, width: int) -> list[int]:
    return [(value >> (4 * i)) & 15 for i in range(width)]


def string(value: bytes) -> list[int]:
    return (
        word(0x02A2C, 5)
        + word(5 + 2 * len(value), 5)
        + [nibble for byte in value for nibble in word(byte, 2)]
    )


def directory(entries: list[tuple[bytes, list[int]]]) -> list[int]:
    # DORRP, attached-library count, newest-entry relative link. Entries carry
    # a backward link, length/name/repeated length, then the native object.
    data = word(0x02A96, 5) + word(0, 3) + word(0, 5)
    previous = 0
    for name, value in entries:
        link = len(data)
        item = link + 5
        data += word(link - previous if previous else 0, 5)
        data += word(len(name), 2)
        data += [nibble for byte in name for nibble in word(byte, 2)]
        data += word(len(name), 2) + value
        previous = item
    data[8:13] = word(previous - 8 if previous else 0, 5)
    return data


def main() -> None:
    destination = Path(sys.argv[1])
    destination.mkdir(parents=True, exist_ok=True)
    nested = directory([(b"VALUE", string(b"NESTED")), (b"KEEP", string(b"KEEP"))])
    data = directory([(b"INNER", nested), (b"OUTSIDE", string(b"OUTSIDE"))])
    if len(data) % 2:
        data.append(0)
    packed = bytes(data[i] | (data[i + 1] << 4) for i in range(0, len(data), 2))
    (destination / "ZZM7DIR.hp").write_bytes(b"HPHP49-C" + packed)


if __name__ == "__main__":
    main()
