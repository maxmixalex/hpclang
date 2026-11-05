"""Validate Stock lifecycle SRAM snapshots; report sampled native free capacity, not peaks."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

MODELS = {
    name: i + 1
    for i, name in enumerate(("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g"))
}
FIELDS = {"version", "phase", "model", "time_us", "pc", "cpsr", "base", "bytes"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def allocator_root(rom, model):
    signature = struct.pack("<3I", 0xE2810003, 0xE3C05003, 0xE2854014)
    require(rom.count(signature) == 1, "unrecognized allocator signature")
    start = rom.index(signature) - 0x4C
    require(0 <= start <= len(rom) - 0x158, "truncated allocator")
    digest = hashlib.sha256(rom[start : start + 0x158]).hexdigest()
    expected = (
        "b34c84fce1a31f720682ee404ee5b7dca5572ee9199008a6b33c1e4f298a9a12"
        if model == 2
        else "2b15eda432b4983009cd537704439d7c1159e28ae04b3a6a09bd220497166e54"
    )
    require(digest == expected, "unreviewed allocator implementation")
    instruction = struct.unpack_from("<I", rom, start + 0x58)[0]
    require(instruction & 0xFFFFF000 == 0xE59F6000, "unexpected root-load instruction")
    literal = start + 0x60 + (instruction & 0xFFF)
    require(literal <= len(rom) - 4, "root literal outside ROM")
    return struct.unpack_from("<I", rom, literal)[0], digest


def free_capacity(ram, roots):
    begin, end = 0x08000000, 0x08000000 + len(ram)

    def word(address):
        require(
            address % 4 == 0 and begin <= address <= end - 4,
            "unaligned/out-of-range allocator address",
        )
        return struct.unpack_from("<I", ram, address - begin)[0]

    heaps, spans = [], []
    for index in range(2):
        head, previous, seen, sizes = word(roots + index * 16), 0, set(), []
        while head:
            require(
                head not in seen and begin <= head <= end - 20, "cyclic/out-of-range free block"
            )
            seen.add(head)
            size, flags, following, back = (word(head + offset) for offset in (0, 4, 8, 12))
            require(size % 4 == 0 and size <= end - head - 20, "invalid free extent")
            require(
                flags & 0xF000 == 0 and flags & 0x300 == (index + 1) * 0x100,
                "invalid free block flags",
            )
            require(
                back == previous and word(head + 16 + size) == size, "inconsistent free list/footer"
            )
            spans.append((head, head + size + 20))
            sizes.append(size)
            previous, head = head, following
        heaps.append(
            dict(
                heap=(index + 1) * 0x100,
                blocks=len(sizes),
                usable_bytes=sum(sizes),
                largest_usable_bytes=max(sizes, default=0),
                metadata_bytes=20 * len(sizes),
            )
        )
    spans.sort()
    require(all(a[1] <= b[0] for a, b in zip(spans, spans[1:])), "overlapping free blocks")
    return heaps


def metadata(data, model, phase, size):
    require(type(data) is dict and set(data) == FIELDS, "invalid snapshot schema")
    require(
        all(type(data[key]) is int and data[key] >= 0 for key in FIELDS - {"phase"}),
        "invalid numeric metadata",
    )
    require(data["version"] == 1 and data["model"] == model, "snapshot version/model mismatch")
    require(
        data["phase"] == phase and re.fullmatch(r"[A-Za-z0-9_-]{1,63}", phase),
        "snapshot phase mismatch",
    )
    require(
        data["base"] == 0x08000000
        and data["bytes"] == size == (0x40000 if model == 4 else 0x80000),
        "SRAM extent mismatch",
    )
    require(
        data["pc"] <= 0xFFFFFFFF
        and data["cpsr"] <= 0xFFFFFFFF
        and data["time_us"] <= 0xFFFFFFFFFFFFFFFF,
        "metadata overflow",
    )


def ui_return_capacity(phases, model):
    """Check the scripted UI's native heaps, not all Stock memory ownership."""
    baseline = "fetch-complete" if MODELS[model] <= 3 else "armtoolbox"
    require(baseline in phases, "missing UI pre-entry capacity snapshot")
    before, after = phases[baseline], phases["settled-return"]
    require(
        before["time_us"] < phases["application-pass"]["time_us"],
        "UI baseline must precede application PASS",
    )
    changes = []
    for old, new in zip(before["heaps"], after["heaps"]):
        require(old["heap"] == new["heap"], "native heap identity changed")
        usable = new["usable_bytes"] - old["usable_bytes"]
        largest = new["largest_usable_bytes"] - old["largest_usable_bytes"]
        require(usable >= 0, "UI return lost native free capacity")
        require(largest >= 0, "UI return reduced largest native free block")
        changes.append(dict(heap=old["heap"], usable_delta=usable, largest_delta=largest))
    return dict(
        baseline_phase=baseline,
        returned_phase="settled-return",
        heaps=changes,
        scope="Scripted UI native free capacity and fragmentation regression; not total memory ownership or a peak bound.",
    )


def verify(directory, model, rom_path=None, ui_return=False):
    model_id = MODELS[model]
    status_path = directory / f"{model}-stock.json"
    status = json.loads(status_path.read_text())
    require(
        status.get("status") == "pass"
        and all(status.get(k) is True for k in ("saw_pass", "returned", "return_settled")),
        "Stock execution/settled return did not pass",
    )
    rom = (rom_path or directory / "state/flash").read_bytes()
    roots, fingerprint = allocator_root(rom, model_id)
    snapshots = []
    prefix = model + "-"
    paths = list(directory.glob(prefix + "*.sram"))
    require(paths, "missing SRAM snapshots")
    for path in paths:
        phase = path.stem[len(prefix) :]
        data = json.loads(path.with_suffix(".json").read_text())
        ram = path.read_bytes()
        metadata(data, model_id, phase, len(ram))
        snapshots.append(
            dict(data, sram_sha256=hashlib.sha256(ram).hexdigest(), heaps=free_capacity(ram, roots))
        )
    for sidecar in directory.glob(prefix + "*.json"):
        if sidecar == status_path:
            continue
        data = json.loads(sidecar.read_text())
        if type(data) is dict and "phase" in data:
            require(sidecar.with_suffix(".sram").is_file(), "orphan snapshot sidecar")
    snapshots.sort(key=lambda row: row["time_us"])
    phases = {row["phase"]: row for row in snapshots}
    require(
        "application-pass" in phases and "settled-return" in phases, "missing PASS/return snapshot"
    )
    passed, returned = phases["application-pass"]["time_us"], phases["settled-return"]["time_us"]
    require(
        snapshots[0]["time_us"] < passed < returned and snapshots[-1]["phase"] == "settled-return",
        "invalid lifecycle order",
    )
    report = dict(
        version=1,
        model=model,
        passed=True,
        scope="Sampled native free lists, not total memory or peak admission",
        rom_sha256=hashlib.sha256(rom).hexdigest(),
        status_sha256=hashlib.sha256(status_path.read_bytes()).hexdigest(),
        allocator_code_sha256=fingerprint,
        free_list_roots=roots,
        snapshots=snapshots,
    )
    if ui_return:
        report["ui_return_capacity"] = ui_return_capacity(phases, model)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--model", choices=MODELS, required=True)
    parser.add_argument("--rom", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--ui-return", action="store_true", help="require UI native heap recovery")
    args = parser.parse_args()
    report = verify(args.directory, args.model, args.rom, args.ui_return)
    with args.report.open("x", encoding="utf-8") as output:
        json.dump(report, output, indent=2)
        output.write("\n")
    print(
        f"PASS {args.model}: {len(report['snapshots'])} lifecycle snapshots and native free lists"
    )


if __name__ == "__main__":
    main()
