#!/usr/bin/env python3
"""Verify ENTER writeback in the named HP39 Aplet directory after Stock exit."""

import argparse
import json
import runpy
import struct
from pathlib import Path


def word(value, width=5):
    return f"{value:0{width}x}"[::-1]


def nibbles(data):
    return "".join(f"{b & 15:x}{b >> 4:x}" for b in data)


def check(
    aplet,
    sram,
    copy_state=None,
    arm_entry=False,
    c_probe=None,
    check_workspace=None,
    initial_note=None,
    saved_note=None,
):
    title = b"EntryProbe"
    header = b"HP39BinB" + bytes([len(title)]) + title
    if not aplet.startswith(header):
        raise ValueError("unexpected transfer title")
    directory = nibbles(aplet[len(header) :])
    if not directory.startswith(word(0x2A96) + word(0x7FF, 3)):
        raise ValueError("unexpected Aplet attachment")
    # The seventh application-data field holds the initial custom data.
    # It follows the two origin coordinates and precedes the outer list SEMI.
    initial = word(0x2A74) + word(0x312B)
    if initial_note is not None:
        value = b"hpclang-notebook/1\n" + initial_note.encode()
        initial = word(0x2A2C) + word(5 + len(value) * 2) + nibbles(value)
    origins = (word(0x2911) + word(0)) * 2
    tail = origins + initial + word(0x312B)
    if directory.count(tail) != 1:
        raise ValueError("missing initial custom-data slot")
    slot = directory.index(tail) + len(origins)
    link = slot + len(initial) + 5
    marker = b"hpclang-enter"
    saved = word(0x2A2C) + word(5 + len(marker) * 2) + nibbles(marker)

    def with_custom_data(data):
        growth = len(data) - len(initial)
        result = directory
        for pos in (8, link):
            offset = int(result[pos : pos + 5][::-1], 16)
            result = result[:pos] + word(offset + growth) + result[pos + 5 :]
        return result[:slot] + data + result[slot + len(initial) :]

    # Both the length-prefixed directory name and its trailing length belong
    # to the Stock directory entry. Transfer buffers contain a different name.
    memory = nibbles(sram)

    def require_entry(title, body):
        name = b"'" + title
        size = word(len(name), 2)
        prefix = size + nibbles(name) + size
        variants = [body]
        if arm_entry:
            # The source retains the seed. Only the aligned execution copy may
            # overwrite it. Stock can move the enclosing directory during Exit.
            source = bytes.fromhex(
                "01002de908009fe508008fe50100bde81eff2fe1394150487856341200000000"
            )
            before = nibbles(source)
            reserve = len(before) + 7
            if body.count(before + "0" * reserve) != 1:
                raise ValueError("missing ARM source and reserved execution area")
            pos = body.index(before) + len(before)
            after = nibbles(source[:24] + source[20:24] + source[28:])
            variants = [
                body[:pos] + "0" * pad + after + "0" * (7 - pad) + body[pos + reserve :]
                for pad in range(8)
            ]
        elif c_probe is not None:
            meta = json.loads((c_probe / "metadata.json").read_text())
            blob = (c_probe / "probe.bin").read_bytes()
            blob += bytes(-len(blob) % 8)
            source = nibbles(blob)
            reserve = len(source) + 7
            if blob.hex() != meta["blob"] or body.count(source + "0" * reserve) != 1:
                raise ValueError("C probe does not match the Aplet")
            pos = body.index(source) + len(source)
            start = prefix + body[:pos]
            if memory.count(start) != 1:
                raise ValueError("missing named C probe directory")
            idx = memory.index(start) + len(start)
            area = memory[idx : idx + reserve]
            pad = area.find(source[:64])
            if not 0 <= pad <= 7:
                raise ValueError("missing executed C probe")
            packed = area[pad : pad + len(source)]
            actual = bytes(int(packed[i : i + 2][::-1], 16) for i in range(0, len(packed), 2))
            base = None
            if check_workspace is None:
                workspace = meta["symbols"]["workspace"]
                marker = struct.unpack_from("<I", actual, workspace)[0]
                base = struct.unpack_from("<I", actual, workspace + 16)[0] - workspace
                if (
                    marker != 0x48504343
                    or base % 4
                    or not 0x08000000 <= base <= 0x08000000 + len(sram) - len(blob)
                ):
                    raise ValueError("invalid C workspace or execution address")
            # Stock may relocate the directory after the payload records its addresses.
            check_payload = runpy.run_path(str(Path(__file__).with_name("verify.py")))[
                "check_payload"
            ]
            check_payload(actual, meta, base, check_workspace)
            previous = ["0" * reserve]
            if saved_note is not None:
                # Publishing a longer note can move the Aplet before its second
                # launch. The previous aligned copy can leave up to seven edge
                # nibbles, all from immutable instructions at the ends of the blob.
                previous = ["0" * i + source + "0" * (7 - i) for i in range(8)]
            variants = {
                body[:pos] + old[:pad] + packed + old[pad + len(source) :] + body[pos + reserve :]
                for old in previous
            }
        if sum(memory.count(prefix + variant) for variant in variants) != 1:
            raise ValueError(f"saved named Aplet directory did not match {title!r}")

    expected = saved if initial_note is None else initial
    if saved_note is not None:
        value = b"hpclang-notebook/1\n" + saved_note.encode()
        expected = word(0x2A2C) + word(5 + len(value) * 2) + nibbles(value)
    require_entry(title, with_custom_data(expected))
    if copy_state is not None:
        require_entry(b"2", with_custom_data(word(0x5DA2) if copy_state == "reset" else expected))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aplet", type=Path)
    parser.add_argument("sram", type=Path)
    parser.add_argument("--copy-state", choices=("saved", "reset"))
    entry = parser.add_mutually_exclusive_group()
    entry.add_argument("--arm-entry", action="store_true")
    entry.add_argument("--c-probe", type=Path, help="directory with probe.bin and metadata.json")
    entry.add_argument(
        "--rpl-pool", type=Path, help="build directory for the paused RPL page probe"
    )
    entry.add_argument(
        "--bitmap-pool", type=Path, help="build directory for the stack-held bitmap probe"
    )
    parser.add_argument("--bitmap-image", type=Path, help="HPC1 image expected in the staged pages")
    parser.add_argument(
        "--notebook-editor",
        nargs=2,
        type=Path,
        metavar=("ENTRY_FRAME", "DOWN_FRAME"),
        help="require the editor display, focus movement, application return and restored L1",
    )
    parser.add_argument("--initial-note", help="expect this note in the transfer and editor")
    parser.add_argument(
        "--saved-note", help="expect this edited text in the reopened editor and native directory"
    )
    parser.add_argument("--save-frame", type=Path, help="LCD capture after pressing Save")
    parser.add_argument("--save-run", type=Path, help="verify all captures from the Save sequence")
    parser.add_argument(
        "--after-reset", action="store_true", help="expect an empty editor after Stock Reset"
    )
    args = parser.parse_args()
    if args.after_reset and not args.save_run:
        parser.error("--after-reset requires --save-run")
    if args.save_run:
        if args.notebook_editor or args.save_frame:
            parser.error("--save-run supplies --notebook-editor and --save-frame")
        args.notebook_editor = [
            args.save_run / f"screen-{name}.pgm" for name in ("editor2", "moved2")
        ]
        args.save_frame = args.save_run / "screen-saved.pgm"
    if (args.saved_note is not None) != (args.save_frame is not None) or (
        args.save_frame and not args.notebook_editor
    ):
        parser.error("--saved-note and --save-frame require each other and --notebook-editor")
    if args.bitmap_image and not args.bitmap_pool:
        parser.error("--bitmap-image requires --bitmap-pool")
    if args.notebook_editor:
        if not args.bitmap_image:
            parser.error("--notebook-editor requires --bitmap-image and --bitmap-pool")
        symbols = json.loads((args.bitmap_pool / "metadata.json").read_text())["symbols"]
        if "hpclang_bootstrap_run" not in symbols:
            parser.error("this probe only stages the image")
        tests = Path(__file__).resolve().parents[1]
        lcd = runpy.run_path(str(tests / "notebook/verify.py"))
        glyphs = lcd["font_rows"](tests.parents[1] / "runtime/fonts/generated/newrpl_6a.S")
        header = b"P5\n131 64\n255\n"
        text = args.saved_note if args.save_frame else args.initial_note
        frames = [
            (args.notebook_editor[0], text, True, "Loaded" if text is not None else None),
            (args.notebook_editor[1], text, False, "Loaded" if text is not None else None),
        ]
        if args.save_frame:
            frames.append((args.save_frame, text, False, "Saved; exit normally"))
        if args.save_run:
            frames.extend(
                [
                    (
                        args.save_run / "screen-action.pgm",
                        None if args.after_reset else args.initial_note,
                        True,
                        "No saved note; edit retained"
                        if args.after_reset
                        else "Loaded"
                        if args.initial_note is not None
                        else None,
                    ),
                    (args.save_run / "screen-typed.pgm", text, True, None),
                ]
            )
            library = (args.save_run / "screen-downloaded.pgm").read_bytes()
            if (
                not library.startswith(header)
                or len(library) != len(header) + 131 * 64
                or len(set(library[len(header) :])) < 2
            ):
                raise ValueError("missing original library display after transfer")
            for name in ("exited", "final"):
                if (args.save_run / f"screen-{name}.pgm").read_bytes() != library:
                    raise ValueError(f"Stock library display differed after {name}")
        for path, expected, focused, status in frames:
            frame = path.read_bytes()
            if not frame.startswith(header) or len(frame) != len(header) + 131 * 64:
                raise ValueError("unexpected HP39 LCD frame")
            pixels = frame[len(header) :]
            lcd["check_line"](pixels, glyphs, 0, "Notebook", path)
            lcd["check_line"](pixels, glyphs, 16, "Alpha to type", path)
            lcd["check_line"](pixels, glyphs, 24, expected or "", path, focused=focused)
            if status:
                lcd["check_line"](pixels, glyphs, 32, status, path)
    workspace_check = None
    report = {}
    if args.rpl_pool or args.bitmap_pool:
        verifier = Path(__file__).resolve().parents[1] / "independent-pool/verify.py"
        validate = runpy.run_path(str(verifier))[
            "check_bitmap_workspace" if args.bitmap_pool else "check_rpl_workspace"
        ]

        def workspace_check(words):
            report.update(validate(words, sram))
            if args.bitmap_image:
                inspect_image = runpy.run_path(str(Path(__file__).with_name("build.py")))[
                    "bitmap_image_info"
                ]
                size, checksum = inspect_image(args.bitmap_image)
                if tuple(words[44:46]) != (checksum, size):
                    raise ValueError("Notebook readback differs from the expected code image")
                report.update(image_bytes=size, code_fnv1a=f"{checksum:08x}")
            if args.notebook_editor:
                data_l1, code_l1, result, restored_data, restored_code = words[46:51]
                if result or (data_l1, code_l1) != (restored_data, restored_code):
                    raise ValueError("Notebook return or Stock L1 restoration failed")
                report.update(application_result=result, stock_l1=[restored_data, restored_code])

    sram = args.sram.read_bytes()
    check(
        args.aplet.read_bytes(),
        sram,
        args.copy_state,
        args.arm_entry,
        args.c_probe or args.rpl_pool or args.bitmap_pool,
        workspace_check,
        args.initial_note,
        args.saved_note,
    )
    if report:
        print(json.dumps(report))


if __name__ == "__main__":
    main()
