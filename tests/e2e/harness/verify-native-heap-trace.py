"""Validate qualified allocator observations; not whole-system peak acceptance."""

import argparse
import json
from pathlib import Path
import runpy

PROFILES = runpy.run_path(str(Path(__file__).with_name("verify-resource-snapshots.py")))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def counter(value):
    return type(value) is int and 0 <= value <= 0xFFFFFFFFFFFFFFFF


def capacity(heaps, available):
    require(isinstance(heaps, list) and len(heaps) == 2, "expected two native heaps")
    total = 0
    for heap in heaps:
        require(isinstance(heap, dict), "invalid heap capacity")
        require(set(heap) == {"usable", "largest", "metadata", "blocks"}, "invalid capacity fields")
        require(all(counter(value) for value in heap.values()), "invalid capacity value")
        require(heap["largest"] <= heap["usable"], "largest block exceeds usable capacity")
        require(heap["metadata"] == heap["blocks"] * 20, "invalid native block overhead")
        require(heap["blocks"] != 0 or heap["usable"] == 0, "inconsistent empty heap")
        total += heap["usable"] + heap["metadata"]
    require(total <= available, "capacity exceeds SRAM")
    return sum(heap["usable"] for heap in heaps)


def verify(trace, profile, status):
    require(status.get("status") == "pass", "Stock run did not pass")
    require(
        all(status.get(key) is True for key in ("saw_pass", "returned", "return_settled")),
        "Stock execution/settled return missing",
    )
    require(
        type(trace.get("version")) is int and trace["version"] == 1, "unsupported trace version"
    )
    for key, source in (
        ("model", "model"),
        ("return_pc", "return_pc"),
        ("roots", "free_list_roots"),
        ("end", "end"),
    ):
        require(
            type(trace.get(key)) is int and trace[key] == profile[source],
            "trace/profile mismatch: " + key,
        )
    require(counter(trace.get("ignored_before_first_phase")), "missing pre-phase event count")
    phases = trace.get("phases")
    require(isinstance(phases, list) and 1 <= len(phases) <= 64, "invalid phase count")
    require(all(isinstance(phase, dict) for phase in phases), "invalid phase record")
    names = [phase.get("phase") for phase in phases]
    require(
        all(
            isinstance(name, str)
            and 1 <= len(name) <= 63
            and all(c.isascii() and (c.isalnum() or c in "-_") for c in name)
            for name in names
        ),
        "invalid phase name",
    )
    require(
        "application-pass" in names and names[-1] == "settled-return", "incomplete lifecycle phases"
    )
    available = profile["end"] - profile["begin"]
    accepted = failed = 0
    minima = []
    for index, phase in enumerate(phases):
        require(counter(phase.get("start_us")), "invalid phase start")
        if index + 1 < len(phases):
            require(
                counter(phase.get("end_us"))
                and phase["end_us"] >= phase["start_us"]
                and phase["end_us"] == phases[index + 1].get("start_us"),
                "broken phase interval",
            )
        else:
            require("end_us" in phase and phase["end_us"] is None, "last phase must remain open")
        for key in ("accepted", "rejected", "failed_allocations", "minimum_sample"):
            require(counter(phase.get(key)), "invalid observation counter: " + key)
        require(phase["rejected"] == 0, "rejected heap observations prevent acceptance")
        require(
            phase["failed_allocations"] <= phase["accepted"],
            "allocation failure count exceeds samples",
        )
        values = {
            key: capacity(phase.get(key), available) for key in ("initial", "minimum", "latest")
        }
        if phase["accepted"]:
            require(
                1 <= phase["minimum_sample"] <= phase["accepted"], "invalid minimum sample index"
            )
            require(
                type(phase.get("minimum_total_usable")) is int
                and phase["minimum_total_usable"] == values["minimum"],
                "minimum is not a paired-heap sum",
            )
            require(
                values["minimum"] <= min(values["initial"], values["latest"]),
                "minimum exceeds observed endpoints",
            )
            minima.append(dict(phase=phase["phase"], usable=values["minimum"]))
        else:
            require(
                phase["minimum_sample"] == 0
                and phase.get("minimum_total_usable") is None
                and all(
                    value == 0 for key in values for heap in phase[key] for value in heap.values()
                ),
                "empty phase invents a measurement",
            )
        accepted += phase["accepted"]
        failed += phase["failed_allocations"]
    require(accepted > 0, "no qualified allocator returns observed")
    return dict(
        model=profile["model"],
        phases=len(phases),
        accepted=accepted,
        failed_allocations=failed,
        minima=minima,
        ignored_before_first_phase=trace["ignored_before_first_phase"],
        scope="One qualified allocation function. Empty phases are unsampled; boot, other allocators and native stacks are not bounded.",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--model", choices=PROFILES["MODELS"], required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--stock-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    profile = PROFILES["allocator_profile"](args.rom.read_bytes(), PROFILES["MODELS"][args.model])
    report = verify(
        json.loads(args.trace.read_text()), profile, json.loads(args.stock_report.read_text())
    )
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(report, output, indent=2)
        output.write("\n")
    print(f"PASS {args.model}: {report['accepted']} qualified allocator observations")


if __name__ == "__main__":
    main()
