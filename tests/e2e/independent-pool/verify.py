"""Verify the HP50g Stock allocation experiment and its shared entry/return."""

import argparse
import json
from pathlib import Path
import runpy
import struct

entry = runpy.run_path(str(Path(__file__).parents[1] / "independent-entry/verify.py"))
require = entry["require"]


def check_workspace(words):
    require(words[0] == 0x504F4F4C and words[1] == 0, "pool experiment incomplete")
    records = [words[4 + i * 8 : 12 + i * 8] for i in range(5)]
    require(
        [r[0] for r in records] == [36863, 69631, 135167, 1048576, 36863],
        "unexpected allocation requests",
    )
    for i, record in enumerate(records):
        size, pointer, aligned, pages, descriptor, released, corrupt, cpsr = record
        if not pointer:
            require(i in (2, 3) and not any(record[2:]), "small allocation failed")
            continue
        require(i != 3, "impossible one-MiB allocation on HP50g")
        require(0x08000000 <= pointer < 0x08080000, "allocation outside SRAM")
        require(pointer + size <= 0x08080000, "allocation exceeds SRAM")
        require(aligned == (pointer + 4095) & ~4095, "incorrect page alignment")
        require(pages == (pointer + size - aligned) // 4096, "incorrect page count")
        require(
            descriptor & 3 == 2 and descriptor & 0xFFF00000 == 0x08000000,
            "pool is not in the Stock identity mapping",
        )
        require(released == 0 and corrupt == 0, "release or memory check failed")
        require(cpsr & 255 == 0x10, "Stock caller mode/interrupt state changed")
    require(records[0][1] == records[4][1], "small block not reused after release/failure")
    return [
        dict(request_bytes=r[0], pointer=r[1], usable_pages=r[3], released=r[5]) for r in records
    ]


def check_rpl_workspace(words, ram):
    require(words[0] == 0x52504F4C and words[1] == 0, "RPL borrow incomplete")
    require(words[2] & 255 == 0xD0, "RPL borrow did not mask interrupts")
    cpu, lower, upper, first, end, count, corrupt, unrestored, unchanged = words[3:12]
    require(0x08000000 <= cpu <= 0x0807FC00, "invalid Saturn state")
    require(0x80000 <= lower < upper <= 0x100000, "invalid RPL stack bounds")
    require(first == (lower + 8192 + 8191) & ~8191, "missing lower reserve")
    require(end == (upper - 512) & ~8191, "missing upper reserve")
    require(0 < count <= 64 and count * 8192 == end - first, "invalid RPL pool size")
    require(corrupt == unrestored == 0 and unchanged == 1, "borrow damaged memory or bounds")
    pages = words[12 : 12 + count]
    require(len(set(pages)) == count, "aliased RPL pool pages")
    for i, page in enumerate(pages):
        require(0x08001000 <= page <= 0x0807F000 and page % 4096 == 0, "invalid RPL physical page")
        offset = cpu - 0x08000000 + ((first + i * 8192) >> 12) * 4
        require(
            struct.unpack_from("<2I", ram, offset) == (page, page + 2048),
            "RPL pages disagree with independent saved Read map",
        )
    return dict(
        pages=list(pages),
        rsktop=lower,
        dsktop=upper,
        reserved_nibbles=[first, end],
        borrowed_bytes=count * 4096,
    )


def stock_stack_gap_nibbles(ram, cpu):
    require(0x08000000 <= cpu <= 0x08000000 + len(ram) - 1024, "invalid Saturn map")
    # RSKTOP and DSKTOP are adjacent five-nibble words in the same mapped page.
    # Read the snapshot's current gap independently of the earlier ARM result.
    rsktop = 0x806F3
    physical = struct.unpack_from("<I", ram, cpu - 0x08000000 + (rsktop >> 12) * 4)[0]
    require(
        0x08000000 <= physical <= 0x08000000 + len(ram) - 2048 and physical % 2048 == 0,
        "invalid stack page",
    )
    offset = physical - 0x08000000 + (rsktop & 4095) // 2
    pair = int.from_bytes(ram[offset : offset + 6], "little") >> 4
    lower, upper = pair & 0xFFFFF, (pair >> 20) & 0xFFFFF
    require(0x80000 <= lower <= upper < 0x100000, "invalid saved Stock stack bounds")
    return upper - lower


def check_bitmap_workspace(words, ram, rows=896):
    marker, status, cpu, top, obj, prologue, span, count, first, end = words[:10]
    require(marker == 0x47524F42 and status == 0, "bitmap inspection incomplete")
    require(0x08000000 <= cpu <= 0x0807FC00 and 0x80000 <= top <= 0xFFFFA, "invalid Stock stack")
    require(prologue == 0x2B1E and span == rows * 256 + 15, "unexpected native bitmap")
    require(0x80000 <= obj < obj + 5 + span <= 0x100000, "bitmap outside RPL address space")
    require(
        first == (obj + 20 + 8191) & ~8191 and end == (obj + 5 + span) & ~8191,
        "bitmap bounds changed",
    )
    pages = words[12 : 12 + count]
    require(
        rows // 32 - 1 <= count <= rows // 32 and end - first == count * 8192,
        "invalid bitmap page count",
    )
    require(len(set(pages)) == count, "aliased bitmap pages")
    for i, page in enumerate(pages):
        require(page % 4096 == 0 and 0x08001000 <= page <= 0x0807F000, "invalid bitmap frame")
        offset = cpu - 0x08000000 + ((first + i * 8192) >> 12) * 4
        require(
            struct.unpack_from("<2I", ram, offset) == (page, page + 2048), "bitmap mapping changed"
        )
    return {
        "bitmap_nibble_address": obj,
        "bitmap_object_nibbles": 5 + span,
        "aligned_frame_bytes": len(pages) * 4096,
        "pages": pages,
        "stock_stack_gap_bytes": stock_stack_gap_nibbles(ram, cpu) // 2,
        "saturn_cpu": cpu,
    }


def verify(artifact, state, baseline, *, rpl=False):
    allocations = []
    borrowed = {}

    def check(words):
        if rpl:
            borrowed.update(check_rpl_workspace(words, (state / "sram").read_bytes()))
        else:
            allocations.extend(check_workspace(words))

    report = entry["verify"](
        artifact,
        state,
        baseline,
        marker_value=0x52504F4C if rpl else 0x504F4F4C,
        workspace_check=check,
    )
    if rpl:
        report.update(
            scope="HP50g paused RPL free-page borrow/restore; no native service coexistence",
            borrowed=borrowed,
        )
    else:
        report.update(
            scope="HP50g Stock KOS page allocation/release, not an HPC1 launcher",
            allocations=allocations,
        )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", type=Path, required=True)
    parser.add_argument("--state", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--rpl", action="store_true", help="verify rpl-probe.c instead of KOS")
    args = parser.parse_args()
    result = verify(args.artifact, args.state, args.baseline, rpl=args.rpl)
    args.report.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
