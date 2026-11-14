"""Verify the HP50g Stock allocation experiment and its shared entry/return."""

import argparse
import json
from pathlib import Path
import runpy

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
    require(
        records[0][1] == records[4][1], "small block not reused after release/failure"
    )
    return [
        dict(request_bytes=r[0], pointer=r[1], usable_pages=r[3], released=r[5])
        for r in records
    ]


def verify(artifact, state, baseline):
    allocations = []

    def check(words):
        allocations.extend(check_workspace(words))

    report = entry["verify"](
        artifact, state, baseline, marker_value=0x504F4F4C, workspace_check=check
    )
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
    args = parser.parse_args()
    result = verify(args.artifact, args.state, args.baseline)
    args.report.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
