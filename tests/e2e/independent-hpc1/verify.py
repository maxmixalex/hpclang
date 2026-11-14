"""Verify the bounded independent HPC1 fixture against a real HP50g snapshot."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct

module = runpy.run_path(str(Path(__file__).resolve().parent.parent / "independent-entry/verify.py"))
require = module["require"]


def verify(artifact, state, baseline):
    manifest = json.loads((artifact / "build.json").read_text())
    for name, expected in manifest["outputs"].items():
        require(Path(name).name == name, "invalid artifact path")
        require(
            hashlib.sha256((artifact / name).read_bytes()).hexdigest() == expected,
            "artifact changed: " + name,
        )
    flash = (state / "flash").read_bytes()
    ram = (state / "sram").read_bytes()
    require(
        flash == (baseline / "flash").read_bytes(), "flash == (baseline / 'flash').read_bytes()"
    )
    require(
        hashlib.sha256(flash[:1310720]).hexdigest() == module["ROM_SHA256"],
        "hashlib.sha256(flash[:1310720]).hexdigest() == module['ROM_SHA256']",
    )
    module["no_toolbox"]((baseline / "sram").read_bytes(), flash)
    nib, word, count = module["no_toolbox"](ram, flash)
    obj = word(word(526072))
    require(
        word(obj) == 9748 and word(obj + 5) == 12 and (nib(obj + 16) == 0),
        "word(obj) == 9748 and word(obj + 5) == 12 and (nib(obj + 16) == 0)",
    )
    require(
        sum((nib(obj + 10 + i) * 10**i for i in range(6))) == 124275,
        "sum((nib(obj + 10 + i) * 10 ** i for i in range(6))) == 124275",
    )
    meta = json.loads((artifact / "metadata.json").read_text())
    symbols = meta["symbols"]
    expected = bytes.fromhex(meta["blob"])
    marker = struct.pack("<I", 843272264)
    candidates = []
    for at in range(len(ram) - 3):
        if ram.startswith(marker, at):
            start = at - symbols["workspace"]
            if start >= 0 and ram[start : start + 32] == expected[:32]:
                candidates.append(start)
    require(len(candidates) == 1, candidates)
    start = candidates[0]
    physical = 134217728 + start
    result = struct.unpack_from("<134I", ram, start + symbols["workspace"])
    require(result[0] == 843272264 and result[1] == 0, result[:16])
    require(
        result[2] == 42 and result[3:5] == result[5:7] and (result[7] == result[8]),
        "result[2] == 42 and result[3:5] == result[5:7] and (result[7] == result[8])",
    )
    require(result[11:14] == (6, 16384, 826495048), "result[11:14] == (6, 16384, 826495048)")
    require(
        result[14] & 255 == result[15] & 255 == 208, "result[14] & 255 == result[15] & 255 == 208"
    )
    control = struct.unpack_from("<5I", ram, start + symbols["control"])
    require(
        control[0] & 255 == control[2] & 255 == 16, "control[0] & 255 == control[2] & 255 == 16"
    )
    require(
        control[3] != 0 and control[3] == control[4], "control[3] != 0 and control[3] == control[4]"
    )
    arena = result[9]
    end = result[10]
    require(
        arena % 4096 == 0 and end - arena == 24576, "arena % 4096 == 0 and end - arena == 24576"
    )
    require(
        physical + symbols["arena"] <= arena < end <= physical + symbols["arena_end"],
        "physical + symbols['arena'] <= arena < end <= physical + symbols['arena_end']",
    )
    hpc = (artifact / "app.hpc").read_bytes()
    magic, size, entry, rw = struct.unpack_from("<4sIII", hpc)
    require(
        magic == b"HPC1" and rw == 12 and entry & 1 and (len(hpc) == size + 16),
        "magic == b'HPC1' and rw == 12 and entry & 1 and (len(hpc) == size + 16)",
    )
    require(
        ram[arena - 134217728 + 4096 : arena - 134217728 + 4096 + size] == hpc[16:],
        "ram[arena - 134217728 + 4096:arena - 134217728 + 4096 + size] == hpc[16:]",
    )
    require(
        struct.unpack_from("<III", ram, arena - 134217728 + 12288) == (6, 16384, 826495048),
        "struct.unpack_from('<III', ram, arena - 134217728 + 12288) == (6, 16384, 826495048)",
    )
    mutable = [
        (symbols["control"], symbols["control"] + 20),
        (symbols["workspace"], symbols["workspace"] + 536),
        (symbols["stack_start"], symbols["stack_end"]),
        (symbols["arena"], symbols["arena_end"]),
    ]
    actual = ram[start : start + meta["size"]]
    require(len(actual) == len(expected), "len(actual) == len(expected)")
    for i, (a, b) in enumerate(zip(actual, expected)):
        if not any((lo <= i < hi for lo, hi in mutable)):
            require(a == b, hex(i))

    def nibbles(data):
        return "".join((f"{b & 15:x}{b >> 4:x}" for b in data))

    packed = nibbles(ram)
    prefix = nibbles((artifact / "program.HP").read_bytes()[8:])[: meta["prefix"]]
    owners = [
        i
        for i in range(len(packed))
        if packed.startswith(prefix, i)
        and i + len(prefix) <= start * 2
        and (start * 2 + meta["size"] * 2 <= i + len(prefix) + meta["reserve"])
    ]
    require(len(owners) == 1, "len(owners) == 1")
    report = dict(
        complete=True,
        model="hp50g",
        payload_physical=physical,
        app_result=result[2],
        boot_model=result[11],
        rw_bytes=result[12],
        arena=[arena, end],
        entry_control=[control[0] & 255, control[2] & 255],
        entry_swi=list(control[3:]),
        l1_before=list(result[3:5]),
        l1_after=list(result[5:7]),
        swi_before=result[7],
        swi_after=result[8],
        cpsr_control_before=result[14] & 255,
        cpsr_control_after=result[15] & 255,
        no_library_275=True,
        unchanged_flash=True,
        owned_storage=True,
        scope="Bounded embedded HPC1/Thumb program through common bootstrap from independent Stock entry; not arbitrary apps, service coexistence or other families.",
        program_sha256=hashlib.sha256((artifact / "program.HP").read_bytes()).hexdigest(),
        sram_sha256=hashlib.sha256(ram).hexdigest(),
    )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("artifact", "state", "baseline", "report"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.artifact, args.state, args.baseline)
    with args.report.open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2)
        output.write("\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
