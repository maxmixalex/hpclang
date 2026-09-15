#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: probe-hp39-live-nor.py <state-flash>")

path = Path(sys.argv[1])
packed = path.read_bytes()
if len(packed) != 0x200000:
    raise SystemExit(
        f"{path}: expected 0x200000-byte SST29VF160 image, got 0x{len(packed):x}"
    )

nibs = bytearray(len(packed) * 2)
for index, value in enumerate(packed):
    nibs[index * 2] = value & 0x0F
    nibs[index * 2 + 1] = value >> 4

samples = "PAS51"
main_raw = {
    "P": "f01111f010101000",
    "A": "e01111f111111100",
    "S": "e01110e00111e000",
    "5": "f110f0010111e000",
    "1": "406040404040e000",
}
mini_raw = {
    "P": "47050701010",
    "A": "42050705050",
    "S": "46010204030",
    "5": "47010704070",
    "1": "43020202070",
}


def raw_values(text: str) -> bytes:
    return bytes(int(ch, 16) for ch in text)


def record(base: int, code: int, stride: int) -> bytearray:
    start = base + code * stride
    return nibs[start:start + stride]


def exact_candidates(raw_map: dict[str, str], stride: int) -> list[int]:
    anchor = raw_values(raw_map["A"])
    haystack = bytes(nibs)
    result: list[int] = []
    start = 0
    while True:
        pos = haystack.find(anchor, start)
        if pos < 0:
            break
        base = pos - ord("A") * stride
        if base >= 0 and all(
            record(base, ord(ch), stride) == raw_values(raw_map[ch])
            for ch in samples
        ):
            result.append(base)
        start = pos + 1
    return result


def describe(base: int) -> str:
    byte = base >> 1
    parity = "high" if base & 1 else "low"
    return (
        f"table_nibble=0x{base:08x} physical_byte=0x{byte:08x} "
        f"parity={parity} physical_page=0x{byte & ~0xfff:08x}"
    )


print(f"HP39 LIVE NOR FONT PROBE file={path} bytes=0x{len(packed):x}")
for label, raw_map, stride in (
    ("main-exact", main_raw, 16),
    ("mini-exact", mini_raw, 11),
):
    candidates = exact_candidates(raw_map, stride)
    print(f"{label} candidates={len(candidates)}")
    for base in candidates:
        print(f"  {describe(base)}")
