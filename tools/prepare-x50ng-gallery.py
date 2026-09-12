#!/usr/bin/env python3
"""Add the focused Font Gallery frontend to an already prepared x50ng tree."""

from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(text: str, old: str, new: str, what: str) -> str:
    if text.count(old) != 1:
        raise SystemExit(f"Font Gallery x50ng patch: expected one {what} anchor")
    return text.replace(old, new, 1)


def copy_frontend(source: Path, destination: Path) -> None:
    text = source.read_text()
    text = replace_once(
        text,
        "#define TEST_RAM_SIZE 0x00100000u",
        "#define TEST_RAM_SIZE 0x00200000u",
        "reserved test RAM size",
    )
    text = replace_once(
        text,
        "#define STAGE_PAGES 96u",
        "#define STAGE_PAGES 224u",
        "synthetic stage page count",
    )
    text = replace_once(
        text,
        "#define INJECT_PAGES 159u",
        "#define INJECT_PAGES 224u",
        "synthetic launcher page count",
    )
    text = replace_once(
        text,
        "#define L1_STAGE_VA 0x07f00240u\n#define L1_INJECT_VA 0x07f00c00u",
        "#define L1_STAGE_VA 0x07f00240u\n#define L1_FLASH_ALIAS_VA 0x07f00280u\n#define L1_INJECT_VA 0x07f00c00u",
        "flash alias L1 diagnostic address",
    )
    text = replace_once(
        text,
        "    uint32_t old_stage;\n    uint32_t old_inject;",
        "    uint32_t old_stage;\n    uint32_t old_flash_alias;\n    uint32_t old_inject;",
        "flash alias snapshot field",
    )
    text = replace_once(
        text,
        "        !guest_read32(s->hdw, L1_STAGE_VA, &s->old_stage) ||\n        !guest_read32(s->hdw, L1_INJECT_VA, &s->old_inject) ||",
        "        !guest_read32(s->hdw, L1_STAGE_VA, &s->old_stage) ||\n        !guest_read32(s->hdw, L1_FLASH_ALIAS_VA, &s->old_flash_alias) ||\n        !guest_read32(s->hdw, L1_INJECT_VA, &s->old_inject) ||",
        "flash alias stock snapshot",
    )
    text = replace_once(
        text,
        "    snapshot_cpu(&s->stock, s->hdw->cpu);\n    return 1;",
        "    snapshot_cpu(&s->stock, s->hdw->cpu);\n    fprintf(stderr, \"Font Gallery alias L1 stock=0x%08x\\n\",\n        s->old_flash_alias);\n    return 1;",
        "flash alias snapshot log",
    )
    text = replace_once(
        text,
        "static void write_json(struct state *s, const char *status)\n{\n    FILE *f = fopen(s->json, \"w\");",
        "static void write_json(struct state *s, const char *status)\n{\n    uint32_t current_flash_alias = 0xffffffffu;\n    (void)guest_read32(s->hdw, L1_FLASH_ALIAS_VA, &current_flash_alias);\n    FILE *f = fopen(s->json, \"w\");",
        "flash alias current snapshot",
    )
    text = replace_once(
        text,
        "        \"  \\\"return_settle_observations\\\":%u,\\n\"\n        \"  \\\"pc\\\":\\\"0x%08x\\\"\\n\"",
        "        \"  \\\"return_settle_observations\\\":%u,\\n\"\n        \"  \\\"stock_alias_l1\\\":\\\"0x%08x\\\",\\n\"\n        \"  \\\"current_alias_l1\\\":\\\"0x%08x\\\",\\n\"\n        \"  \\\"pc\\\":\\\"0x%08x\\\"\\n\"",
        "flash alias JSON fields",
    )
    text = replace_once(
        text,
        "        s->return_quiescence.observations,\n        s->hdw->cpu->regs[15]);",
        "        s->return_quiescence.observations,\n        s->old_flash_alias,\n        current_flash_alias,\n        s->hdw->cpu->regs[15]);",
        "flash alias JSON arguments",
    )
    text = replace_once(
        text,
        "static void fail(struct state *s, const char *why)\n{\n    s->failed = 1;\n    fprintf(stderr, \"Font Gallery E2E FAIL: %s\\n\", why);",
        "static void fail(struct state *s, const char *why)\n{\n    uint32_t current_flash_alias = 0xffffffffu;\n    (void)guest_read32(s->hdw, L1_FLASH_ALIAS_VA, &current_flash_alias);\n    s->failed = 1;\n    fprintf(stderr,\n        \"Font Gallery E2E FAIL: %s (alias stock=0x%08x current=0x%08x)\\n\",\n        why, s->old_flash_alias, current_flash_alias);",
        "flash alias failure log",
    )
    text = replace_once(
        text,
        "int main(int argc, char **argv)",
        "int hpclang_gallery_main(int argc, char **argv)",
        "frontend main()",
    )
    destination.write_text(text)


def expand_reserved_ram(root: Path) -> None:
    path = root / "src" / "module.c"
    text = path.read_text()
    text = replace_once(
        text,
        "phys_ram_size += 0x00100000;",
        "phys_ram_size += 0x00200000;",
        "reserved hpclang E2E RAM",
    )
    path.write_text(text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--gallery-frontend", type=Path, required=True)
    args = parser.parse_args()

    root = args.x50ng
    expand_reserved_ram(root)
    copy_frontend(args.gallery_frontend, root / "src" / "hpclang_gallery.c")

    makefile = root / "Makefile"
    lines = makefile.read_text().splitlines(keepends=True)
    matches = []
    for index, line in enumerate(lines):
        logical = line.strip()
        if logical.endswith("\\"):
            logical = logical[:-1].rstrip()
        if logical.endswith("src/main.c"):
            matches.append(index)
    if len(matches) != 1:
        raise SystemExit(
            f"{makefile}: expected one src/main.c source line, found {len(matches)}"
        )

    index = matches[0]
    original = lines[index]
    leading = original[: len(original) - len(original.lstrip())]
    continuation = chr(92)
    lines[index:index + 1] = [
        f"{leading}./src/hpclang_gallery.c {continuation}\n",
        original,
    ]
    makefile.write_text("".join(lines))


if __name__ == "__main__":
    main()
