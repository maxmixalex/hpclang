#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

DOTS = (
    (0x01, 0x08),
    (0x02, 0x10),
    (0x04, 0x20),
    (0x40, 0x80),
)


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"P5"):
        raise ValueError(f"{path}: expected P5 PGM")
    pos = 2
    values = []
    while len(values) < 3:
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        if data[pos] == ord("#"):
            while pos < len(data) and data[pos] not in b"\r\n":
                pos += 1
            continue
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n":
            pos += 1
        values.append(data[start:pos])
    width, height, maxval = map(int, values)
    if maxval != 255:
        raise ValueError(f"{path}: unsupported maxval {maxval}")
    while pos < len(data) and data[pos] in b" \t\r\n":
        pos += 1
    pixels = data[pos:pos + width * height]
    if len(pixels) != width * height:
        raise ValueError(f"{path}: truncated pixel data")
    return width, height, pixels


def render(width: int, height: int, pixels: bytes, threshold: int) -> str:
    lines = []
    for y0 in range(0, height, 4):
        chars = []
        for x0 in range(0, width, 2):
            mask = 0
            for dy in range(4):
                y = y0 + dy
                if y >= height:
                    continue
                for dx in range(2):
                    x = x0 + dx
                    if x >= width:
                        continue
                    if pixels[y * width + x] < threshold:
                        mask |= DOTS[dy][dx]
            chars.append(chr(0x2800 + mask) if mask else " ")
        lines.append("".join(chars).rstrip())
    return "\n".join(lines)


def title(path: Path) -> str:
    name = path.name[:-4] if path.name.endswith(".pgm") else path.name
    return name.replace("-", " ").replace("_", " ")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("files", nargs="*", type=Path)
    p.add_argument("--json", dest="json_files", action="append", default=[])
    p.add_argument("--threshold", type=int, default=192)
    p.add_argument("--heading", default="Calculator LCD evidence")
    args = p.parse_args()

    print(f"## {args.heading}")
    found = False
    for path in args.files:
        if not path.exists():
            continue
        found = True
        width, height, pixels = read_pgm(path)
        print(f"\n### {title(path)} — {width}×{height}")
        print("~~~text")
        print(render(width, height, pixels, args.threshold))
        print("~~~")

    for raw in args.json_files:
        path = Path(raw)
        if not path.exists():
            continue
        found = True
        print(f"\n### {title(path)}")
        print("~~~json")
        try:
            print(json.dumps(json.loads(path.read_text()), indent=2, sort_keys=True))
        except Exception:
            print(path.read_text().rstrip())
        print("~~~")

    if not found:
        print("\n_No LCD evidence was produced before this job stopped._")


if __name__ == "__main__":
    main()
