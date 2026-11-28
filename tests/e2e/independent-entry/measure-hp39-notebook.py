"""Compare HP39/40 Notebook relaunch from two installed Stock Aplet library states."""

import argparse
import json
import os
from pathlib import Path
import runpy
import shutil
import tempfile

HERE = Path(__file__).resolve().parent
run_stock = runpy.run_path(str(HERE / "run-hp39-notebook.py"))["run_stock"]
lcd = runpy.run_path(str(HERE.parent / "notebook/verify.py"))
SAMPLES = range(5, 305, 5)


def sequence():
    events = [
        "2500 capture library",
        "5000 tap enter",
        "8000 capture start",
        "11000 capture pre",
        "11000 tap enter",
    ]
    events.extend(f"{11000 + ms} capture t{ms:03d}" for ms in SAMPLES)
    events.extend(
        [
            "14000 capture action",
            "14600 tap two",
            "15000 tap zero",
            "15400 capture typed",
            "16000 tap down",
            "16600 tap enter",
            "18000 capture saved",
            "20000 tap on",
            "23000 tap aplet",
            "28000 capture exited",
            "31000 tap enter",
            "37000 tap enter",
            "40000 capture reopened",
            "46000 tap on",
            "49000 tap aplet",
            "52000 capture final",
            "54000 finish-install",
        ]
    )
    return "\n".join(events) + "\n"


def measure(dist, source, work, model, glyphs):
    work.mkdir()
    state = work / "state"
    shutil.copytree(source, state)
    seq = work / "sequence.seq"
    seq.write_text(sequence())
    env = {
        k: v
        for k, v in os.environ.items()
        if not k.startswith(("HPCLANG_X50NG_", "HPCLANG_E2E_", "HPCLANG_STOCK_"))
    }
    env.update(
        TARGET=model,
        HPCLANG_X50NG_HARNESS="stock",
        HPCLANG_STOCK_MODE="install",
        HPCLANG_X50NG_LCD_HEIGHT="64",
        HPCLANG_E2E_SEQUENCE=str(seq),
        HPCLANG_STOCK_PREFIX=str(work / "screen"),
        HPCLANG_STOCK_JSON=str(work / "result.json"),
        HPCLANG_X50NG_RESOURCE_SNAPSHOT=str(work / "res"),
    )
    if (dist / "runtime-lib").is_dir():
        env["LD_LIBRARY_PATH"] = os.pathsep.join(
            filter(None, [str(dist / "runtime-lib"), env.get("LD_LIBRARY_PATH")])
        )
    run_stock(dist, state, work, [], env, 150)

    def editor(tag, text):
        path = work / f"screen-{tag}.pgm"
        magic, size, maximum, pixels = path.read_bytes().split(b"\n", 3)
        assert (magic, size, maximum) == (b"P5", b"131 64", b"255") and len(pixels) == 131 * 64
        try:
            lcd["check_line"](pixels, glyphs, 0, "Notebook", path)
            lcd["check_line"](pixels, glyphs, 24, text, path, focused=True)
        except AssertionError:
            return False
        return True

    def timestamp(tag):
        return json.loads((work / f"res-{tag}.json").read_text())["time_us"]

    assert not editor("pre", "20") and editor("action", "20")
    assert editor("typed", "2020") and editor("reopened", "2020")
    assert (work / "screen-final.pgm").read_bytes() == (work / "screen-exited.pgm").read_bytes()
    start = timestamp("pre")
    previous = 0
    for ms in SAMPLES:
        tag = f"t{ms:03d}"
        elapsed = timestamp(tag) - start
        assert elapsed >= previous
        if editor(tag, "20"):
            return [previous, elapsed]
        previous = elapsed
    raise ValueError("focused Notebook editor did not appear during the sampled interval")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--model", choices=("hp39gplus", "hp39gs", "hp40gs"), required=True)
    for name in ("production", "independent"):
        parser.add_argument(
            f"--{name}",
            type=Path,
            required=True,
            help="Stock state with Notebook selected in the library and 20 saved",
        )
    args = parser.parse_args()
    glyphs = lcd["font_rows"](HERE.parents[2] / "runtime/fonts/generated/newrpl_6a.S")
    with tempfile.TemporaryDirectory(prefix="hpclang-notebook-timing-") as directory:
        result = {
            name: measure(
                args.dist.resolve(), getattr(args, name), Path(directory) / name, args.model, glyphs
            )
            for name in ("production", "independent")
        }
    print(json.dumps({"model": args.model, "editor_interval_us": result}))


if __name__ == "__main__":
    main()
