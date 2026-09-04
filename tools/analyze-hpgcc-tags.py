#!/usr/bin/env python3
from __future__ import annotations

import difflib
import hashlib
import json
import re
import sys
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit("usage: analyze-hpgcc-tags.py HPGCC11 HPGCC20 OUT")

r11, r20, out = map(Path, sys.argv[1:])
out.mkdir(parents=True, exist_ok=True)

FOCUS_PREFIXES = (
    "sources/crt0/",
    "sources/hplib/",
    "sources/hpg/",
    "sources/ggl/",
    "sources/win/",
    "sources/fsystem/",
    "elf2hp/",
    "examples/",
)

KEYS = [
    "sources/crt0/Makefile",
    "sources/crt0/MMUloader.c",
    "sources/crt0/MMUglobals.c",
    "sources/crt0/MMUld.script",
    "sources/crt0/VCloader.c",
    "sources/crt0/VCld.script",
    "sources/crt0/crt0.c",
    "sources/Makefile",
    "sources/README.TXT",
    "elf2hp/elf2hp.c",
]

FLAG_RE = re.compile(r"(?<![A-Za-z0-9_])-(?:m|f)[A-Za-z0-9_.=+\\-]+")
ASMREG_RE = re.compile(r"asm\\s*\\(\\s*[\"']([^\"']+)[\"']\\s*\\)")
HEX_RE = re.compile(r"0x[0-9A-Fa-f]{5,8}")
LINK_RE = re.compile(r"\\b(?:ENTRY|STARTUP|INPUT|OUTPUT_ARCH|OUTPUT_FORMAT)\\s*\\([^\\n]+")
GLOBAL_RE = re.compile(r"\\b__[A-Za-z0-9_]+\\b")

def files(root: Path):
    d = {}
    for p in root.rglob("*"):
        if p.is_file() and ".svn" not in p.parts:
            rel = p.relative_to(root).as_posix()
            b = p.read_bytes()
            d[rel] = {
                "size": len(b),
                "sha256": hashlib.sha256(b).hexdigest(),
            }
    return d

def read_text(path: Path):
    if not path.exists():
        return None
    return path.read_text(errors="replace")

m11, m20 = files(r11), files(r20)
allp = sorted(set(m11) | set(m20))
only11 = [p for p in allp if p not in m20]
only20 = [p for p in allp if p not in m11]
changed = [p for p in allp if p in m11 and p in m20 and m11[p]["sha256"] != m20[p]["sha256"]]
same = [p for p in allp if p in m11 and p in m20 and m11[p]["sha256"] == m20[p]["sha256"]]

(out / "inventory-1.1.json").write_text(json.dumps(m11, indent=2, sort_keys=True))
(out / "inventory-2.0.json").write_text(json.dumps(m20, indent=2, sort_keys=True))

focus_only11 = [p for p in only11 if p.startswith(FOCUS_PREFIXES)]
focus_only20 = [p for p in only20 if p.startswith(FOCUS_PREFIXES)]
focus_changed = [p for p in changed if p.startswith(FOCUS_PREFIXES)]

summary = [
    "# Official SourceForge tag comparison",
    "",
    f"- HPGCC 1.1 tag files: {len(m11)}",
    f"- HPGCC 2.0 VC tag files: {len(m20)}",
    f"- identical paths/content: {len(same)}",
    f"- changed common paths: {len(changed)}",
    f"- only in 1.1: {len(only11)}",
    f"- only in 2.0: {len(only20)}",
    "",
    "Tags compared:",
    "- frozen_1.1_MMU_trunk",
    "- frozen_2.0_VC_release",
    "",
    "## Calculator/runtime-focused file changes",
    "",
    f"- changed: {len(focus_changed)}",
    f"- removed from 2.0: {len(focus_only11)}",
    f"- added in 2.0: {len(focus_only20)}",
    "",
    "### Added in 2.0",
    "",
]
summary += [f"- {p}" for p in focus_only20[:300]] or ["- none"]
summary += ["", "### Removed from 2.0", ""]
summary += [f"- {p}" for p in focus_only11[:300]] or ["- none"]
summary += ["", "### Changed common files", ""]
summary += [f"- {p}" for p in focus_changed[:500]] or ["- none"]

summary += ["", "## Key file fingerprints", ""]
for key in KEYS:
    a = m11.get(key)
    b = m20.get(key)
    summary.append(f"### {key}")
    summary.append(f"- 1.1: {a['size']} bytes, {a['sha256']}" if a else "- 1.1: absent")
    summary.append(f"- 2.0: {b['size']} bytes, {b['sha256']}" if b else "- 2.0: absent")
    summary.append("")

summary += ["## Compiler/codegen flags found in build files and READMEs", ""]
for label, root in (("1.1", r11), ("2.0", r20)):
    found = set()
    for rel in ["sources/Makefile", "sources/crt0/Makefile", "sources/README.TXT"]:
        t = read_text(root / rel)
        if t:
            found.update(FLAG_RE.findall(t))
    summary += [f"### {label}", "", " ".join(sorted(found)), ""]

summary += ["## Runtime-specific tokens from crt0", ""]
for label, root in (("1.1", r11), ("2.0", r20)):
    summary += [f"### {label}", ""]
    for rel in [
        "sources/crt0/MMUloader.c",
        "sources/crt0/VCloader.c",
        "sources/crt0/MMUglobals.c",
        "sources/crt0/MMUld.script",
        "sources/crt0/VCld.script",
    ]:
        t = read_text(root / rel)
        if not t:
            continue
        regs = sorted(set(ASMREG_RE.findall(t)))
        hexes = sorted(set(HEX_RE.findall(t)))
        links = sorted(set(LINK_RE.findall(t)))
        globals_ = sorted(set(GLOBAL_RE.findall(t)))
        summary.append(f"- {rel}")
        if regs:
            summary.append("  - explicit asm registers: " + ", ".join(regs))
        if hexes:
            summary.append("  - fixed addresses: " + ", ".join(hexes[:40]))
        if links:
            summary.append("  - linker directives: " + "; ".join(links[:20]))
        if globals_:
            summary.append("  - double-underscore symbols: " + ", ".join(globals_[:60]))
    summary.append("")

(out / "tag-comparison.md").write_text("\n".join(summary) + "\n")

parts = []
for key in [
    "sources/crt0/Makefile",
    "sources/crt0/MMUloader.c",
    "sources/crt0/MMUglobals.c",
    "sources/crt0/MMUld.script",
    "sources/crt0/VCloader.c",
    "sources/crt0/VCld.script",
    "sources/crt0/crt0.c",
]:
    ta = read_text(r11 / key)
    tb = read_text(r20 / key)
    if ta is None and tb is None:
        continue
    a = [] if ta is None else ta.splitlines(keepends=True)
    b = [] if tb is None else tb.splitlines(keepends=True)
    parts.extend(difflib.unified_diff(a, b, fromfile=f"1.1/{key}", tofile=f"2.0/{key}", n=3))
    parts.append("\n")
(out / "crt0-runtime.diff").write_text("".join(parts))

status = []
for p in allp:
    if p in only11:
        s = "removed"
    elif p in only20:
        s = "added"
    elif p in changed:
        s = "changed"
    else:
        s = "same"
    status.append({"path": p, "status": s})
(out / "path-status.json").write_text(json.dumps(status, indent=2))
