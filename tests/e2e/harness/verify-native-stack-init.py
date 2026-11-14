#!/usr/bin/env python3
"""Reconcile ROM stack initializers with observations; do not infer reservations."""

import argparse
import hashlib
import json
from pathlib import Path
import struct


ANCHORS = {
    "hp39gplus": 158004,
    "hp39gs": 211184,
    "hp40gs": 215348,
    "hp48gii": 178344,
    "hp49gplus": 258840,
    "hp50g": 258840,
}
# Mode, relative initializer address, CPSR mask, literal stack top. These
# decoded instructions establish tops, not ownership of adjacent memory.
INITIALIZERS = (
    (27, 0, 0xDB, 0x0800057C),
    (23, 12, 0xD7, 0x080005FC),
    (18, 24, 0xD2, 0x0800072C),
    (17, 36, 0xD1, 0x0800077C),
    (19, 52, 0x13, 0x080004FC),
)


def verify(model, flash, trace):
    def word(address):
        if address < 0 or address % 4 or address + 4 > len(flash):
            raise ValueError("ROM instruction/literal outside image")
        return struct.unpack_from("<I", flash, address)[0]

    rows = {}
    for line in trace.splitlines():
        row = json.loads(line)
        fields = (
            "version",
            "mode",
            "region",
            "writes",
            "minimum",
            "maximum",
            "minimum_pc",
            "maximum_pc",
        )
        if not isinstance(row, dict) or any(
            type(row.get(k)) is not int for k in fields
        ):
            raise ValueError("malformed stack-region observation")
        if (
            row["version"] != 1
            or not 0 <= row["mode"] < 32
            or not 0 <= row["region"] < 4096
            or not 0 < row["writes"] < 2**64
            or not 0 <= row["minimum"] <= row["maximum"] < 2**32
            or any(not 0 <= row[k] < 2**32 for k in ("minimum_pc", "maximum_pc"))
            or row["minimum"] >> 20 != row["region"]
            or row["maximum"] >> 20 != row["region"]
        ):
            raise ValueError("invalid stack-region geometry")
        key = row["mode"], row["region"]
        if key in rows:
            raise ValueError("duplicate mode/region")
        if "low_sram" in row:
            low = row["low_sram"]
            if row["region"] != 128:
                raise ValueError("low SRAM observation outside SRAM region")
            if low is not None and (
                not isinstance(low, dict)
                or any(type(low.get(k)) is not int for k in fields[3:])
                or not 0 < low["writes"] <= row["writes"]
                or not 0x08000000 <= low["minimum"] <= low["maximum"] < 0x08001000
                or not row["minimum"]
                <= low["minimum"]
                <= low["maximum"]
                <= row["maximum"]
                or any(not 0 <= low[k] < 2**32 for k in ("minimum_pc", "maximum_pc"))
            ):
                raise ValueError("invalid low SRAM observation")
        rows[key] = row

    anchor = ANCHORS[model]
    results = []
    for mode, offset, flags, expected_top in INITIALIZERS:
        pc = anchor + offset
        instruction = word(pc)
        if (
            instruction & 0xFFFFF000 != 0xE59FD000
            or word(pc - 8) != 0xE3801000 | flags
            or word(pc - 4) != 0xE12FF001
        ):
            raise ValueError(f"unrecognized mode/SP initializer at {pc:#x}")
        literal = pc + 8 + (instruction & 0xFFF)
        top = word(literal)
        if top != expected_top:
            raise ValueError(f"unrecognized stack top for mode {mode}")
        observed = rows.get((mode, top >> 20))
        if observed is None or not observed["minimum"] <= top <= observed["maximum"]:
            raise ValueError(f"missing initializer coverage for mode {mode}")
        low = observed.get("low_sram")
        if "low_sram" in observed and (
            low is None or not low["minimum"] <= top <= low["maximum"]
        ):
            raise ValueError(f"missing low SRAM initializer coverage for mode {mode}")
        results.append(
            dict(
                mode=mode,
                pc=pc,
                literal=literal,
                top=top,
                observed_minimum=observed["minimum"],
                top_minus_observed_minimum=top - observed["minimum"],
                observed_writes=observed["writes"],
                low_sram=low,
            )
        )
    return dict(
        version=1,
        model=model,
        initializers=results,
        flash_sha256=hashlib.sha256(flash).hexdigest(),
        trace_sha256=hashlib.sha256(trace).hexdigest(),
        observations=list(rows.values()),
        scope="ROM initializer reconciliation only. Differences are not reserved capacities; "
        "borrowed stacks, other regions, unexecuted paths and transient memory remain separate.",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", choices=ANCHORS, required=True)
    parser.add_argument("--flash", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = verify(args.model, args.flash.read_bytes(), args.trace.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"native stack initializer check failed: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
