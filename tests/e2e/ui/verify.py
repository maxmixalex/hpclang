#!/usr/bin/env python3
"""Verify the device-manager LCD checkpoints independently of guest assertions."""

import argparse
import hashlib
import json
from pathlib import Path


def read_frame(path: Path, height: int) -> bytes:
    data = path.read_bytes()
    header = f"P5\n132 {height}\n255\n".encode()
    assert data.startswith(header), f"{path}: geometry"
    pixels = data[len(header) :]
    assert len(pixels) == 132 * height, f"{path}: truncated pixels"
    visible = bytes(pixels[y * 132 + x] for y in range(height) for x in range(131))
    assert set(visible) == {0, 255}, f"{path}: expected populated monochrome UI"
    return pixels


def waveform(pixels: bytes, top: int, prefix: Path) -> None:
    for i in range(32):
        phase = (i + 6) % 32
        sample = min(phase, 31 - phase) * 64
        x = i * 130 // 31
        y = top + 23 - sample * 23 // 1023
        assert pixels[y * 132 + x] == 0, f"{prefix}: missing wave vertex {i} at {x},{y}"
    assert pixels[top * 132 + 65] == 255, f"{prefix}: canvas background overwritten"


def verify_memory(prefix: Path) -> tuple[int, int, int, int]:
    memory = json.loads(Path(f"{prefix}-memory-1.json").read_text())
    assert set(memory) == {"version", "id", "words"}, f"{prefix}: memory report fields"
    assert type(memory["version"]) is int and type(memory["id"]) is int
    assert memory["version"] == 1 and memory["id"] == 1, f"{prefix}: memory report schema"
    assert isinstance(memory["words"], list) and len(memory["words"]) == 8
    assert all(type(value) is int and 0 <= value <= 0xFFFFFFFF for value in memory["words"])
    version, entry, main_peak, final_peak, stack_limit, heap_peak, heap_limit, rw_bytes = memory[
        "words"
    ]
    assert version == 1 and rw_bytes == 36864, f"{prefix}: application memory budget"
    assert 0 < entry <= main_peak <= final_peak < stack_limit == 8192, (
        f"{prefix}: missing or exhausted stack watermark"
    )
    assert all(value % 4 == 0 for value in (entry, main_peak, final_peak))
    assert 0 < heap_peak <= heap_limit < rw_bytes - stack_limit, f"{prefix}: heap budget"
    return final_peak, stack_limit, heap_peak, heap_limit


def verify_stack_pointer(prefix: Path) -> dict:
    """Check the shared User/System SP bank against the application stack arena."""
    output = Path(f"{prefix}-stack-pointer-budget.json")
    output.unlink(missing_ok=True)
    touched, limit, _, _ = verify_memory(prefix)
    trace_path = Path(f"{prefix}-stack-pointer.jsonl")
    trace = trace_path.read_bytes()
    rows = [json.loads(line) for line in trace.splitlines()]
    assert rows, f"{prefix}: missing SP observations"
    fields = {"version", "mode", "writes", "minimum", "maximum", "minimum_pc"}
    modes = {}
    for row in rows:
        assert isinstance(row, dict) and set(row) == fields, f"{prefix}: SP report schema"
        assert all(type(row[key]) is int for key in fields)
        assert row["version"] == 1 and 0 < row["writes"] <= 0xFFFFFFFFFFFFFFFF
        mode = row["mode"]
        assert mode in (16, 17, 18, 19, 23, 27, 31) and mode not in modes
        assert 0 <= row["minimum"] <= row["maximum"] <= 0xFFFFFFFF
        assert row["minimum"] % 4 == row["maximum"] % 4 == 0
        assert 0x01200000 <= row["minimum_pc"] < 0x01300000
        assert row["minimum_pc"] % 2 == 0
        modes[mode] = row
    # Bootstrap preserves the caller mode; Stock may enter in User mode.
    # User/System share r13, while exception modes have separate banked stacks.
    application = [modes[mode] for mode in (16, 31) if mode in modes]
    assert application, f"{prefix}: missing application User/System-mode SP"
    top = 0x01100000 + 36864
    assert max(row["maximum"] for row in application) == top, (
        f"{prefix}: wrong application stack top"
    )
    used = top - min(row["minimum"] for row in application)
    assert touched <= used < limit, f"{prefix}: exhausted or incomplete SP observation"
    report = dict(
        version=1,
        stack_pointer_bytes=used,
        stack_touched_bytes=touched,
        stack_limit_bytes=limit,
        observations=rows,
        trace_sha256=hashlib.sha256(trace).hexdigest(),
        memory_sha256=hashlib.sha256(Path(f"{prefix}-memory-1.json").read_bytes()).hexdigest(),
        scope="Executed application SP writes in the shared User/System bank; excludes native firmware instructions, exception bank switches and unexecuted paths.",
    )
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS: {prefix}: observed SP usage {used}/{limit}, touched {touched}")
    return report


def verify_updates(prefix: Path) -> tuple[int, int, int, int]:
    report = json.loads(Path(f"{prefix}-memory-2.json").read_text())
    assert set(report) == {"version", "id", "words"}, f"{prefix}: update report fields"
    assert type(report["version"]) is int and type(report["id"]) is int
    assert report["version"] == 1 and report["id"] == 2, f"{prefix}: update report schema"
    assert isinstance(report["words"], list) and len(report["words"]) == 5
    assert all(type(value) is int and 0 <= value <= 0xFFFFFFFF for value in report["words"])
    version, frame_us, input_us, frames, inputs = report["words"]
    assert version == 1, f"{prefix}: update payload version"
    assert 0 < frame_us < 0xFFFFFFFF and 0 < input_us < 0xFFFFFFFF, (
        f"{prefix}: absent or saturated update timing"
    )
    # Six timed application/theme updates, one font update and two pressure updates.
    assert frames == 9, f"{prefix}: incomplete frame timing coverage"
    assert inputs == 34, f"{prefix}: incomplete key-edge timing"
    return frame_us, input_us, frames, inputs


def verify(prefix: Path, height: int, stock: bool = False) -> None:
    final_peak, stack_limit, heap_peak, heap_limit = verify_memory(prefix)
    frame_us, input_us, frame_samples, input_samples = verify_updates(prefix)
    frames = [read_frame(Path(f"{prefix}-{phase}.pgm"), height) for phase in range(1, 8)]
    assert len(set(frames)) == 7, f"{prefix}: distinct application/theme states missing"

    # The final action increments the model revision from 1 to 2. Its focused
    # eight-pixel button sits at the viewport bottom; a two-pixel gap separates
    # it from the 24-pixel canvas. Check the model's exact waveform vertices on
    # the physical LCD, without calling the renderer or matching an image hash.
    top = height - 34
    pixels = frames[5]
    waveform(pixels, top, prefix)
    # Replacing the renderer keeps the same model, geometry and focused control.
    # Custom canvas paint owns its palette; the surrounding themed UI reverses.
    for y in range(height):
        for x in range(131):
            original = pixels[y * 132 + x]
            expected = original if top <= y < top + 24 else 255 - original
            assert frames[6][y * 132 + x] == expected, (
                f"{prefix}: theme/canvas boundary mismatch at {x},{y}"
            )
    # Stock default fonts have eight-row line metrics on all six models. A
    # Direct context without a ROM mapping must use the six-row packaged font.
    candidates = [size for size in (6, 8) if Path(f"{prefix}-{100 + size}.pgm").exists()]
    assert len(candidates) == 1, f"{prefix}: missing/ambiguous font checkpoint"
    line_height = candidates[0]
    assert not stock or line_height == 8, f"{prefix}: Stock native font unavailable"
    font_pixels = read_frame(Path(f"{prefix}-{100 + line_height}.pgm"), height)
    waveform(font_pixels, height - (28 + line_height), prefix)
    pressure_pixels = read_frame(Path(f"{prefix}-121.pgm"), height)
    assert pressure_pixels == font_pixels, f"{prefix}: allocator exhaustion changed the live UI"
    rebuilt_pixels = read_frame(Path(f"{prefix}-122.pgm"), height)
    assert rebuilt_pixels == font_pixels, f"{prefix}: renderer reconstruction changed the live UI"
    if line_height == 6:
        assert font_pixels == frames[6], f"{prefix}: packaged fallback changed the view"
    else:
        assert font_pixels != frames[6], f"{prefix}: native font did not alter layout"
    print(f"PASS: {prefix}: UI states, waveform, independent theme and font layout")
    print(
        f"PASS: {prefix}: touched stack {final_peak}/{stack_limit}, heap {heap_peak}/{heap_limit}"
    )
    print(
        f"PASS: {prefix}: observed frame/input maxima {frame_us}/{input_us} us, "
        f"samples {frame_samples}/{input_samples} (emulator observations)"
    )


def verify_pool(prefix: Path, model: int) -> int:
    reports = []
    fields = {
        "version",
        "id",
        "model",
        "rw_bytes",
        "stage_pool_pages",
        "native_workspace_pages",
        "native_os_block",
        "pool_l2_physical",
        "physical_pages",
    }
    for checkpoint in (1, 2):
        report = json.loads(Path(f"{prefix}-pool-{checkpoint}.json").read_text())
        assert set(report) == fields, f"{prefix}: pool report schema"
        assert all(
            type(report[key]) is int and 0 <= report[key] <= 0xFFFFFFFF
            for key in fields - {"physical_pages"}
        )
        assert report["version"] == 1 and report.pop("id") == checkpoint
        assert report["model"] == model and report["rw_bytes"] == 36864
        workspace = 2 if model <= 3 else 0
        assert report["native_workspace_pages"] == workspace
        count = report["stage_pool_pages"] + workspace
        assert 0 < report["stage_pool_pages"] and count <= 256
        pages = report["physical_pages"]
        assert isinstance(pages, list) and len(pages) == count
        assert all(
            type(page) is int and 0 < page <= 0xFFFFF000 and page % 4096 == 0 for page in pages
        )
        assert len(set(pages)) == count, f"{prefix}: aliased pool pages"
        assert report["pool_l2_physical"] in pages, f"{prefix}: table outside pool"
        reports.append(report)
    assert reports[0] == reports[1], f"{prefix}: pool changed across cleanup"
    print(f"PASS: {prefix}: observed native pool {count} pages ({count * 4096} bytes)")
    return count


def verify_budget(prefix: Path, model: str, artifact: Path) -> dict:
    """Reconcile the observed pool and RW watermarks with this linked image."""
    output = Path(f"{prefix}-budget.json")
    output.unlink(missing_ok=True)
    models = ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g")
    count = verify_pool(prefix, models.index(model) + 1)
    stack_peak, stack_limit, heap_peak, heap_limit = verify_memory(prefix)
    payload = (artifact / "e2e.hpc").read_bytes()
    layout_bytes = (artifact / "launchers/memory.json").read_bytes()
    layout = json.loads(layout_bytes)
    assert len(payload) >= 16 and payload[:4] == b"HPC1"
    image, static = int.from_bytes(payload[4:8], "little"), int.from_bytes(payload[12:16], "little")
    assert image > 0 and len(payload) == image + 16
    assert layout["version"] == 1 and layout["page_bytes"] == 4096
    app, stage, selected = layout["application"], layout["launcher"], layout["models"][model]
    assert app["image_bytes"] == image and app["static_bytes"] == static
    assert app["code_pages"] == (image + 4095) // 4096
    assert app["boot_pages"] == 1 and app["stack_bytes"] == stack_limit
    assert app["required_rw_bytes"] == app["rw_ceiling_bytes"] == 36864
    keys = ("stage_and_prefix_pages", "stage_stack_pages", "mmu_workspace_pages")
    assert all(type(stage[key]) is int and stage[key] > 0 for key in keys)
    assert stage["stage_stack_pages"] == stage["mmu_workspace_pages"] == 1
    assert 0 < stage["stage_extent_bytes"] <= stage["stage_and_prefix_pages"] * 4096
    workspace = 2 if models.index(model) < 3 else 0
    assert selected["stock_native_workspace_pages"] == workspace
    required = sum(stage[key] for key in keys) + app["code_pages"] + 9 + workspace
    assert selected["minimum_stock_pool_pages"] == required
    assert selected["stock_pool_pages_for_rw_ceiling"] == required
    assert required <= count <= layout["pool_capacity_pages"] == 256
    arena_overhead = 36864 - 4096 - static - stack_limit - heap_limit
    assert arena_overhead >= 0, f"{prefix}: static/stack/heap exceed RW arena"
    report = dict(
        version=1,
        model=model,
        payload_sha256=hashlib.sha256(payload).hexdigest(),
        layout_sha256=hashlib.sha256(layout_bytes).hexdigest(),
        pool_reserved_bytes=count * 4096,
        pool_required_bytes=required * 4096,
        pool_excess_bytes=(count - required) * 4096,
        rw_bytes=36864,
        static_bytes=static,
        boot_bytes=4096,
        stack_limit=stack_limit,
        stack_touched=stack_peak,
        heap_limit=heap_limit,
        heap_extent_peak=heap_peak,
        arena_other_bytes=arena_overhead,
        scope="Observed pool and application arena; excludes native allocator overhead, stored objects and unsampled system peaks. Heap includes renderer and LCD allocations; do not add them again.",
    )
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS: {prefix}: pool required/reserved {required * 4096}/{count * 4096} bytes")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    parser.add_argument(
        "--pool", action="store_true", help="require Stock native-pool observations"
    )
    parser.add_argument(
        "--artifact", type=Path, help="reconcile pool/RW with the linked UI artifact"
    )
    parser.add_argument("--stack-pointer", action="store_true", help="require application SP trace")
    args = parser.parse_args()
    if args.pool and args.mode != "stock":
        parser.error("--pool requires Stock mode")
    if args.artifact and not args.pool:
        parser.error("--artifact requires --pool")
    for target in Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines():
        Path(f"build/{target}-frame-budget.json").unlink(missing_ok=True)
        verify(
            Path(f"build/{target}-frame"),
            80 if args.family == "hp4950" else 64,
            args.mode == "stock",
        )
        if args.stack_pointer:
            verify_stack_pointer(Path(f"build/{target}-frame"))
        if args.artifact:
            verify_budget(Path(f"build/{target}-frame"), target, args.artifact)
        elif args.pool:
            models = ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g")
            verify_pool(Path(f"build/{target}-frame"), models.index(target) + 1)


if __name__ == "__main__":
    main()
