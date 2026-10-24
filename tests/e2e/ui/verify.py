#!/usr/bin/env python3
"""Verify the device-manager LCD checkpoints independently of guest assertions."""

import argparse
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


def verify_allocation(prefix: Path, model: int) -> int:
    """Cross-check the block and pool; return their union, not total native RAM."""
    pool_pages = verify_pool(prefix, model)
    reports = []
    fields = {
        "version",
        "id",
        "model",
        "pointer",
        "usable_bytes",
        "flags",
        "next",
        "previous",
        "metadata_bytes",
        "mapped_pages",
        "unmapped_usable_bytes",
    }
    begin, end = 0x08000000, 0x08040000 if model == 4 else 0x08080000
    for checkpoint in (1, 2):
        report = json.loads(Path(f"{prefix}-allocation-{checkpoint}.json").read_text())
        pool = json.loads(Path(f"{prefix}-pool-{checkpoint}.json").read_text())
        assert set(report) == fields, f"{prefix}: allocation report schema"
        assert all(type(value) is int and 0 <= value <= 0xFFFFFFFF for value in report.values())
        assert report["version"] == 1 and report.pop("id") == checkpoint
        assert report["model"] == model
        pointer, usable = report["pointer"], report["usable_bytes"]
        assert pointer == pool["native_os_block"], f"{prefix}: allocation/pool pointer mismatch"
        assert all(begin <= page <= end - 4096 for page in pool["physical_pages"])
        if not pointer:
            assert not any(report[key] for key in fields - {"version", "id", "model"})
        else:
            assert pointer % 4 == usable % 4 == 0
            assert begin + 16 <= pointer <= end - 4 and usable <= end - pointer - 4
            assert report["metadata_bytes"] == 20 and report["flags"] & 0xF000 in (0x1000, 0x2000)
            for key in ("next", "previous"):
                link = report[key]
                assert not link or (
                    link % 4 == 0 and begin <= link <= end - 16 and link != pointer - 16
                )
            mapped = 0
            for page in pool["physical_pages"]:
                if page < pointer + usable + 4 and page + 4096 > pointer - 16:
                    assert pointer <= page and page + 4096 <= pointer + usable
                    mapped += 1
            assert report["mapped_pages"] == mapped
            assert report["unmapped_usable_bytes"] == usable - mapped * 4096
        reports.append(report)
    assert reports[0] == reports[1], f"{prefix}: allocation changed across cleanup"
    total = pool_pages * 4096 + reports[0]["unmapped_usable_bytes"] + reports[0]["metadata_bytes"]
    print(f"PASS: {prefix}: pool/block union {total} bytes (other native allocations excluded)")
    return total


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("direct", "stock"))
    parser.add_argument("family", choices=("hp39", "hp48", "hp4950"))
    parser.add_argument(
        "--pool", action="store_true", help="require Stock native-pool observations"
    )
    parser.add_argument(
        "--allocation", action="store_true", help="also require validated native-block observations"
    )
    args = parser.parse_args()
    if args.pool and args.mode != "stock":
        parser.error("--pool requires Stock mode")
    if args.allocation and not args.pool:
        parser.error("--allocation requires --pool")
    for target in Path(f"build/{args.mode}-{args.family}-targets.txt").read_text().splitlines():
        verify(
            Path(f"build/{target}-frame"),
            80 if args.family == "hp4950" else 64,
            args.mode == "stock",
        )
        if args.pool:
            models = ("hp39gplus", "hp39gs", "hp40gs", "hp48gii", "hp49gplus", "hp50g")
            check = verify_allocation if args.allocation else verify_pool
            check(Path(f"build/{target}-frame"), models.index(target) + 1)


if __name__ == "__main__":
    main()
