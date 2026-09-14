#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: probe-hp39-stock-font-tables.py <firmware.bin>")

path = Path(sys.argv[1])
packed = path.read_bytes()
nibs = bytearray(len(packed) * 2)
j = 0
for value in packed:
    nibs[j] = value & 0x0F
    nibs[j + 1] = value >> 4
    j += 2

samples = "PAS51"
main_raw = {
    "P": "f01111f010101000",
    "A": "e01111f111111100",
    "S": "e01110e00111e000",
    "5": "f110f0010111e000",
    "1": "406040404040e000",
}
main_rows = {
    "P": [0x0F, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01, 0x00],
    "A": [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00],
    "S": [0x0E, 0x11, 0x01, 0x0E, 0x10, 0x11, 0x0E, 0x00],
    "5": [0x1F, 0x01, 0x0F, 0x10, 0x10, 0x11, 0x0E, 0x00],
    "1": [0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00],
}
mini_raw = {
    "P": "47050701010",
    "A": "42050705050",
    "S": "46010204030",
    "5": "47010704070",
    "1": "43020202070",
}

def raw_values(text):
    return bytes(int(ch, 16) for ch in text)

def record(base, code, stride):
    start = base + code * stride
    return nibs[start:start + stride]

def exact_candidates(raw_map, stride):
    anchor = raw_values(raw_map["A"])
    out = []
    start = 0
    while True:
        pos = bytes(nibs).find(anchor, start)
        if pos < 0:
            break
        base = pos - ord("A") * stride
        if base >= 0 and all(record(base, ord(ch), stride) == raw_values(raw_map[ch]) for ch in samples):
            out.append(base)
        start = pos + 1
    return out

def main_visible_match(pos, rows):
    if pos < 0 or pos + 16 > len(nibs):
        return False
    for y, expected in enumerate(rows):
        value = nibs[pos + y * 2] | (nibs[pos + y * 2 + 1] << 4)
        if (value & 0x1F) != expected:
            return False
    return True

def visible_main_candidates():
    out = []
    stride = 16
    max_start = len(nibs) - 256 * stride
    for base in range(max_start + 1):
        apos = base + ord("A") * stride
        if not main_visible_match(apos, main_rows["A"]):
            continue
        if all(main_visible_match(base + ord(ch) * stride, main_rows[ch]) for ch in samples):
            out.append(base)
    return out

def fmt_base(base):
    file_byte = base >> 1
    nor_byte = 0x4000 + file_byte
    parity = base & 1
    return f"nibble=0x{base:08x} file_byte=0x{file_byte:08x} parity={parity} nor_byte=0x{nor_byte:08x} nor_page=0x{nor_byte & ~0xfff:08x}"

print(f"HP39 STOCK FONT TABLE PROBE file={path.name} bytes=0x{len(packed):x} nibbles=0x{len(nibs):x}")
for label, raw_map, stride in (("main-exact", main_raw, 16), ("mini-exact", mini_raw, 11)):
    candidates = exact_candidates(raw_map, stride)
    print(f"{label} candidates={len(candidates)}")
    for base in candidates[:32]:
        print(f"  {fmt_base(base)}")

candidates = visible_main_candidates()
print(f"main-visible-low5 candidates={len(candidates)}")
for base in candidates[:32]:
    print(f"  {fmt_base(base)}")

# Also report what the old HP39/40G logical addresses contain in this update
# file when interpreted directly, to make cross-firmware mismatches explicit.
for label, base, stride in (("legacy-main", 0x07c651, 16), ("legacy-mini", 0x07e695, 11)):
    values = []
    for ch in samples:
        r = record(base, ord(ch), stride)
        values.append("".join(format(v, "x") for v in r) if len(r) == stride else "<oob>")
    print(f"{label}[P,A,S,5,1]={','.join(values)}")
