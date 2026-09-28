#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import hashlib
import html
import importlib.util
import io
import json
import re
import statistics
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path


LOCALES = (
    "ja",
    "ko",
    "latin",
    "zh_cn",
    "zh_hk",
    "zh_tr",
    "zh_tw",
    "zh_hans",
    "zh_hant",
)
LOCALE_RE = re.compile(
    r"^(?P<prefix>.+)-(?P<locale>" + "|".join(LOCALES) + r")(?P<suffix>\.[^.]+)$"
)
FOCUS_FAMILIES = {
    "ark-pixel",
    "boutique-bitmap-7x7",
    "boutique-bitmap-9x9",
    "dina",
    "hasankazan",
    "japanyoshi",
    "pixeldroid",
    "tamzen",
}
QUICK_TEXT = "Quick"
ASCII_TEST = "AaBbCc 0123456789 %#"
LCD_WIDTH = 131


def load_font_tool():
    path = Path(__file__).with_name("hpclang-font.py")
    spec = importlib.util.spec_from_file_location("hpclang_font_tool", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


hf = load_font_tool()


def codepoint_hash(codepoints: list[int]) -> str:
    h = hashlib.sha256()
    for cp in codepoints:
        h.update(struct.pack("<I", cp))
    return h.hexdigest()


def semantic_hash(metrics: tuple[int, int, int, int], glyphs: dict[int, tuple]) -> str:
    h = hashlib.sha256()
    h.update(struct.pack("<iiii", *metrics))
    for cp in sorted(glyphs):
        xoff, yoff, width, height, advance, stride, bitmap = glyphs[cp]
        h.update(
            struct.pack(
                "<IiiiiiiI",
                cp,
                xoff,
                yoff,
                width,
                height,
                advance,
                stride,
                len(bitmap),
            )
        )
        h.update(bitmap)
    return h.hexdigest()


def source_semantics(font):
    glyphs = {
        cp: (
            glyph.x_offset,
            glyph.y_offset,
            glyph.width,
            glyph.height,
            glyph.advance,
            glyph.stride,
            glyph.bitmap,
        )
        for cp, glyph in font.glyphs.items()
    }
    metrics = (font.ascent, font.descent, font.line_height, font.nominal_advance)
    return metrics, glyphs


def parse_generated_codepoints(path: Path) -> list[int]:
    text = path.read_text(encoding="utf-8")
    return [int(match.group(1)) for match in re.finditer(r"(?m)^\s+(\d+)\s*$", text)]


def parse_generated_glyphs(row: dict, generated_dir: Path):
    glyphs: dict[int, tuple] = {}
    issues: list[str] = []
    descriptor_only = f"{row['id']}_full.S" if len(row["sources"]) > 1 else None
    for source in row["sources"]:
        # Chunked faces use <id>_full.S only for the descriptor. Unchunked
        # faces whose id itself ends in _full use <id>.S for glyphs+descriptor;
        # do not classify those by suffix alone.
        if descriptor_only is not None and source == descriptor_only:
            continue
        path = generated_dir / source
        if not path.exists():
            issues.append(f"missing source {source}")
            continue
        for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            line = raw.strip()
            if not line.startswith("G "):
                continue
            parts = line[2:].split(",")
            if len(parts) < 8:
                issues.append(f"{source}:{lineno}: malformed G record")
                continue
            try:
                cp, xoff, yoff, width, height, advance, stride = map(int, parts[:7])
                raw_bitmap = bytes(int(value) for value in parts[7:])
            except (ValueError, OverflowError) as exc:
                issues.append(f"{source}:{lineno}: invalid G record: {exc}")
                continue
            expected = max(0, stride) * max(0, height)
            if expected and len(raw_bitmap) != expected:
                issues.append(
                    f"{source}:{lineno}: cp {cp} bitmap bytes {len(raw_bitmap)} != {expected}"
                )
                continue
            bitmap = raw_bitmap[:expected] if expected else b""
            payload = (xoff, yoff, width, height, advance, stride, bitmap)
            if cp in glyphs:
                if glyphs[cp] != payload:
                    issues.append(f"{source}:{lineno}: conflicting duplicate cp {cp}")
                continue
            glyphs[cp] = payload
    return glyphs, issues


def mismatch_summary(source: dict[int, tuple], generated: dict[int, tuple], limit: int = 20):
    source_cps = set(source)
    generated_cps = set(generated)
    missing = sorted(source_cps - generated_cps)
    extra = sorted(generated_cps - source_cps)
    changed = sorted(cp for cp in source_cps & generated_cps if source[cp] != generated[cp])
    return {
        "missing_count": len(missing),
        "extra_count": len(extra),
        "changed_count": len(changed),
        "missing_sample": missing[:limit],
        "extra_sample": extra[:limit],
        "changed_sample": changed[:limit],
    }


def locale_group(selector: str):
    path = Path(selector)
    match = LOCALE_RE.match(path.name)
    if not match:
        return None, None
    key_name = match.group("prefix") + "-<locale>" + match.group("suffix")
    return str(path.with_name(key_name)).replace("\\", "/"), match.group("locale")


def grouped_hashes(faces: list[dict], key: str):
    groups = defaultdict(list)
    for face in faces:
        groups[face[key]].append(face["selector"])
    return [
        {"sha256": digest, "selectors": sorted(selectors)}
        for digest, selectors in sorted(groups.items())
        if len(selectors) > 1
    ]


def bitmap_blank(glyph: tuple) -> bool:
    _xoff, _yoff, width, height, _advance, _stride, bitmap = glyph
    return width <= 0 or height <= 0 or not bitmap or not any(bitmap)


def glyph_visual_signature(glyph: tuple) -> tuple:
    _xoff, _yoff, width, height, _advance, stride, bitmap = glyph
    return (width, height, stride, bitmap)


def percentile(values: list[int], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    pos = (len(ordered) - 1) * fraction
    lo = int(pos)
    hi = min(len(ordered) - 1, lo + 1)
    if lo == hi:
        return float(ordered[lo])
    weight = pos - lo
    return ordered[lo] * (1.0 - weight) + ordered[hi] * weight


def repertoire_counts(codepoints: list[int]) -> dict[str, int]:
    def count_ranges(*ranges):
        return sum(any(lo <= cp <= hi for lo, hi in ranges) for cp in codepoints)

    return {
        "ascii_printable": count_ranges((0x20, 0x7E)),
        "latin_extended": count_ranges((0x80, 0x024F)),
        "greek": count_ranges((0x0370, 0x03FF)),
        "kana": count_ranges((0x3040, 0x30FF)),
        "hangul": count_ranges((0xAC00, 0xD7AF)),
        "han": count_ranges((0x3400, 0x9FFF), (0xF900, 0xFAFF)),
        "arrows_math": count_ranges((0x2190, 0x22FF)),
        "box_block": count_ranges((0x2500, 0x259F)),
        "misc_symbols": count_ranges((0x2600, 0x27BF)),
        "private_use": count_ranges((0xE000, 0xF8FF)),
    }


def choose_spread(codepoints: list[int], limit: int) -> list[int]:
    cps = sorted(set(codepoints))
    if len(cps) <= limit:
        return cps
    if limit <= 1:
        return [cps[len(cps) // 2]]
    result = []
    for i in range(limit):
        index = round(i * (len(cps) - 1) / (limit - 1))
        cp = cps[index]
        if cp not in result:
            result.append(cp)
    return result


def feature_codepoints(glyphs: dict[int, tuple], limit: int = 24) -> list[int]:
    cps = sorted(glyphs)
    buckets = [
        [cp for cp in cps if 0xE000 <= cp <= 0xF8FF],
        [cp for cp in cps if 0x3400 <= cp <= 0x9FFF or 0xF900 <= cp <= 0xFAFF],
        [cp for cp in cps if 0x3040 <= cp <= 0x30FF],
        [cp for cp in cps if 0xAC00 <= cp <= 0xD7AF],
        [cp for cp in cps if 0x2190 <= cp <= 0x22FF],
        [cp for cp in cps if 0x2600 <= cp <= 0x27BF],
        [cp for cp in cps if 0x80 <= cp <= 0x024F],
    ]
    out: list[int] = []
    per_bucket = (8, 6, 4, 4, 4, 4, 4)
    for bucket, cap in zip(buckets, per_bucket):
        for cp in choose_spread(bucket, cap):
            if cp not in out:
                out.append(cp)
                if len(out) >= limit:
                    return out
    for cp in choose_spread(cps, limit):
        if cp not in out:
            out.append(cp)
            if len(out) >= limit:
                break
    return out


def analyze_face(source_glyphs: dict[int, tuple], selector: str, family: str | None):
    printable = list(range(0x20, 0x7F))
    present = [cp for cp in printable if cp in source_glyphs]
    missing = [cp for cp in printable if cp not in source_glyphs]
    blank = [cp for cp in present if bitmap_blank(source_glyphs[cp])]
    signatures = Counter(glyph_visual_signature(source_glyphs[cp]) for cp in present)
    dominant = max(signatures.values(), default=0)
    distinct = len(signatures)

    quick_missing = [ord(ch) for ch in QUICK_TEXT if ord(ch) not in source_glyphs]
    quick_width = None
    if not quick_missing:
        quick_width = sum(source_glyphs[ord(ch)][4] for ch in QUICK_TEXT)

    spacing_values = []
    for cp in present:
        if cp == 0x20 or bitmap_blank(source_glyphs[cp]):
            continue
        _xo, _yo, width, _height, advance, _stride, _bitmap = source_glyphs[cp]
        spacing_values.append(advance - width)
    spacing = {
        "mean_advance_minus_width": round(statistics.fmean(spacing_values), 2)
        if spacing_values
        else None,
        "median_advance_minus_width": statistics.median(spacing_values) if spacing_values else None,
        "p90_advance_minus_width": round(percentile(spacing_values, 0.9), 2)
        if spacing_values
        else None,
        "max_advance_minus_width": max(spacing_values) if spacing_values else None,
    }

    signals: list[str] = []
    if quick_width is not None and quick_width > LCD_WIDTH:
        signals.append("quick-overflow")
    if len(present) >= 16 and dominant / len(present) >= 0.80:
        signals.append("glyph-collapse")
    if present and len(blank) / len(present) >= 0.25:
        signals.append("blank-heavy")
    if missing:
        signals.append("ascii-missing")
    if not present and any(0xE000 <= cp <= 0xF8FF for cp in source_glyphs):
        signals.append("glyph-only/private")
    if spacing_values and statistics.median(spacing_values) >= 2:
        signals.append("loose-spacing")
    if family in FOCUS_FAMILIES:
        signals.append("user-focus")
    if family == "tamzen" and "powerline" in selector.lower():
        signals.append("powerline-variant")

    return {
        "quick_text": QUICK_TEXT,
        "quick_width": quick_width,
        "quick_missing": quick_missing,
        "quick_fits_lcd": quick_width is not None and quick_width <= LCD_WIDTH,
        "lcd_width": LCD_WIDTH,
        "ascii": {
            "present": len(present),
            "missing": len(missing),
            "missing_sample": missing[:20],
            "blank": len(blank),
            "blank_sample": blank[:20],
            "distinct_visual_glyphs": distinct,
            "dominant_visual_glyph_count": dominant,
        },
        "spacing": spacing,
        "repertoire": repertoire_counts(sorted(source_glyphs)),
        "feature_codepoints": feature_codepoints(source_glyphs),
        "signals": signals,
    }


def glyph_changed_across(members: list[dict], cp: int) -> bool:
    payloads = []
    for face in members:
        glyphs = face["_source_glyphs"]
        payloads.append(glyphs.get(cp))
    return any(payload != payloads[0] for payload in payloads[1:])


def enrich_locale_groups(faces: list[dict]):
    buckets = defaultdict(list)
    for face in faces:
        if face["locale_group"]:
            buckets[face["locale_group"]].append(face)

    groups = []
    for key, members in sorted(buckets.items()):
        cp_hashes = {face["codepoint_sha256"] for face in members}
        payload_hashes = {face["semantic_sha256"] for face in members}
        if len(payload_hashes) == 1:
            classification = "exact-duplicate"
        elif len(cp_hashes) == 1:
            classification = "same-codepoints-different-payload"
        else:
            classification = "different-codepoints"
        subsets = grouped_hashes(members, "semantic_sha256")
        all_cps = sorted(set().union(*(set(face["_source_glyphs"]) for face in members)))
        changed = [cp for cp in all_cps if glyph_changed_across(members, cp)]
        changed_sample = choose_spread(changed, 20)
        for face in members:
            face["locale_diff"] = {
                "group": key,
                "classification": classification,
                "changed_count": len(changed),
                "changed_sample": changed_sample,
            }
        groups.append(
            {
                "group": key,
                "classification": classification,
                "changed_count": len(changed),
                "changed_sample": changed_sample,
                "faces": [
                    {
                        "locale": face["locale"],
                        "selector": face["selector"],
                        "glyph_count": face["glyph_count"],
                        "codepoint_sha256": face["codepoint_sha256"],
                        "semantic_sha256": face["semantic_sha256"],
                    }
                    for face in sorted(members, key=lambda item: item["selector"])
                ],
                "exact_duplicate_subsets": subsets,
            }
        )
    return groups


def codepoint_label(cps: list[int]) -> str:
    return " ".join(f"U+{cp:04X}" for cp in cps)


def bitmap_pixel(bitmap: bytes, stride: int, x: int, y: int) -> bool:
    offset = y * stride + (x >> 3)
    return offset < len(bitmap) and (bitmap[offset] & (0x80 >> (x & 7))) != 0


def render_strip(glyphs: dict[int, tuple], cps: list[int], advances: bool = True) -> str | None:
    if not cps:
        return None
    from PIL import Image

    selected = [(cp, glyphs.get(cp)) for cp in cps]
    selected = [(cp, glyph) for cp, glyph in selected if glyph is not None]
    if not selected:
        return None
    height = max(max(1, glyph[3]) for _cp, glyph in selected) + 6
    widths = []
    for _cp, glyph in selected:
        width = glyph[4] if advances else max(glyph[2], glyph[4])
        widths.append(max(1, width) + 4)
    width = min(900, max(1, sum(widths) + 4))
    image = Image.new("L", (width, height), 255)
    x_cursor = 2
    for (_cp, glyph), cell_width in zip(selected, widths):
        _xoff, _yoff, gw, gh, _advance, stride, bitmap = glyph
        if x_cursor >= width:
            break
        draw_x = x_cursor + 2
        draw_y = max(1, (height - gh) // 2)
        for yy in range(max(0, gh)):
            for xx in range(max(0, gw)):
                if draw_x + xx >= width:
                    break
                if bitmap_pixel(bitmap, max(0, stride), xx, yy):
                    image.putpixel((draw_x + xx, draw_y + yy), 0)
        x_cursor += cell_width
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=True)
    return "data:image/png;base64," + base64.b64encode(output.getvalue()).decode("ascii")


def text_codepoints(text: str, glyphs: dict[int, tuple]) -> list[int]:
    return [ord(ch) for ch in text if ord(ch) in glyphs]


def html_report(report: dict, faces: list[dict], output: Path) -> None:
    exact_by_selector = {}
    for number, group in enumerate(report["exact_duplicate_groups"], 1):
        for selector in group["selectors"]:
            exact_by_selector.setdefault(selector, []).append(number)

    review = []
    for face in faces:
        signals = list(face["visual_audit"]["signals"])
        if face["selector"] in exact_by_selector:
            signals.insert(0, "exact-duplicate")
        if signals:
            row = dict(face)
            row["_signals"] = signals
            review.append(row)

    def badge(signal: str) -> str:
        severity = (
            "bad" if signal in {"quick-overflow", "glyph-collapse", "blank-heavy"} else "warn"
        )
        if signal == "user-focus":
            severity = "info"
        return f'<span class="badge {severity}">{html.escape(signal)}</span>'

    out = ["<!doctype html><meta charset='utf-8'><title>hpclang font asset audit</title>"]
    out.append("""<style>
:root{font-family:ui-sans-serif,system-ui,sans-serif;color-scheme:light dark}body{margin:0;background:#111;color:#eee}main{max-width:1700px;margin:auto;padding:26px}h1{margin:0 0 6px}.sub{color:#aaa;line-height:1.45}.stats{display:flex;gap:10px;flex-wrap:wrap;margin:18px 0}.stat{border:1px solid #333;background:#1a1a1a;border-radius:10px;padding:9px 12px}.stat b{font-size:20px}.legend{border:1px solid #333;background:#171717;padding:12px;border-radius:10px;line-height:1.55;margin-bottom:20px}table{border-collapse:collapse;width:100%;font-size:12px}th{position:sticky;top:0;background:#171717;z-index:3}th,td{border-bottom:1px solid #303030;padding:7px;vertical-align:top;text-align:left}.selector{max-width:340px;overflow-wrap:anywhere;color:#aaa}.shot{image-rendering:pixelated;background:#fff;border:1px solid #555;max-width:360px;min-height:18px}.badge{display:inline-block;margin:1px 3px 1px 0;padding:2px 5px;border-radius:5px;border:1px solid #555}.bad{color:#ffb4a9}.warn{color:#ffd28a}.info{color:#9ed0ff}.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}.small{color:#aaa;font-size:11px}.group{white-space:nowrap}</style><main>""")
    out.append("<h1>hpclang Font Asset Audit</h1>")
    out.append(
        "<div class='sub'>Canonical source → generated parity, full semantic duplicates, spacing/coverage signals, repertoire previews, and actual locale-difference glyphs. This report recommends review; it does not delete fonts.</div>"
    )
    stats = [
        (report["face_count"], "faces"),
        (f"{report['generated_parity']['passed']}/{report['face_count']}", "generated parity"),
        (len(report["exact_duplicate_groups"]), "exact duplicate groups"),
        (
            sum(1 for face in faces if "quick-overflow" in face["visual_audit"]["signals"]),
            "Quick > 131px",
        ),
        (
            sum(1 for face in faces if "glyph-collapse" in face["visual_audit"]["signals"]),
            "glyph collapse",
        ),
        (
            sum(1 for face in faces if "blank-heavy" in face["visual_audit"]["signals"]),
            "blank-heavy",
        ),
        (len(review), "review rows"),
    ]
    out.append(
        "<div class='stats'>"
        + "".join(
            f"<div class='stat'><b>{html.escape(str(v))}</b><br>{html.escape(k)}</div>"
            for v, k in stats
        )
        + "</div>"
    )
    out.append(
        "<div class='legend'><b>Policy signals.</b> <span class='bad'>quick-overflow</span>: ‘Quick’ exceeds the 131px LCD. <span class='bad'>glyph-collapse</span>: ≥80% of present printable ASCII resolves to one visual glyph. <span class='bad'>blank-heavy</span>: ≥25% of present printable ASCII is blank. <span class='warn'>ascii-missing</span>: printable ASCII codepoints are absent. <span class='warn'>loose-spacing</span>: median (advance − bitmap width) ≥2px; this is diagnostic, not an automatic defect. <span class='warn'>powerline-variant</span>: Tamzen Powerline-specific asset. Locale rows render codepoints that actually differ across locale faces, rather than fixed samples.</div>"
    )
    out.append(
        "<table><thead><tr><th>Face</th><th>Signals / metrics</th><th>Quick / ASCII</th><th>Distinctive repertoire</th><th>Locale differences</th><th>Source</th></tr></thead><tbody>"
    )
    for face in review:
        va = face["visual_audit"]
        glyphs = face["_source_glyphs"]
        signals = " ".join(badge(signal) for signal in face["_signals"])
        exact = exact_by_selector.get(face["selector"], [])
        if exact:
            signals += "<div class='small'>duplicate group " + ", ".join(map(str, exact)) + "</div>"
        quick = render_strip(glyphs, text_codepoints(ASCII_TEST, glyphs))
        feature_cps = va["feature_codepoints"]
        feature = render_strip(glyphs, feature_cps, advances=False)
        locale = face.get("locale_diff") or {}
        locale_cps = locale.get("changed_sample", [])
        locale_img = render_strip(glyphs, locale_cps, advances=False)
        qwidth = va["quick_width"] if va["quick_width"] is not None else "—"
        spacing = va["spacing"]
        metrics = (
            f"<div class='mono'>Quick {qwidth}px / 131</div>"
            f"<div>glyphs {face['glyph_count']} · line {face['metrics']['line_height']}px</div>"
            f"<div>ASCII present {va['ascii']['present']}/95 · missing {va['ascii']['missing']} · blank {va['ascii']['blank']} · distinct {va['ascii']['distinct_visual_glyphs']} · dominant {va['ascii']['dominant_visual_glyph_count']}</div>"
            f"<div>advance−width median {spacing['median_advance_minus_width']} · p90 {spacing['p90_advance_minus_width']} · max {spacing['max_advance_minus_width']}</div>"
        )
        rep = va["repertoire"]
        rep_text = " · ".join(f"{k} {v}" for k, v in rep.items() if v)
        locale_text = ""
        if locale:
            locale_text = f"<div>{html.escape(locale.get('classification', ''))}: {locale.get('changed_count', 0)} differing cps</div>"
            if locale_cps:
                locale_text += (
                    f"<div class='small mono'>{html.escape(codepoint_label(locale_cps))}</div>"
                )
        out.append("<tr>")
        out.append(
            f"<td><b>{html.escape(str(face.get('family') or ''))}</b><br>{html.escape(face['selector'].split('/')[-1])}</td>"
        )
        out.append(f"<td>{signals}{metrics}</td>")
        out.append(
            f"<td>{f'<img class=shot src="{quick}">' if quick else '—'}<div class='small mono'>{html.escape(ASCII_TEST)}</div></td>"
        )
        out.append(
            f"<td>{f'<img class=shot src="{feature}">' if feature else '—'}<div class='small'>{html.escape(rep_text)}</div><div class='small mono'>{html.escape(codepoint_label(feature_cps))}</div></td>"
        )
        out.append(
            f"<td>{f'<img class=shot src="{locale_img}">' if locale_img else ''}{locale_text or '—'}</td>"
        )
        out.append(f"<td class='selector'>{html.escape(face['selector'])}</td>")
        out.append("</tr>")
    out.append("</tbody></table></main>")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("".join(out), encoding="utf-8")


def public_face(face: dict) -> dict:
    return {key: value for key, value in face.items() if not key.startswith("_")}


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit canonical font faces and checked-in generated assembly semantically."
    )
    parser.add_argument("--package", default="runtime/fonts/fonts.zip")
    parser.add_argument("--generated-dir", default="runtime/fonts/generated")
    parser.add_argument("--family", default=None, help="limit audit to one catalog family")
    parser.add_argument("--json", dest="json_path", default=None, help="write machine report")
    parser.add_argument(
        "--html", dest="html_path", default=None, help="write self-contained visual review report"
    )
    args = parser.parse_args()

    package = hf.Package(args.package)
    generated_dir = Path(args.generated_dir)
    catalog_path = generated_dir / "catalog.json"
    generated_catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    rows = [
        row
        for row in generated_catalog["faces"]
        if args.family is None or row.get("family") == args.family
    ]
    if not rows:
        raise SystemExit(f"no generated faces matched family={args.family!r}")

    faces: list[dict] = []
    failures = 0
    for row in rows:
        selector = row["selector"]
        font = hf.load_selected(package, selector, "FULL")
        metrics, source_glyphs = source_semantics(font)
        source_cps = sorted(source_glyphs)
        source_cp_hash = codepoint_hash(source_cps)
        source_hash = semantic_hash(metrics, source_glyphs)

        cp_path = generated_dir / f"{row['id']}.codepoints.cmake"
        generated_cps = parse_generated_codepoints(cp_path) if cp_path.exists() else []
        generated_glyphs, parse_issues = parse_generated_glyphs(row, generated_dir)
        generated_metrics = (
            int(row["ascent"]),
            int(row["descent"]),
            int(row["line_height"]),
            int(row["nominal_advance"]),
        )
        generated_hash = semantic_hash(generated_metrics, generated_glyphs)
        mismatches = mismatch_summary(source_glyphs, generated_glyphs)
        parity = (
            not parse_issues
            and metrics == generated_metrics
            and source_cps == generated_cps
            and source_hash == generated_hash
            and not any(
                mismatches[name] for name in ("missing_count", "extra_count", "changed_count")
            )
        )
        if not parity:
            failures += 1

        group, locale = locale_group(selector)
        faces.append(
            {
                "id": row["id"],
                "selector": selector,
                "family": row.get("family"),
                "locale_group": group,
                "locale": locale,
                "glyph_count": len(source_cps),
                "metrics": {
                    "ascent": metrics[0],
                    "descent": metrics[1],
                    "line_height": metrics[2],
                    "nominal_advance": metrics[3],
                },
                "codepoint_sha256": source_cp_hash,
                "semantic_sha256": source_hash,
                "generated_semantic_sha256": generated_hash,
                "generated_parity": parity,
                "generated_issues": parse_issues,
                "generated_mismatches": mismatches,
                "visual_audit": analyze_face(source_glyphs, selector, row.get("family")),
                "_source_glyphs": source_glyphs,
            }
        )

    locale_groups = enrich_locale_groups(faces)
    exact_duplicates = grouped_hashes(faces, "semantic_sha256")
    report = {
        "schema": "hpclang-font-asset-audit-v2",
        "family": args.family,
        "face_count": len(faces),
        "generated_parity": {
            "passed": len(faces) - failures,
            "failed": failures,
        },
        "exact_duplicate_groups": exact_duplicates,
        "locale_variant_groups": locale_groups,
        "visual_summary": {
            "quick_overflow": sum(
                1 for face in faces if "quick-overflow" in face["visual_audit"]["signals"]
            ),
            "glyph_collapse": sum(
                1 for face in faces if "glyph-collapse" in face["visual_audit"]["signals"]
            ),
            "blank_heavy": sum(
                1 for face in faces if "blank-heavy" in face["visual_audit"]["signals"]
            ),
            "ascii_missing": sum(
                1 for face in faces if "ascii-missing" in face["visual_audit"]["signals"]
            ),
            "loose_spacing": sum(
                1 for face in faces if "loose-spacing" in face["visual_audit"]["signals"]
            ),
            "powerline_variant": sum(
                1 for face in faces if "powerline-variant" in face["visual_audit"]["signals"]
            ),
        },
        "faces": [public_face(face) for face in faces],
    }

    counts = defaultdict(int)
    for group in locale_groups:
        counts[group["classification"]] += 1
    print(
        f"faces={len(faces)} generated_parity={len(faces) - failures}/{len(faces)} "
        f"exact_duplicate_sets={len(exact_duplicates)} locale_groups={len(locale_groups)}"
    )
    for classification in (
        "exact-duplicate",
        "same-codepoints-different-payload",
        "different-codepoints",
    ):
        print(f"locale_{classification}={counts[classification]}")
    print(
        "visual " + " ".join(f"{name}={value}" for name, value in report["visual_summary"].items())
    )
    for group in locale_groups:
        print(
            f"{group['classification']}: {group['group']} "
            f"changed_codepoints={group['changed_count']}"
        )
        for subset in group["exact_duplicate_subsets"]:
            locales = [
                face["locale"] for face in group["faces"] if face["selector"] in subset["selectors"]
            ]
            print(f"  exact subset: {', '.join(locale for locale in locales if locale)}")
    for number, group in enumerate(exact_duplicates, 1):
        print(f"exact_duplicate_group_{number}: {' = '.join(group['selectors'])}")

    if args.json_path:
        output = Path(args.json_path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"report={output}")

    if args.html_path:
        output = Path(args.html_path)
        html_report(report, faces, output)
        print(f"visual_report={output}")

    if failures:
        print(f"FAIL: {failures} generated face(s) differ from canonical source", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
