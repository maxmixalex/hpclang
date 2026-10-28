"""Audit retained ordinary-Stock configuration probes, not startup safety.

The fixture starts in CFGSCOPE with CFGKEEP in HOME. An attachment is a HOME
record; the child directory's three-nibble field is not its library number.
Upload may add a program to the child, so its whole body need not be identical.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "port-recovery"))
from verify import home_object, home_string, native_words, require_stock_pc


def snapshot(path):
    require_stock_pc(path)
    ram = (path / "sram").read_bytes()
    flash = (path / "flash").read_bytes()
    read = native_words(ram, flash)
    home = read(0x80711)
    child = read(0x8071B)
    assert child != home and read(home) == read(child) == 0x2A96
    assert home_object(read, b"CFGSCOPE") == child
    assert read(child + 5, 3) == 0x7FF
    assert home_string(read, b"CFGKEEP") == b"early-config sentinel"
    top, end = read(0x806F8), read(0x806FD)
    # rpl-stack-view.c excludes the terminating zero cell from stack depth.
    assert end - top == 5 and read(top) == 0
    count = read(home + 5, 3)
    assert count < 128
    attachments = [
        tuple(read(home + 8 + 13 * i + offset, width) for offset, width in ((0, 3), (3, 5), (8, 5)))
        for i in range(count)
    ]
    catalog_count = read(0x8611D, 3)
    assert catalog_count < 128
    catalog = bytes(read(0x8611D + i, 1) for i in range(3 + 16 * catalog_count))
    return {
        "attachments": attachments,
        "catalog": catalog,
        "flash": flash,
        "sram_sha256": hashlib.sha256(ram).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--record", nargs=3, type=lambda value: int(value, 0), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    before, after = snapshot(args.before), snapshot(args.after)
    expected = Counter(before["attachments"])
    expected[tuple(args.record)] += 1
    assert Counter(after["attachments"]) == expected
    assert before["catalog"] == after["catalog"]
    assert before["flash"] == after["flash"]
    result = {
        "before": str(args.before),
        "after": str(args.after),
        "before_sram_sha256": before["sram_sha256"],
        "after_sram_sha256": after["sram_sha256"],
        "flash_sha256": hashlib.sha256(before["flash"]).hexdigest(),
        "added_home_record": args.record,
        "before_home_count": len(before["attachments"]),
        "after_home_count": len(after["attachments"]),
        "catalog_and_flash_unchanged": True,
        "child_header_and_sentinel_preserved": True,
        "stack_empty": True,
        "scope": "Ordinary Stock execution only; no startup or idempotence proof",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
