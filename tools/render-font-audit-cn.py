#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
import html
import importlib.util
import io
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

from PIL import Image

LOCALE_LABELS = {
    "latin": "拉丁文字环境",
    "zh_cn": "中文·中国大陆",
    "zh_hk": "中文·香港",
    "zh_tw": "中文·台湾",
    "zh_tr": "中文·传统印刷体",
    "ja": "日文",
    "ko": "韩文",
}
ARK_RE = re.compile(
    r"ark-pixel-(?P<size>10|12|16)px-(?P<mode>monospaced|proportional)-"
    r"(?P<locale>latin|zh_cn|zh_hk|zh_tw|zh_tr|ja|ko)\.bdf$"
)
COMMON_DIFF_TEXT = (
    "一二三十人口大小天地日月年月上下左右前后东西南北中中国國日本語韩国韓國"
    "你我他她它的是不了在有这這中来來个個们們到说說和地也子时時道出而要于於"
    "就下得可年生自会會那后後能对對着著事其里裡所去行过過家用发發天方然作成"
    "开開面无無小心前本见見经經头頭樣样只种種学學书書文文字体體门門国國臺台"
    "湾灣汉漢龙龍马馬风風云雲广廣东東乐樂长長"
    "。、，！？：；（）「」『』【】《》〈〉·—…～〜ー￥¥₩"
    "あいうえおかきくけこさしすせそたちつてとなにぬねの"
    "アイウエオカキクケコサシスセソタチツテトナニヌネノ"
    "가나다라마바사아자차카타파하한글"
)
ACCENT_TEXT = "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÑÒÓÔÕÖØÙÚÛÜÝßàáâãäåæçèéêëìíîïñòóôõöøùúûüýÿŒœ"
SYMBOL_TEXT = "←↑→↓↔ ±×÷≠≤≥≈∞√ ∑∏∆Ωαβπλ μ°©®€£¥"
PANGRAMS = (
    "Il1| 0OQ 2Z5S 6G8B rn m w MW AV To",
    "aegxy 0123456789 {}[]() <> /\\ _-+= *#@&%",
)
ASCII_UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
ASCII_LOWER = "abcdefghijklmnopqrstuvwxyz"
DIGITS_PUNCT = "0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"


def load_font_tool():
    path = Path(__file__).with_name("hpclang-font.py")
    spec = importlib.util.spec_from_file_location("hpclang_font_tool_cn_report", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


hf = load_font_tool()


def glyph_tuple(glyph):
    return (
        glyph.x_offset,
        glyph.y_offset,
        glyph.width,
        glyph.height,
        glyph.advance,
        glyph.stride,
        bytes(glyph.bitmap),
    )


def glyph_signature(glyph):
    return glyph_tuple(glyph)


def bitmap_pixel(bitmap: bytes, stride: int, x: int, y: int) -> bool:
    offset = y * stride + (x >> 3)
    return offset < len(bitmap) and (bitmap[offset] & (0x80 >> (x & 7))) != 0


def data_uri(image: Image.Image) -> str:
    buf = io.BytesIO()
    image.save(buf, format="PNG", optimize=True)
    return "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode("ascii")


def render_cps(font, cps, *, scale=3, cell_mode=False, max_cps=None):
    if max_cps is not None:
        cps = cps[:max_cps]
    items = [(cp, font.glyphs.get(cp)) for cp in cps]
    if not any(glyph is not None for _cp, glyph in items):
        return None
    max_h = max((max(1, glyph.height) for _cp, glyph in items if glyph is not None), default=1)
    if cell_mode:
        max_w = max((max(1, glyph.width, glyph.advance) for _cp, glyph in items if glyph is not None), default=1)
        cell_w = max_w + 3
        cols = min(24, max(1, len(items)))
        rows = (len(items) + cols - 1) // cols
        img = Image.new("L", (cols * cell_w + 2, rows * (max_h + 3) + 2), 255)
        for idx, (_cp, glyph) in enumerate(items):
            if glyph is None:
                continue
            col, row = idx % cols, idx // cols
            x0 = col * cell_w + 1 + max(0, (max_w - glyph.width) // 2)
            y0 = row * (max_h + 3) + 1 + max(0, (max_h - glyph.height) // 2)
            for y in range(max(0, glyph.height)):
                for x in range(max(0, glyph.width)):
                    if bitmap_pixel(bytes(glyph.bitmap), max(0, glyph.stride), x, y):
                        img.putpixel((x0 + x, y0 + y), 0)
    else:
        widths = [max(1, (glyph.advance if glyph is not None else 3)) + 1 for _cp, glyph in items]
        img = Image.new("L", (sum(widths) + 4, max_h + 4), 255)
        cursor = 2
        for (_cp, glyph), width in zip(items, widths):
            if glyph is not None:
                x0 = cursor + max(0, glyph.x_offset)
                y0 = 2 + max(0, (max_h - glyph.height) // 2)
                for y in range(max(0, glyph.height)):
                    for x in range(max(0, glyph.width)):
                        if bitmap_pixel(bytes(glyph.bitmap), max(0, glyph.stride), x, y):
                            if x0 + x < img.width:
                                img.putpixel((x0 + x, y0 + y), 0)
            cursor += width
    if scale > 1:
        img = img.resize((img.width * scale, img.height * scale), Image.Resampling.NEAREST)
    return data_uri(img)


def available_text_cps(font, text):
    return [ord(ch) for ch in text if ord(ch) in font.glyphs]


def spread(values, limit):
    values = sorted(set(values))
    if len(values) <= limit:
        return values
    if limit <= 1:
        return values[:1]
    result = []
    for i in range(limit):
        idx = round(i * (len(values) - 1) / (limit - 1))
        cp = values[idx]
        if cp not in result:
            result.append(cp)
    return result


def feature_groups(font):
    cps = sorted(font.glyphs)
    groups = [
        ("拉丁扩展", [cp for cp in cps if 0x80 <= cp <= 0x024F], 72),
        ("希腊 / 西里尔", [cp for cp in cps if 0x0370 <= cp <= 0x052F], 72),
        ("箭头 / 数学", [cp for cp in cps if 0x2190 <= cp <= 0x22FF], 72),
        ("方框 / 图形符号", [cp for cp in cps if 0x2500 <= cp <= 0x27BF], 72),
        ("日文假名", [cp for cp in cps if 0x3040 <= cp <= 0x30FF], 72),
        ("汉字字符集抽样", [cp for cp in cps if 0x3400 <= cp <= 0x9FFF], 96),
        ("韩文", [cp for cp in cps if 0xAC00 <= cp <= 0xD7AF], 72),
        ("私用区 / 图标", [cp for cp in cps if 0xE000 <= cp <= 0xF8FF], 96),
    ]
    return [(label, spread(values, limit), len(values)) for label, values, limit in groups if values]


def face_title(row):
    name = Path(row["selector"]).stem.replace("_", " ").replace("-", " ")
    family = (row.get("family") or "").replace("-", " ")
    return f"{family} · {name}" if family else name


def ark_meta(row):
    match = ARK_RE.match(Path(row["selector"]).name)
    if not match:
        return None
    d = match.groupdict()
    mode_cn = "等宽" if d["mode"] == "monospaced" else "比例"
    d["label"] = f"Ark Pixel {d['size']}px · {mode_cn} · {LOCALE_LABELS[d['locale']]}"
    d["group"] = f"{d['size']}px-{d['mode']}"
    return d


def changed_codepoints(group):
    all_cps = sorted(set().union(*(set(font.glyphs) for _row, font, _meta in group)))
    changed = []
    for cp in all_cps:
        sigs = []
        for _row, font, _meta in group:
            glyph = font.glyphs.get(cp)
            sigs.append(None if glyph is None else glyph_signature(glyph))
        if any(sig != sigs[0] for sig in sigs[1:]):
            changed.append(cp)
    return changed


def common_diff_sample(changed, limit=42):
    # Do not pad this list with arbitrary CJK just to fill a grid. The point of
    # the Ark locale panel is to show differences a human reader is likely to
    # recognize. If a size has only a handful of common-character differences,
    # show only those and say so.
    changed_set = set(changed)
    priority = []
    for ch in COMMON_DIFF_TEXT:
        cp = ord(ch)
        if cp in changed_set and cp not in priority:
            priority.append(cp)
    return priority[:limit]


def img_tag(uri, alt):
    if not uri:
        return "<div class='empty'>这一组没有可显示字符</div>"
    return f"<img class='shot' src='{uri}' alt='{html.escape(alt)}'>"


def render_specimens(font):
    parts = []
    for title, text in [
        ("大写字母", ASCII_UPPER),
        ("小写字母", ASCII_LOWER),
        ("数字与常用标点", DIGITS_PUNCT),
        ("变音拉丁字母", ACCENT_TEXT),
        ("数学 / 箭头 / 常用符号", SYMBOL_TEXT),
    ]:
        cps = available_text_cps(font, text)
        parts.append(
            f"<div class='sample'><b>{title}</b>{img_tag(render_cps(font, cps, scale=3), title)}"
            f"<div class='chars'>{html.escape(''.join(chr(cp) for cp in cps)) or '—'}</div></div>"
        )
    for text in PANGRAMS:
        cps = available_text_cps(font, text)
        parts.append(
            f"<div class='sample wide'><b>正文节奏</b>{img_tag(render_cps(font, cps, scale=3), text)}"
            f"<div class='chars'>{html.escape(text)}</div></div>"
        )
    feature_html = []
    for label, cps, total in feature_groups(font):
        feature_html.append(
            f"<div class='sample'><b>{html.escape(label)} <span class='muted'>({total} 个，本图抽样)</span></b>"
            f"{img_tag(render_cps(font, cps, scale=3, cell_mode=True), label)}"
            f"<div class='chars mono'>{html.escape(' '.join(f'U+{cp:04X}' for cp in cps[:24]))}"
            f"{' …' if len(cps) > 24 else ''}</div></div>"
        )
    return "".join(parts), "".join(feature_html)


def main():
    ap = argparse.ArgumentParser(description="Render a human-oriented Chinese hpclang font review report.")
    ap.add_argument("--package", default="runtime/fonts/fonts.zip")
    ap.add_argument("--generated-dir", default="runtime/fonts/generated")
    ap.add_argument("--audit-json", default="build/font-audit/all-fonts.json")
    ap.add_argument("--html", default="build/font-audit/字体人工审阅报告.html")
    args = ap.parse_args()

    package = hf.Package(args.package)
    catalog = json.loads((Path(args.generated_dir) / "catalog.json").read_text(encoding="utf-8"))
    audit_path = Path(args.audit_json)
    audit = json.loads(audit_path.read_text(encoding="utf-8")) if audit_path.exists() else {}
    audit_by_selector = {face["selector"]: face for face in audit.get("faces", [])}

    rows = catalog["faces"]
    loaded = []
    for row in rows:
        loaded.append((row, hf.load_selected(package, row["selector"], "FULL")))

    ark_groups = defaultdict(list)
    review_families = defaultdict(list)
    for row, font in loaded:
        meta = ark_meta(row)
        if meta:
            ark_groups[meta["group"]].append((row, font, meta))
        if not meta:
            review_families[row.get("family") or "未分类"].append((row, font))

    summary = audit.get("visual_summary", {})
    face_count = len(rows)
    out = ["""<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<title>hpclang 字体人工审阅报告</title>
<style>
:root{font-family:Inter,"Noto Sans SC","Microsoft YaHei",system-ui,sans-serif;color-scheme:dark}
*{box-sizing:border-box}body{margin:0;background:#101114;color:#eceef2}main{max-width:1540px;margin:auto;padding:28px}
h1{font-size:30px;margin:0 0 8px}h2{margin-top:34px;border-bottom:1px solid #333;padding-bottom:8px}
h3{margin:22px 0 9px}.lead{color:#b7bcc8;line-height:1.65;max-width:1100px}
.cards{display:flex;gap:10px;flex-wrap:wrap;margin:18px 0}.card{padding:10px 14px;border:1px solid #30343c;border-radius:10px;background:#181a20}
.card strong{font-size:22px}.good{color:#9bddb2}.warn{color:#ffd37c}.bad{color:#ff9e94}
.notice{padding:12px 14px;border-left:3px solid #7da6ff;background:#171b25;margin:14px 0;line-height:1.65}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(430px,1fr));gap:12px}
.face,.locale{border:1px solid #30343c;background:#17191e;border-radius:12px;padding:14px;overflow:hidden}
.face h4,.locale h4{margin:0 0 10px;font-size:15px}.sample{margin:10px 0}.sample b{display:block;margin-bottom:5px}
.shot{display:block;max-width:100%;height:auto;background:white;border:1px solid #444;image-rendering:pixelated}
.chars{margin-top:4px;color:#aeb4c0;font-size:12px;overflow-wrap:anywhere}.muted{color:#8e95a3;font-weight:400}
.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}.empty{color:#737987;font-size:12px}
details{border:1px solid #30343c;border-radius:12px;background:#15171b;margin:12px 0;padding:10px}
summary{cursor:pointer;font-weight:700}.locale-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:10px;margin-top:10px}
.tag{display:inline-block;border:1px solid #404650;border-radius:6px;padding:2px 6px;margin-right:5px;font-size:12px;color:#cad0da}
.source{font-size:11px;color:#777f8d;overflow-wrap:anywhere;margin-top:8px}
</style><main>"""]
    out.append("<h1>hpclang 字体人工审阅报告</h1>")
    out.append(
        "<p class='lead'>这份报告只解决一个问题：<b>这些字体实际长什么样，哪些值得留下。</b>"
        "机器校验数字不再放在正文中心。每个需要人工判断的位图 face 都给出大小写、数字标点、正文、"
        "变音拉丁字母、数学符号，以及它真实拥有的其他 Unicode 字符抽样。Ark Pixel 则单独比较"
        "同一尺寸/宽度模式下各地区版本真正不同的字形。</p>"
    )
    out.append("<div class='cards'>")
    out.append(f"<div class='card'><strong>{face_count}</strong><br>当前 face 总数</div>")
    if audit:
        parity = audit.get("generated_parity", {})
        out.append(f"<div class='card'><strong>{parity.get('passed','—')}/{face_count}</strong><br>源字体与生成结果一致</div>")
        out.append(f"<div class='card'><strong>{summary.get('quick_overflow',0)}</strong><br>仍然过大</div>")
        out.append(f"<div class='card'><strong>{summary.get('powerline_variant',0)}</strong><br>仍然是 Powerline</div>")
    out.append("</div>")
    out.append(
        "<div class='notice'><b>删除标准：</b>Dina OEM、Powerline、以及 “Quick” 在 131px 屏幕上都放不下的 face "
        "已经由你的明确裁决决定删除；如果这里仍显示数量不为 0，就说明清理没有真正完成，不能把本轮当作通过。</div>"
    )

    out.append("<h2>1. Ark Pixel：地区版本到底差在哪</h2>")
    out.append(
        "<p class='lead'>名字统一写成“Ark Pixel + 像素尺寸 + 等宽/比例 + 地区”。下面每一组都只挑"
        "<b>确实在各地区版本之间发生变化</b>的字符，并优先从常用汉字、常用标点、假名和韩文里找。"
        "如果某一尺寸真正变化的常用字符很少，报告会如实显示，而不是为了凑数量塞一排生僻字。</p>"
    )
    for group_name in sorted(ark_groups, key=lambda s: (int(s.split("px")[0]), s)):
        group = sorted(ark_groups[group_name], key=lambda item: item[2]["locale"])
        changed = changed_codepoints(group)
        common = common_diff_sample(changed)
        common_chars = "".join(chr(cp) for cp in common)
        first_meta = group[0][2]
        mode_cn = "等宽" if first_meta["mode"] == "monospaced" else "比例"
        out.append("<details open>")
        out.append(
            f"<summary>Ark Pixel {first_meta['size']}px · {mode_cn} — "
            f"共 {len(changed)} 个字符存在地区差异，本页只展示常用字符表实际命中的 {len(common)} 个差异字符</summary>"
        )
        if common:
            out.append(f"<div class='chars' style='font-size:16px;margin:10px 0'>本组对比字符：{html.escape(common_chars)}</div>")
        else:
            out.append("<div class='notice'>这一组虽然存在字形差异，但在当前常用字符优先表中没有命中；不拿生僻字冒充“常用差异”。</div>")
        out.append("<div class='locale-grid'>")
        for row, font, meta in group:
            out.append("<div class='locale'>")
            out.append(f"<h4>{html.escape(meta['label'])}</h4>")
            out.append(img_tag(render_cps(font, common, scale=4, cell_mode=True), meta["label"]))
            out.append(f"<div class='source'>{html.escape(row['selector'])}</div></div>")
        out.append("</div></details>")

    out.append("<h2>2. 其他位图字体：逐个 face 看完整得多的样子</h2>")
    out.append(
        "<p class='lead'>这里不再只给一行 AaBbCc。每个位图 face 都至少检查基本拉丁、数字标点、"
        "两句正文、变音拉丁字符、常见数学符号；字体如果还包含 CJK、假名、韩文、图标或其他大字符集，"
        "下面继续给整块抽样。这样 Boutique Bitmap、JapanYoshi、PixelDroid、HasanKazan 等可以按"
        "实际视觉效果决定去留。</p>"
    )
    for family in sorted(review_families):
        faces = sorted(review_families[family], key=lambda item: item[0]["selector"])
        out.append(f"<details open><summary>{html.escape(family)} — {len(faces)} 个 face</summary><div class='grid'>")
        for row, font in faces:
            basic, extras = render_specimens(font)
            audit_face = audit_by_selector.get(row["selector"], {})
            va = audit_face.get("visual_audit", {})
            ascii_info = va.get("ascii", {})
            out.append("<article class='face'>")
            out.append(f"<h4>{html.escape(face_title(row))}</h4>")
            out.append(
                f"<div><span class='tag'>字形 {len(font.glyphs)}</span>"
                f"<span class='tag'>行高 {font.line_height}px</span>"
                f"<span class='tag'>ASCII 缺 {ascii_info.get('missing','—')}</span></div>"
            )
            out.append(basic)
            if extras:
                out.append("<hr style='border:0;border-top:1px solid #2b2e35;margin:14px 0'>")
                out.append(extras)
            out.append(f"<div class='source'>{html.escape(row['selector'])}</div></article>")
        out.append("</div></details>")

    out.append(
        "<h2>3. 怎么用这份报告</h2>"
        "<div class='notice'>先看同一 family 内各个 face 是否真的有可辨认、可用的风格差异；"
        "再看缺字是不是影响常用场景。某个 face 只因为“奇怪”不一定该删，但如果基本拉丁不可读、"
        "风格重复、或者它真正有价值的字符我们根本用不到，就可以下一轮直接裁掉。"
        "本报告不再用机器异常数字替你做这种视觉决定。</div>"
    )
    out.append("</main></html>")
    target = Path(args.html)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("".join(out), encoding="utf-8")
    print(f"cn_visual_report={target}")


if __name__ == "__main__":
    main()
