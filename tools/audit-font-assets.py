#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path


LOCALES = ("ja", "ko", "latin", "zh_cn", "zh_hk", "zh_tr", "zh_tw")
LOCALE_RE = re.compile(r"^(?P<prefix>.+)-(?P<locale>" + "|".join(LOCALES) + r")(?P<suffix>\.[^.]+)$")


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
    for source in row["sources"]:
        if source.endswith("_full.S"):
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
            if cp in glyphs:
                issues.append(f"{source}:{lineno}: duplicate cp {cp}")
                continue
            glyphs[cp] = (xoff, yoff, width, height, advance, stride, bitmap)
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


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit canonical font faces and checked-in generated assembly semantically."
    )
    parser.add_argument("--package", default="runtime/fonts/fonts.zip")
    parser.add_argument("--generated-dir", default="runtime/fonts/generated")
    parser.add_argument("--family", default=None, help="limit audit to one catalog family")
    parser.add_argument("--json", dest="json_path", default=None, help="write machine report")
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
            and not any(mismatches[name] for name in ("missing_count", "extra_count", "changed_count"))
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
            }
        )

    locale_buckets = defaultdict(list)
    for face in faces:
        if face["locale_group"]:
            locale_buckets[face["locale_group"]].append(face)

    locale_groups = []
    for key, members in sorted(locale_buckets.items()):
        cp_hashes = {face["codepoint_sha256"] for face in members}
        payload_hashes = {face["semantic_sha256"] for face in members}
        if len(payload_hashes) == 1:
            classification = "exact-duplicate"
        elif len(cp_hashes) == 1:
            classification = "same-codepoints-different-payload"
        else:
            classification = "different-codepoints"
        subsets = grouped_hashes(members, "semantic_sha256")
        locale_groups.append(
            {
                "group": key,
                "classification": classification,
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

    exact_duplicates = grouped_hashes(faces, "semantic_sha256")
    report = {
        "schema": "hpclang-font-asset-audit-v1",
        "family": args.family,
        "face_count": len(faces),
        "generated_parity": {
            "passed": len(faces) - failures,
            "failed": failures,
        },
        "exact_duplicate_groups": exact_duplicates,
        "locale_variant_groups": locale_groups,
        "faces": faces,
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
    for group in locale_groups:
        print(f"{group['classification']}: {group['group']}")
        for subset in group["exact_duplicate_subsets"]:
            locales = [
                face["locale"]
                for face in group["faces"]
                if face["selector"] in subset["selectors"]
            ]
            print(f"  exact subset: {', '.join(locale for locale in locales if locale)}")

    if args.json_path:
        output = Path(args.json_path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"report={output}")

    if failures:
        print(f"FAIL: {failures} generated face(s) differ from canonical source", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
