"""Build and run the HP49g+/HP50g no-library Notebook save/relaunch fixture."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import shutil
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
E2E = HERE.parent
ROOT = E2E.parents[1]
entry = runpy.run_path(str(E2E / "independent-entry/build.py"))
word = entry["word"]
sys.path.insert(0, str(E2E / "port-recovery"))
from run import emulator, completed, transfer  # noqa: E402
from verify import home_string, require_stock_pc  # noqa: E402

entry_verifier = runpy.run_path(str(E2E / "independent-entry/verify.py"))
free_list_covers = runpy.run_path(str(E2E / "ui/verify.py"))["free_list_covers"]
allocator = runpy.run_path(str(E2E / "harness/verify-resource-snapshots.py"))
verify_observations = runpy.run_path(str(E2E / "harness/verify-native-heap-trace.py"))[
    "verify_observations"
]
no_toolbox = entry_verifier["no_toolbox"]
ui = runpy.run_path(str(E2E / "notebook/verify.py"))

# The low-memory run starts with the normal result already on the Stock stack.
# A distinct RPL result proves that the new program reached its cleanup check.
NORMAL_RESULT = 124275
LOW_MEMORY_RESULT = 124276
REFUSED_RESULT = 124277


builder = runpy.run_path(str(HERE / "build.py"))
pack = builder["pack"]


def build(dest, toolchain, profile, sdk, at):
    env = dict(os.environ, PATH=str(toolchain) + os.pathsep + os.environ["PATH"])
    if sdk is None:
        sdk = dest / "sdk"
        subprocess.run(
            ["cmake", "-S", ROOT, "-B", dest / "install", "-DCMAKE_INSTALL_PREFIX=" + str(sdk)],
            env=env,
            check=True,
        )
        subprocess.run(["cmake", "--install", dest / "install"], env=env, check=True)
    sdk = sdk.resolve()
    for cmd in [
        [
            "cmake",
            "-S",
            sdk / "share/hpclang/examples/notebook",
            "-B",
            dest / "app",
            "-DCMAKE_BUILD_TYPE=MinSizeRel",
            "-DCMAKE_PREFIX_PATH=" + str(sdk),
            "-DCMAKE_TOOLCHAIN_FILE="
            + str(sdk / "share/hpclang/cmake/toolchains/hpclang-armv4t.cmake"),
            "-DHPCLANG_BUILD_MODEL=" + profile["target"],
            f"-DHPCLANG_NOTEBOOK_PORT1_OFFSET={at}",
        ],
        [
            "cmake",
            "--build",
            dest / "app",
            "--target",
            "notebook_launchers",
            "notebook_independent_program",
            "-j4",
        ],
    ]:
        subprocess.run(cmd, env=env, check=True)
    hpc = (dest / "app/notebook.hpc").read_bytes()
    image, checksum, offset = builder["read_image"](hpc)
    for name in ("USER.HP", "COPY.HP", "DATA.HP"):
        shutil.copyfile(dest / "app/notebook-independent" / name, dest / name)
    for package, rw_bytes, expected_status in (
        ("RUN.HP", 32768, 0),
        ("AFTERGC.HP", 32768, 0),
        ("LOWMEM.HP", 20480, 1),
        ("REFUSE.HP", 32768, None),
    ):
        out = dest / package.removesuffix(".HP").lower()
        out.mkdir()
        build_runner(
            out,
            dest / package,
            toolchain,
            profile,
            len(image),
            checksum,
            offset,
            rw_bytes,
            expected_status,
            enter_after_gc=package == "AFTERGC.HP",
        )
    return image


def build_runner(
    out,
    package,
    toolchain,
    profile,
    size,
    checksum,
    offset,
    rw_bytes,
    expected_status,
    enter_after_gc,
):
    parts = [
        builder["build_phase"](
            out,
            toolchain,
            profile["target"],
            size,
            checksum,
            offset,
            rw_bytes,
            expected_status,
            phase,
            enter_after_gc=enter_after_gc,
        )
        for phase in ("acquire", "check")
    ]
    gc = entry["moving_gc_program"]()
    package.write_bytes(
        pack(
            word(0x2D9D)
            + parts[0]
            + gc
            + word(0x2614)
            + word(12)
            + str(REFUSED_RESULT)[::-1]
            + "0"
            + word(0x6657)  # NEWOB: private writable copy of the refusal result
            + parts[1]
            + word(0x312B)
        )
    )


def port1_field(ram, at, n):
    assert 0 <= at <= 0x40000 - n
    return sum(
        ((ram[0x20000 + (at + i) // 2] >> (4 * ((at + i) % 2))) & 15) << (4 * i) for i in range(n)
    )


def editor_pool_usage(ram, image, code_offset, record_start):
    """Account for each live launcher allocation once, including allocator overhead."""
    base = 0x08000000
    assert len(ram) == 0x80000

    def word_at(offset):
        return struct.unpack_from("<I", ram, offset)[0]

    # The launcher places its lease record in the leading slack of its first block.
    records = [
        at
        for at in range(16, 0x20000 - 168, 4)
        if word_at(at) == 0x4B4C4541 and word_at(at + 16) == base + at
    ]
    assert len(records) == 1
    record = records[0]
    count, pages = struct.unpack_from("<II", ram, record + 4)
    assert 1 <= count <= 12 and pages == 9
    extents = []
    frames = []
    for i in range(count):
        ptr, n = struct.unpack_from("<II", ram, record + 16 + 8 * i)
        at = ptr - base
        assert 16 <= at < 0x20000 and 1 <= n <= pages
        size, flags = struct.unpack_from("<II", ram, at - 16)
        assert flags & 0xF000 in (0x1000, 0x2000)
        assert n * 4096 + 4095 <= size and at + size + 4 <= 0x20000
        assert word_at(at + size) == size
        extents.append((ptr - 16, ptr + size + 4, flags & 0x300))
        first = (ptr + 4095) & ~4095
        frames.extend(first + 4096 * j for j in range(n))
    extents.sort()
    assert all(prev[1] <= cur[0] for prev, cur in zip(extents, extents[1:]))
    assert len(set(frames)) == len(frames) == pages
    # The last frame holds both L2 tables. Code stays in its separate Port 1 record.
    l2 = frames[-1] - base
    assert [word_at(l2 + 4 * i) for i in range(8)] == [p | 0xFF2 for p in frames[:8]]
    code = 0x20000 + code_offset
    assert ram[code : code + len(image)] == image
    code_pages = (len(image) + 4095) // 4096
    for i in range(code_pages):
        assert word_at(l2 + 4 * (256 + 32 + i)) == (base + code + i * 4096) | 0xAA2
    # NBLSE is a named Stock backup containing a string. Measure its complete
    # record and verify that every mapped code frame belongs to the string.
    assert port1_field(ram, record_start, 5) == 0x2B62
    record_end = record_start + 5 + port1_field(ram, record_start + 5, 5)
    name_len = port1_field(ram, record_start + 10, 2)
    assert name_len == 5
    name = bytes(port1_field(ram, record_start + 12 + 2 * i, 2) for i in range(name_len))
    assert name == b"NBLSE"
    string = record_start + 14 + 2 * name_len
    assert port1_field(ram, string - 2, 2) == name_len and port1_field(ram, string, 5) == 0x2A2C
    string_end = string + 5 + port1_field(ram, string + 5, 5)
    assert string + 10 <= 2 * code_offset < 2 * (code_offset + code_pages * 4096) <= string_end
    assert string_end + 10 == record_end and port1_field(ram, record_end, 5) == 0
    return {
        "kos_blocks": count,
        "kos_resident_bytes": sum(end - start for start, end, _ in extents),
        "pool_frame_bytes": pages * 4096,
        "port1_code_frame_bytes": code_pages * 4096,
        "port1_record_nibbles": record_end - record_start,
    }, extents


def user_memory(prefix, phases, model, flash, image, page, record_start, profile, trace):
    roots = profile["free_list_roots"]
    verify_observations(trace, profile)
    timeline = trace["phases"]
    names = [row["phase"] for row in timeline]
    deposited = names.index("deposited")
    assert names[deposited:] == ["deposited", *phases, "returned", "checkpoint", "installed"]
    free = {}
    pools, gaps = {}, {}
    owned = []
    for phase in ("deposited", *phases, "returned"):
        ram = (prefix / f"mem-{phase}.sram").read_bytes()
        free[phase] = allocator["free_capacity"](ram, roots)[0]["usable_bytes"]
        _, read = entry_verifier["mapping"](ram, flash, model)
        # Stock counts complete five-nibble cells between RSKTOP and DSKTOP in AVMEM.
        rstk, dstk = read(0x806F3), read(0x806F8)
        assert 0x80000 <= rstk <= dstk < 0x100000
        gaps[phase] = dstk - rstk
        assert read(0x80E9B) == gaps[phase] // 5
        if phase in phases:
            pools[phase], extents = editor_pool_usage(ram, image, page, record_start)
            assert not owned or owned == extents, "Notebook lease moved while live"
            owned = extents
        elif phase == "returned":
            base = 0x08000000

            def read32(address):
                assert base <= address <= base + len(ram) - 4 and address % 4 == 0
                return struct.unpack_from("<I", ram, address - base)[0]

            assert owned
            for start, end, heap in owned:
                assert heap == 0x100
                head = read32(roots)
                assert free_list_covers(read32, head, base, base + len(ram), heap, start, end), (
                    "Notebook allocation missing from Stock free list after return"
                )
            pools[phase] = {
                "kos_released_bytes": sum(end - start for start, end, _ in owned),
                "kos_released_blocks": len(owned),
            }
    admission = timeline[deposited]
    assert admission["accepted"] - admission["failed_allocations"] >= pools["editor"]["kos_blocks"]
    active = timeline[deposited : deposited + len(phases) + 1]
    minimum = min(row["minimum_total_usable"] for row in active if row["accepted"])
    assert admission["latest"][0]["usable"] == free["editor"], "trace and editor snapshot disagree"
    peak = free["deposited"] - minimum
    assert peak == pools["editor"]["kos_resident_bytes"], "unexpected peak KOS usage"
    assert all(free[phase] == free["editor"] for phase in phases)
    assert free["returned"] == free["deposited"], "Stock free capacity changed across user launch"
    return (
        pools,
        gaps,
        {
            "checkpoint_free_bytes": free,
            "allocator_returns": sum(row["accepted"] for row in active),
            "failed_allocations": sum(row["failed_allocations"] for row in active),
            "minimum_free_at_allocator_return_bytes": minimum,
            "peak_additional_kos_bytes": peak,
        },
    )


def check_phase(phase, editor_text, flash, original, font, expected_result):
    ram = (phase / "state/sram").read_bytes()
    assert (phase / "state/flash").read_bytes() == flash
    _, read, _ = no_toolbox(ram, flash)
    assert bytes(home_string(read, b"HPCNOTE")) == b"hpclang-notebook/1\n20"
    assert original == ram[0x20000:0x40000]
    stack = read(0x806F8)
    if expected_result is None:
        assert read(0x806FD) - stack == 15  # two values and the bottom marker
        for level, value in enumerate((b"second", b"first")):
            obj = read(stack + level * 5)
            assert read(obj) == 0x2A2C and read(obj + 5) == 5 + 2 * len(value)
            assert bytes(read(obj + 10 + 2 * i, 2) for i in range(len(value))) == value
    else:
        result = read(stack)
        digits = sum(read(result + 10 + i, 1) * 10**i for i in range(6))
        assert read(result) == 0x2614 and read(result + 5) == 12 and digits == expected_result
        entry_verifier["moving_gc_result"](read, stack + 5)
    require_stock_pc(phase / "state")
    returned = (phase / "stock-returned.pgm").read_bytes()
    assert returned == (phase / "stock-checkpoint.pgm").read_bytes()
    if editor_text is None:
        return hashlib.sha256(returned).hexdigest()
    f = phase / "stock-editor.pgm"
    magic, dims, maximum, img = f.read_bytes().split(b"\n", 3)
    width, height = map(int, dims.split())
    assert (
        magic == b"P5"
        and maximum == b"255"
        and width in (131, 132)
        and height == 80
        and len(img) == width * height
    )
    pixels = b"".join(img[i * width : i * width + 131] for i in range(height))
    ui["check_line"](pixels, font, 0, "Notebook", f)
    ui["check_line"](pixels, font, 24, editor_text, f, focused=True)
    assert returned != f.read_bytes()
    return hashlib.sha256(returned).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--model", choices=("hp49gplus", "hp50g"), default="hp50g")
    parser.add_argument("--sdk", type=Path, help="reuse an installed SDK from the current sources")
    parser.add_argument(
        "--baseline", type=Path, help="optional no-275 Stock snapshot for the selected model"
    )
    args = parser.parse_args()
    dest = args.output.resolve()
    dest.mkdir(parents=True)
    profile = json.loads((E2E / "targets" / f"{args.model}.json").read_text())
    spec = SimpleNamespace(
        model=args.model, profile=profile, dist=args.dist.resolve(), flash_busy_reads=0
    )
    baseline = args.baseline
    if baseline is None:
        fw = spec.dist / "firmware"
        with emulator(
            spec,
            dest / "clean",
            "2000 tap f6\n4000 tap f6\n7000 capture home\n9000 finish-install\n",
            [
                "--bootloader=" + str(fw / profile["bootloader"]),
                "--firmware=" + str(fw / "hp4950v215/2MB_FIX/2MB_215f.bin"),
                "--flash",
            ],
        ) as child:
            completed(child, 90)
        baseline = dest / "clean/state"
    ram = (baseline / "sram").read_bytes()
    flash = (baseline / "flash").read_bytes()
    _, read, _ = no_toolbox(ram, flash)
    bootloader = (spec.dist / "firmware" / profile["bootloader"]).read_bytes()
    expected_rom = entry_verifier["ROM_SHA256_BY_MODEL"][args.model]
    if (
        not flash.startswith(bootloader)
        or hashlib.sha256(flash[:0x140000]).hexdigest() != expected_rom
    ):
        raise ValueError(f"unexpected {args.model} ROM or bootloader")
    try:
        home_string(read, b"HPCNOTE")
    except ValueError as exc:
        if str(exc) != "HOME fixture variable is missing":
            raise
    else:
        raise ValueError("baseline contains HPCNOTE")

    # Put another backup across the first page boundary before installing code.
    padding = "ab" * 4500
    (dest / "PAD.HP").write_bytes(pack(word(0x2A2C) + word(5 + len(padding)) + padding))
    transfer(
        spec,
        dest / "padding",
        baseline,
        dest,
        "NBPAD",
        package_name="PAD.HP",
        execute=False,
        remote_commands=["-95 CF HOME NBPAD :1:NBPAD STO", "'NBPAD' PURGE"],
    )
    baseline = dest / "padding/state"
    ram = (baseline / "sram").read_bytes()

    # Stock appends NBLSE after the existing ERAM chain.
    at = 0
    while port1_field(ram, at, 5):
        assert port1_field(ram, at, 5) in (0x2B62, 0x2B40)
        size = port1_field(ram, at + 5, 5) + 5
        assert 20 < size <= 0x40000 - at
        at += size
    offset = at + 34
    assert offset % 2 == 0
    page = (offset // 2 + 4095) & ~4095
    assert page >= 8192
    image = build(dest, args.toolchain.resolve(), profile, args.sdk, at)
    transfer(
        spec,
        dest / "setup",
        baseline,
        dest,
        "LEASRC",
        package_name="DATA.HP",
        execute=False,
        remote_commands=["-95 CF HOME LEASRC :1:NBLSE STO", "'LEASRC' PURGE"],
    )
    staged = (dest / "setup/state/sram").read_bytes()
    assert staged[0x20000 + page : 0x20000 + page + len(image)] == image
    font = ui["font_rows"](ROOT / "runtime/fonts/generated/newrpl_6a.S")
    hashes = {}
    card = dest / "notebook.img"
    media = runpy.run_path(str(ROOT / "tools/e2e-stock-media.py"))
    files = {
        "initial_files": {"KEEP.TXT": "Retained card content\n"},
        "expected_files": {"HPCNOTE.TXT": "202\n"},
    }
    media["prepare"](files, card)
    source = "setup"
    save_keys = "4300 tap two\n4900 tap zero\n5500 tap down\n6200 tap enter\n7600 capture saved\n"
    heap_profile = allocator["allocator_profile"](flash, allocator["MODELS"][args.model])
    for name, previous, editor in (("user-save", "setup", ""), ("user-reopen", "user-save", "20")):
        installed = previous == "user-save"
        with tempfile.TemporaryDirectory(dir=dest) as tmp:
            transfer(
                spec,
                dest / name,
                dest / previous / "state",
                dest,
                "NBUSER",
                package_name="USER.HP",
                installed=installed,
                extra_files=[] if installed else [("COPY.HP", "CKCOPY")],
                remote_commands=[
                    ("HOME " if installed else 'HOME CLEAR "first" "second" ')
                    + "'NBUSER' RCL CKCOPY"
                ],
                actions="3500 capture editor\n"
                + ("" if installed else save_keys)
                + "8500 tap on\n15000 capture returned\n",
                execution_ms=18000,
                resource_prefix=Path(tmp) / "mem",
                native_heap_profile=heap_profile,
            )
            phases = ("editor",) if installed else ("editor", "saved")
            trace = json.loads((dest / name / "native-heap.json").read_text())
            usage, gaps, heap = user_memory(
                Path(tmp), phases, args.model, flash, image, page, at, heap_profile, trace
            )
        if args.baseline is None:
            assert usage["editor"]["kos_blocks"] == 1, "fresh Stock launch used a split pool"
        hashes[name] = {
            "memory": usage,
            "kos_heap": heap,
            "stock_free_gap_nibbles": gaps,
            "user_stack_top_first": ["second", "first"],
            "settled_sha256": check_phase(
                dest / name, editor, flash, staged[0x20000:0x40000], font, None
            ),
        }
    for name, program, package, editor in (
        ("save", "SPLITGC", "RUN.HP", ""),
        ("refused", "NBFULL", "REFUSE.HP", None),
        ("low-memory", "SPLITERR", "LOWMEM.HP", None),
        ("reload", "SPLITGC", None, "20"),
    ):
        actions = ""
        if editor is not None:
            actions = (
                "3500 capture editor\n" + (save_keys if name == "save" else "") + "8500 tap on\n"
            )
        if name == "reload":
            # Move past Save, Reload and Clock to export the edit, then back it up to Port 0.
            actions = (
                "3500 capture editor\n4300 tap two\n"
                + "".join(f"{5000 + 700 * i} tap down\n" for i in range(4))
                + "8000 tap enter\n9200 tap down\n10100 tap enter\n12300 tap on\n"
            )
        transfer(
            spec,
            dest / name,
            dest / source / "state",
            dest,
            program,
            installed=package is None,
            package_name=package,
            extra_files=[("COPY.HP", "CKCOPY")] if name == "save" else [],
            remote_commands=[
                f"HOME '{program}' RCL CKCOPY" + (" 'NBFULL' PURGE" if name == "refused" else "")
            ],
            actions=actions + "15000 capture returned\n",
            execution_ms=18000,
            sd_image=card if name == "reload" else None,
        )
        hashes[name] = {
            "editor": editor,
            "home_hex": b"hpclang-notebook/1\n20".hex(),
            "settled_sha256": check_phase(
                dest / name,
                editor,
                flash,
                staged[0x20000:0x40000],
                font,
                {"refused": REFUSED_RESULT, "low-memory": LOW_MEMORY_RESULT}.get(
                    name, NORMAL_RESULT
                ),
            ),
        }
        source = name
    # Branch from the saved-note state so the existing reload and Port 0 checks
    # retain their own installation and source chain.
    after_gc = dest / "entry-after-gc"
    with tempfile.TemporaryDirectory(dir=dest) as tmp:
        transfer(
            spec,
            after_gc,
            dest / "save/state",
            dest,
            "NBAFT",
            package_name="AFTERGC.HP",
            remote_commands=["HOME 'NBAFT' RCL CKCOPY"],
            actions="3500 capture editor\n8500 tap on\n15000 capture returned\n",
            execution_ms=18000,
            resource_prefix=Path(tmp) / "mem",
        )
        ram = (Path(tmp) / "mem-editor.sram").read_bytes()
        _, read = entry_verifier["mapping"](ram, flash, args.model)
        moved = entry_verifier["moving_gc_result"](read, read(0x806F8) + 5)
    hashes["entry-after-gc"] = {
        "moved_before_editor": moved,
        "editor": "20",
        "settled_sha256": check_phase(
            after_gc, "20", flash, staged[0x20000:0x40000], font, NORMAL_RESULT
        ),
    }
    media["verify"](files, card)
    hashes["reload"]["export"] = files["expected_files"]
    hashes["refused"].update(requested_pages=35, rpl_result=REFUSED_RESULT)
    hashes["low-memory"].update(exit_status=1, rw_bytes=20480)
    recall = dest / "port0-recall"
    transfer(
        spec,
        recall,
        dest / "reload/state",
        dest,
        "SPLITGC",
        installed=True,
        execute=False,
        remote_commands=["HOME :0:HPCNOTE RCL 'NBREAD' STO"],
        execution_ms=4000,
    )
    assert (recall / "state/flash").read_bytes() == flash
    ram = (recall / "state/sram").read_bytes()
    _, read, _ = no_toolbox(ram, flash)
    saved = home_string(read, b"HPCNOTE")
    backup = home_string(read, b"NBREAD")
    assert saved == b"hpclang-notebook/1\n20" and backup == b"hpclang-notebook/1\n202"
    assert ram[0x20000:0x40000] == staged[0x20000:0x40000]
    require_stock_pc(recall / "state")
    hashes["port0-recall"] = {"home_hex": saved.hex(), "backup_hex": backup.hex()}
    purge = dest / "purge"
    transfer(
        spec,
        purge,
        recall / "state",
        dest,
        "SPLITGC",
        installed=True,
        execute=False,
        remote_commands=["HOME :1:NBLSE PURGE"],
        execution_ms=4000,
    )
    after = (purge / "state/sram").read_bytes()[0x20000:0x40000]
    assert after[: at // 2] == staged[0x20000 : 0x20000 + at // 2]
    assert int.from_bytes(after[at // 2 : at // 2 + 3], "little") & 0xFFFFF == 0
    removed = dest / "removed"
    transfer(
        spec,
        removed,
        purge / "state",
        dest,
        "SPLITGC",
        installed=True,
        remote_commands=["HOME 'SPLITGC' RCL CKCOPY"],
        actions="15000 capture returned\n",
        execution_ms=18000,
    )
    hashes["removed"] = {
        "code_offset": page,
        "settled_sha256": check_phase(removed, None, flash, after, font, REFUSED_RESULT),
    }
    (dest / "report.json").write_text(json.dumps(hashes, indent=2))
    print(
        f"PASS independent {args.model} Notebook HOME bytes, editor reload, GC and settled return"
    )


if __name__ == "__main__":
    main()
