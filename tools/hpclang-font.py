#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import re
import struct
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Glyph:
    codepoint: int
    width: int
    height: int
    advance: int
    x_offset: int = 0
    y_offset: int = 0
    bitmap: bytes = b""
    stride: int = 0


@dataclass
class Font:
    source: str
    glyphs: dict[int, Glyph]
    ascent: int
    descent: int
    line_height: int
    nominal_advance: int
    format: str
    face: str = ""


def _num(s: str) -> int:
    s = s.strip()
    if len(s) >= 2 and s[0] == s[-1] == '"':
        s = s[1:-1]
    return int(s, 0)


class Package:
    """Read either the canonical raw fonts/ ZIP or an older manifest package."""

    def __init__(self, path):
        self.path = Path(path)
        self.z = zipfile.ZipFile(path)
        self.root = "fonts/"
        names = self.z.namelist()
        self._name_set = set(names)

        if "fonts/manifest.json" in self._name_set:
            self.manifest = json.loads(self.z.read("fonts/manifest.json"))
        else:
            if not any(name.startswith(self.root) and not name.endswith("/") for name in names):
                raise ValueError("font package has no fonts/ source tree")

            entries = []
            for name in names:
                if name.endswith("/") or not name.startswith(self.root):
                    continue
                logical = name[len(self.root) :]
                ext = Path(logical).suffix.lower()
                sibling_txt = str(Path(logical).with_suffix(".txt")).replace("\\", "/")
                is_font = ext in {".bdf", ".fon"} or (
                    ext == ".bmp" and self.root + sibling_txt in self._name_set
                )
                info = self.z.getinfo(name)
                data = self.z.read(name)
                entries.append(
                    {
                        "path": logical,
                        "bytes": info.file_size,
                        "sha256": hashlib.sha256(data).hexdigest(),
                        "is_font": is_font,
                        "format": ext.lstrip("."),
                        "family": logical.split("/", 1)[0],
                    }
                )
            self.manifest = {
                "schema": "hpclang-font-package-index-v1",
                "files": entries,
            }

        self.files = {entry["path"]: entry for entry in self.manifest["files"]}

    def data(self, path):
        return self.z.read(self.root + path)

    def fonts(self):
        return [entry for entry in self.manifest["files"] if entry.get("is_font")]

    def verify(self):
        bad = []
        corrupt = self.z.testzip()
        if corrupt is not None:
            bad.append(corrupt)
        for entry in self.manifest["files"]:
            try:
                data = self.data(entry["path"])
            except KeyError:
                bad.append(entry["path"])
                continue
            if "bytes" in entry and len(data) != int(entry["bytes"]):
                bad.append(entry["path"])
                continue
            expected = entry.get("sha256")
            if expected and hashlib.sha256(data).hexdigest() != expected:
                bad.append(entry["path"])
        return bad


def parse_bdf(data: bytes, source: str) -> Font:
    lines = data.decode("latin-1").splitlines()
    ascent = descent = None
    fbb = None
    glyphs = {}
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith("FONTBOUNDINGBOX "):
            p = line.split()
            try:
                fbb = tuple(_num(x) for x in p[1:5])
            except Exception:
                pass
        elif line.startswith("FONT_ASCENT "):
            try:
                ascent = _num(line.split(None, 1)[1])
            except Exception:
                pass
        elif line.startswith("FONT_DESCENT "):
            try:
                descent = _num(line.split(None, 1)[1])
            except Exception:
                pass
        elif line.startswith("STARTCHAR"):
            enc = adv = bbx = None
            bitmap = []
            i += 1
            while i < len(lines):
                s = lines[i].strip()
                if s.startswith("ENCODING "):
                    try:
                        vals = s.split()
                        enc = int(vals[1])
                        if enc < 0 and len(vals) > 2:
                            enc = int(vals[2])
                    except Exception:
                        enc = None
                elif s.startswith("DWIDTH "):
                    try:
                        adv = int(s.split()[1])
                    except Exception:
                        pass
                elif s.startswith("BBX "):
                    try:
                        bbx = tuple(int(x) for x in s.split()[1:5])
                    except Exception:
                        pass
                elif s == "BITMAP":
                    i += 1
                    while i < len(lines) and lines[i].strip() != "ENDCHAR":
                        bitmap.append(lines[i].strip())
                        i += 1
                    break
                elif s == "ENDCHAR":
                    break
                i += 1
            if enc is not None and 0 <= enc <= 0x10FFFF and bbx:
                w, h, xoff, yoff = bbx
                stride = (max(w, 0) + 7) // 8
                rows = bytearray()
                for row in bitmap[: max(h, 0)]:
                    try:
                        rb = bytes.fromhex(row)
                    except Exception:
                        rb = b""
                    rows += rb[:stride].ljust(stride, b"\0")
                rows += b"\0" * max(0, h - len(bitmap)) * stride
                glyphs[enc] = Glyph(
                    enc,
                    w,
                    h,
                    adv if adv is not None else w,
                    xoff,
                    yoff,
                    bytes(rows),
                    stride,
                )
        i += 1

    if fbb:
        _, fh, _, fy = fbb
        if ascent is None:
            ascent = max(0, fh + fy)
        if descent is None:
            descent = max(0, -fy)
    if ascent is None:
        ascent = max((g.height + g.y_offset for g in glyphs.values()), default=0)
    if descent is None:
        descent = max((max(0, -g.y_offset) for g in glyphs.values()), default=0)
    line = max(1, ascent + descent)
    nominal = round(sum(g.advance for g in glyphs.values()) / len(glyphs)) if glyphs else 0
    return Font(source, glyphs, ascent, descent, line, nominal, "bdf")


def bmp8(data: bytes):
    if data[:2] != b"BM":
        raise ValueError("not BMP")
    off = struct.unpack_from("<I", data, 10)[0]
    dib = struct.unpack_from("<I", data, 14)[0]
    w = struct.unpack_from("<i", data, 18)[0]
    hs = struct.unpack_from("<i", data, 22)[0]
    planes, bpp = struct.unpack_from("<HH", data, 26)
    comp = struct.unpack_from("<I", data, 30)[0]
    if planes != 1 or bpp != 8 or comp != 0:
        raise ValueError("newRPL BMP must be uncompressed 8-bit")
    h = abs(hs)
    topdown = hs < 0
    stride = ((w + 3) // 4) * 4
    pal_start = 14 + dib
    palette = [struct.unpack_from("<I", data, pal_start + i * 4)[0] for i in range(256)]
    rows = []
    for logical_y in range(h):
        file_y = logical_y if topdown else h - 1 - logical_y
        start = off + file_y * stride
        rows.append(data[start : start + w])
    marker = rows[-1]
    bitmap_rows = rows[:-1]
    return w, h - 1, palette, marker, bitmap_rows


def parse_map(txt: bytes):
    cp_to_index = {}
    index_codes = []
    used = 0
    for raw in txt.decode("latin-1").splitlines():
        line = raw.split("/", 1)[0].strip()
        if not line:
            continue
        vals = []
        for tok in line.split(","):
            tok = tok.strip()
            if not tok:
                continue
            try:
                cp = int(tok, 0)
            except Exception:
                continue
            if 0 <= cp <= 0x10FFFF:
                vals.append(cp)
        if vals:
            index_codes.append(vals)
            for cp in vals:
                cp_to_index[cp] = used
            used += 1
    return cp_to_index, index_codes


def parse_newrpl_bmp(pkg: Package, path: str) -> Font:
    w, h, palette, marker, rows = bmp8(pkg.data(path))
    offsets = []
    widths = []
    prev = None
    for x, value in enumerate(marker):
        if x == 0 or value != prev:
            if offsets:
                widths.append(x - offsets[-1])
            offsets.append(x)
            prev = value
    if offsets:
        widths.append(w - offsets[-1])

    txtpath = os.path.splitext(path)[0] + ".txt"
    cp_to_index, _ = parse_map(pkg.data(txtpath))
    if cp_to_index and max(cp_to_index.values()) >= len(offsets):
        raise ValueError(f"{path}: mapping references glyph beyond {len(offsets)} bitmap runs")

    glyphs = {}
    for cp, idx in cp_to_index.items():
        x0 = offsets[idx]
        gw = widths[idx]
        stride = (gw + 7) // 8
        bits = bytearray(stride * h)
        for yy, row in enumerate(rows):
            for xx in range(gw):
                color = palette[row[x0 + xx]]
                if color == 0:
                    bits[yy * stride + (xx >> 3)] |= 0x80 >> (xx & 7)
        glyphs[cp] = Glyph(cp, gw, h, gw, 0, 0, bytes(bits), stride)

    nominal = round(sum(g.advance for g in glyphs.values()) / len(glyphs)) if glyphs else 0
    return Font(path, glyphs, h, 0, h, nominal, "bmp")


def _pillow_font(data: bytes, size: int, index: int = 0):
    from PIL import ImageFont

    return ImageFont.truetype(io.BytesIO(data), size=size, index=index)


def raster_pillow_bitmap(
    data: bytes,
    source: str,
    *,
    size: int = 12,
    index: int = 0,
    codepoints=None,
    fmt: str,
    face: str = "",
) -> Font:
    font = _pillow_font(data, size, index)
    asc, desc = font.getmetrics()
    if codepoints is None:
        codepoints = range(256)
    glyphs = {}
    for cp in codepoints:
        ch = chr(cp)
        try:
            # Use an explicit left-baseline anchor. Pillow's historical default
            # horizontal anchor is left-ascender; spelling the baseline out here
            # makes the conversion to BDF-style y_offset direct and testable.
            mask, (x0, y0) = font.getmask2(ch, mode="1", anchor="ls")
            adv = round(font.getlength(ch))
        except Exception:
            continue
        gw, gh = mask.size
        stride = (gw + 7) // 8
        raw = bytes(mask)
        if len(raw) != gw * gh:
            raise ValueError(
                f"{source}: U+{cp:04X}: Pillow mask bytes {len(raw)} != geometry {gw}x{gh}"
            )
        bits = bytearray(stride * gh)
        for y in range(gh):
            for x in range(gw):
                if raw[y * gw + x]:
                    bits[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
        glyphs[cp] = Glyph(
            cp,
            gw,
            gh,
            max(0, adv),
            x0,
            -(y0 + gh),
            bytes(bits),
            stride,
        )

    nominal = round(sum(g.advance for g in glyphs.values()) / len(glyphs)) if glyphs else 0
    return Font(source, glyphs, asc, desc, asc + desc, nominal, fmt, face)


def fon_faces(data: bytes, source: str):
    return [
        raster_pillow_bitmap(
            data,
            source,
            size=size,
            index=idx,
            codepoints=range(256),
            fmt="fon",
            face=f"strike-{idx}",
        )
        for idx, size in enumerate((8, 11, 13))
    ]


# These aliases are custom-font convenience defaults only. hpclang_font_default()
# and the native mini/big accessors read the active calculator ROM instead.
ALIASES = {
    "@mini": "newrpl/bitmap-masters/Font6_mediumA.bmp",
    "@normal": "newrpl/bitmap-masters/Font8_StyleC.bmp",
    "@large": "newrpl/bitmap-masters/Font10_StyleA.bmp",
}


def split_selector(selector: str):
    selector = ALIASES.get(selector, selector)
    if "#" in selector:
        base, face = selector.rsplit("#", 1)
        return base, face
    return selector, None


FON_FACE_SIZES = ((0, 8, "6x10"), (1, 11, "7x13"), (2, 13, "8x16"))


def load_selected(
    pkg: Package,
    selector: str,
    mode: str,
    requested: set[int] | None = None,
):
    path, face = split_selector(selector)
    ext = os.path.splitext(path)[1].lower()
    data = pkg.data(path)

    if ext in (".bdf", ".bmp"):
        full = parse_bdf(data, path) if ext == ".bdf" else parse_newrpl_bmp(pkg, path)
        if mode == "FULL":
            return full
        requested = requested or set()
        glyphs = {cp: glyph for cp, glyph in full.glyphs.items() if cp in requested}
        nominal = (
            round(sum(g.advance for g in glyphs.values()) / len(glyphs))
            if glyphs
            else full.nominal_advance
        )
        return Font(
            path,
            glyphs,
            full.ascent,
            full.descent,
            full.line_height,
            nominal,
            full.format,
            full.face,
        )

    if ext == ".fon":
        if face is None:
            idx, size, label = FON_FACE_SIZES[0]
        else:
            match = None
            for item in FON_FACE_SIZES:
                if face in (str(item[0]), item[2]):
                    match = item
                    break
            if match is None:
                raise ValueError(
                    f"{path}: unknown FON face {face!r}; use #0/#1/#2 or #6x10/#7x13/#8x16"
                )
            idx, size, label = match
        chosen = (
            range(256)
            if mode == "FULL"
            else sorted(cp for cp in (requested or set()) if 0 <= cp <= 255)
        )
        return raster_pillow_bitmap(
            data,
            path,
            size=size,
            index=idx,
            codepoints=chosen,
            fmt="fon",
            face=label,
        )

    raise ValueError(f"unsupported font resource: {path}")


def c_ident(name: str):
    ident = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if not ident or ident[0].isdigit():
        ident = "_" + ident
    return ident


def emit_c(font: Font, symbol: str, out):
    symbol = c_ident(symbol)
    glyphs = sorted(font.glyphs.values(), key=lambda glyph: glyph.codepoint)
    print("#include <stdint.h>", file=out)
    print("#include <hpclang/font-package.h>", file=out)
    print("", file=out)
    ptrs = []
    for n, glyph in enumerate(glyphs):
        bitmap_name = f"{symbol}_bitmap_{n}"
        glyph_name = f"{symbol}_glyph_{n}"
        data = ", ".join(f"0x{byte:02x}" for byte in glyph.bitmap) or "0"
        print(f"static const uint8_t {bitmap_name}[] = {{ {data} }};", file=out)
        print(f"static const struct hpclang_bitmap_glyph {glyph_name} = {{", file=out)
        print(
            f"    0x{glyph.codepoint:08x}u, {bitmap_name}, {glyph.x_offset}, {glyph.y_offset},",
            file=out,
        )
        print(
            f"    {glyph.width}u, {glyph.height}u, {glyph.advance}u, {glyph.stride}u",
            file=out,
        )
        print("};", file=out)
        ptrs.append("&" + glyph_name)
    print("", file=out)
    print(
        f"static const struct hpclang_bitmap_glyph *const {symbol}_glyphs[] = {{",
        file=out,
    )
    for ptr in ptrs:
        print(f"    {ptr},", file=out)
    print("};", file=out)
    fallback = 0x3F if 0x3F in font.glyphs else (glyphs[0].codepoint if glyphs else 0)
    print(f"static const struct hpclang_bitmap_font_data {symbol}_data = {{", file=out)
    print(
        f"    {symbol}_glyphs, {len(glyphs)}u, 0x{fallback:08x}u,",
        file=out,
    )
    print(
        f"    {font.ascent}, {font.descent}, {font.line_height}u, {font.nominal_advance}u",
        file=out,
    )
    print("};", file=out)
    print(
        f"const struct hpclang_font {symbol} = HPCLANG_BITMAP_FONT({symbol}_data);",
        file=out,
    )


def catalog(pkg: Package):
    rows = []
    for entry in pkg.fonts():
        path = entry["path"]
        ext = os.path.splitext(path)[1].lower()[1:]
        if ext == "fon":
            for idx, _, label in FON_FACE_SIZES:
                rows.append(
                    {
                        "selector": f"{path}#{idx}",
                        "path": path,
                        "format": ext,
                        "face": label,
                        "family": entry.get("family"),
                    }
                )
        else:
            rows.append(
                {
                    "selector": path,
                    "path": path,
                    "format": ext,
                    "face": None,
                    "family": entry.get("family"),
                }
            )
    return rows


def main():
    parser = argparse.ArgumentParser(
        description="Inspect or explicitly compile fonts from the canonical source ZIP."
    )
    parser.add_argument("--package", default="runtime/fonts/fonts.zip")
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("verify")
    sub.add_parser("audit")
    sub.add_parser("catalog")
    inspect = sub.add_parser("inspect")
    inspect.add_argument("font")
    compile_parser = sub.add_parser("compile")
    compile_parser.add_argument("font")
    compile_parser.add_argument(
        "--mode",
        choices=("STATIC_EXPLICIT", "FULL"),
        default="STATIC_EXPLICIT",
        help="STATIC_AUTO belongs to hpclang_target_font() because it scans target sources",
    )
    compile_parser.add_argument("--symbol", required=True)
    compile_parser.add_argument("--text", action="append", default=[])
    compile_parser.add_argument("--charset-file", action="append", default=[])
    compile_parser.add_argument("--output", required=True)
    args = parser.parse_args()

    pkg = Package(args.package)
    if args.cmd == "verify":
        bad = pkg.verify()
        if bad:
            print("\n".join(bad), file=sys.stderr)
            return 1
        print(
            f"PASS: {len(pkg.manifest['files'])} source files verified; "
            f"{len(pkg.fonts())} font resources"
        )
    elif args.cmd == "audit":
        failed = []
        faces = 0
        for entry in pkg.fonts():
            path = entry["path"]
            ext = os.path.splitext(path)[1].lower()
            try:
                data = pkg.data(path)
                if ext == ".bdf":
                    font = parse_bdf(data, path)
                    faces += 1
                    if not font.glyphs:
                        raise ValueError("no glyphs")
                elif ext == ".bmp":
                    font = parse_newrpl_bmp(pkg, path)
                    faces += 1
                    if not font.glyphs:
                        raise ValueError("no glyphs")
                elif ext == ".fon":
                    for idx, size, label in FON_FACE_SIZES:
                        font = raster_pillow_bitmap(
                            data,
                            path,
                            size=size,
                            index=idx,
                            codepoints=(0x20, 0x3F, 0x41),
                            fmt="fon",
                            face=label,
                        )
                        if not font.glyphs:
                            raise ValueError(f"strike {idx} has no sample glyphs")
                        faces += 1
                else:
                    raise ValueError("unsupported extension")
            except Exception as exc:
                failed.append((path, repr(exc)))
        for path, error in failed:
            print(f"FAIL {path}: {error}")
        print(
            f"resources={len(pkg.fonts())} faces={faces} "
            f"passed={len(pkg.fonts()) - len(failed)} failed={len(failed)}"
        )
        return bool(failed)
    elif args.cmd == "catalog":
        json.dump(
            {
                "resources": len(pkg.fonts()),
                "faces": catalog(pkg),
                "aliases": ALIASES,
            },
            sys.stdout,
            ensure_ascii=False,
            indent=2,
        )
        print()
    elif args.cmd == "inspect":
        path, face = split_selector(args.font)
        ext = os.path.splitext(path)[1].lower()
        if ext == ".fon" and face is None:
            fonts = fon_faces(pkg.data(path), path)
        else:
            fonts = [load_selected(pkg, args.font, "FULL")]
        for font in fonts:
            print(
                f"{font.source} {font.face} fmt={font.format} "
                f"glyphs={len(font.glyphs)} metrics="
                f"{font.ascent}/{font.descent}/{font.line_height} "
                f"nominal={font.nominal_advance}"
            )
    elif args.cmd == "compile":
        requested = set()
        for text in args.text:
            requested.update(ord(ch) for ch in text)
        for file_path in args.charset_file:
            requested.update(ord(ch) for ch in Path(file_path).read_text(encoding="utf-8"))
        if args.mode == "STATIC_EXPLICIT" and not requested:
            parser.error("STATIC_EXPLICIT compile requires --text or --charset-file")
        font = load_selected(pkg, args.font, args.mode, requested)
        if not font.glyphs:
            raise SystemExit(f"no requested glyphs are present in {args.font}")
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("w", encoding="utf-8") as out:
            emit_c(font, args.symbol, out)
        print(f"{output}: {args.font} {args.mode} -> {len(font.glyphs)} glyphs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
