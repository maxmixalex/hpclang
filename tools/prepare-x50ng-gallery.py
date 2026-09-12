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
        "#define L1_STAGE_VA 0x07f00240u\n"
        "#define L1_BASE_VA 0x07f00000u\n"
        "#define FLASH_ALIAS_FIRST_L1_INDEX 0x0a0u\n"
        "#define FLASH_ALIAS_LAST_L1_INDEX 0x0ffu\n"
        "#define FLASH_ALIAS_L1_COUNT \\\n"
        "    (FLASH_ALIAS_LAST_L1_INDEX - FLASH_ALIAS_FIRST_L1_INDEX + 1u)\n"
        "#define FLASH_ALIAS_DESCRIPTOR 0x00000c12u\n"
        "#define BOOT_PLATFORM_COOKIE_VA 0x0110000cu\n"
        "#define L1_INJECT_VA 0x07f00c00u",
        "dynamic flash alias diagnostic constants",
    )
    text = replace_once(
        text,
        "    uint32_t old_stage;\n    uint32_t old_inject;",
        "    uint32_t old_stage;\n"
        "    uint32_t old_flash_alias[FLASH_ALIAS_L1_COUNT];\n"
        "    uint32_t old_inject;",
        "dynamic flash alias stock snapshot",
    )
    text = replace_once(
        text,
        "static int snapshot_stock(struct state *s)\n"
        "{\n"
        "    if (!guest_read32(s->hdw, L1_DATA_VA, &s->old_data) ||\n"
        "        !guest_read32(s->hdw, L1_CODE_VA, &s->old_code) ||\n"
        "        !guest_read32(s->hdw, L1_STAGE_VA, &s->old_stage) ||\n"
        "        !guest_read32(s->hdw, L1_INJECT_VA, &s->old_inject) ||\n"
        "        !guest_read32(s->hdw, SWI_VECTOR_VA, &s->old_swi))\n"
        "        return 0;\n"
        "    if (s->old_inject != 0u)\n"
        "        return 0;\n"
        "    snapshot_cpu(&s->stock, s->hdw->cpu);\n"
        "    return 1;\n"
        "}",
        "static int snapshot_stock(struct state *s)\n"
        "{\n"
        "    if (!guest_read32(s->hdw, L1_DATA_VA, &s->old_data) ||\n"
        "        !guest_read32(s->hdw, L1_CODE_VA, &s->old_code) ||\n"
        "        !guest_read32(s->hdw, L1_STAGE_VA, &s->old_stage) ||\n"
        "        !guest_read32(s->hdw, L1_INJECT_VA, &s->old_inject) ||\n"
        "        !guest_read32(s->hdw, SWI_VECTOR_VA, &s->old_swi))\n"
        "        return 0;\n"
        "    for (uint32_t i = 0u; i < FLASH_ALIAS_L1_COUNT; ++i) {\n"
        "        uint32_t index = FLASH_ALIAS_FIRST_L1_INDEX + i;\n"
        "        if (!guest_read32(\n"
        "                s->hdw, L1_BASE_VA + index * 4u,\n"
        "                &s->old_flash_alias[i]))\n"
        "            return 0;\n"
        "    }\n"
        "    if (s->old_inject != 0u)\n"
        "        return 0;\n"
        "    snapshot_cpu(&s->stock, s->hdw->cpu);\n"
        "    return 1;\n"
        "}",
        "dynamic flash alias stock snapshot loop",
    )
    text = replace_once(
        text,
        "static void write_json(struct state *s, const char *status)\n{",
        "struct flash_alias_diag {\n"
        "    uint32_t selected_index;\n"
        "    uint32_t selected_va;\n"
        "    uint32_t selected_stock;\n"
        "    uint32_t selected_current;\n"
        "    uint32_t first_changed_index;\n"
        "    uint32_t first_changed_stock;\n"
        "    uint32_t first_changed_current;\n"
        "    uint32_t changed_count;\n"
        "    uint32_t platform_cookie;\n"
        "    int platform_cookie_valid;\n"
        "};\n"
        "\n"
        "static void scan_flash_alias(\n"
        "    struct state *s, struct flash_alias_diag *diag)\n"
        "{\n"
        "    diag->selected_index = 0xffffffffu;\n"
        "    diag->selected_va = 0u;\n"
        "    diag->selected_stock = 0xffffffffu;\n"
        "    diag->selected_current = 0xffffffffu;\n"
        "    diag->first_changed_index = 0xffffffffu;\n"
        "    diag->first_changed_stock = 0xffffffffu;\n"
        "    diag->first_changed_current = 0xffffffffu;\n"
        "    diag->changed_count = 0u;\n"
        "    diag->platform_cookie = 0xffffffffu;\n"
        "    diag->platform_cookie_valid = guest_read32(\n"
        "        s->hdw, BOOT_PLATFORM_COOKIE_VA, &diag->platform_cookie);\n"
        "\n"
        "    for (uint32_t i = 0u; i < FLASH_ALIAS_L1_COUNT; ++i) {\n"
        "        uint32_t index = FLASH_ALIAS_FIRST_L1_INDEX + i;\n"
        "        uint32_t current = 0xffffffffu;\n"
        "        if (!guest_read32(s->hdw, L1_BASE_VA + index * 4u, &current))\n"
        "            continue;\n"
        "        uint32_t stock = s->old_flash_alias[i];\n"
        "        if (current != stock) {\n"
        "            ++diag->changed_count;\n"
        "            if (diag->first_changed_index == 0xffffffffu) {\n"
        "                diag->first_changed_index = index;\n"
        "                diag->first_changed_stock = stock;\n"
        "                diag->first_changed_current = current;\n"
        "            }\n"
        "        }\n"
        "        if (diag->selected_index == 0xffffffffu &&\n"
        "            stock == 0u && current == FLASH_ALIAS_DESCRIPTOR) {\n"
        "            diag->selected_index = index;\n"
        "            diag->selected_va = index << 20;\n"
        "            diag->selected_stock = stock;\n"
        "            diag->selected_current = current;\n"
        "        }\n"
        "    }\n"
        "}\n"
        "\n"
        "static void write_json(struct state *s, const char *status)\n{",
        "dynamic flash alias scanner",
    )
    text = replace_once(
        text,
        "static void write_json(struct state *s, const char *status)\n"
        "{\n"
        "    FILE *f = fopen(s->json, \"w\");",
        "static void write_json(struct state *s, const char *status)\n"
        "{\n"
        "    struct flash_alias_diag alias;\n"
        "    scan_flash_alias(s, &alias);\n"
        "    FILE *f = fopen(s->json, \"w\");",
        "dynamic flash alias JSON scan",
    )
    text = replace_once(
        text,
        "        \"  \\\"return_settle_observations\\\":%u,\\n\"\n"
        "        \"  \\\"pc\\\":\\\"0x%08x\\\"\\n\"",
        "        \"  \\\"return_settle_observations\\\":%u,\\n\"\n"
        "        \"  \\\"alias_selected_index\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"alias_selected_va\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"alias_selected_stock\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"alias_selected_current\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"alias_changed_count\\\":%u,\\n\"\n"
        "        \"  \\\"alias_first_changed_index\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"alias_first_changed_stock\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"alias_first_changed_current\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"platform_cookie_valid\\\":%s,\\n\"\n"
        "        \"  \\\"platform_cookie\\\":\\\"0x%08x\\\",\\n\"\n"
        "        \"  \\\"pc\\\":\\\"0x%08x\\\"\\n\"",
        "dynamic flash alias JSON fields",
    )
    text = replace_once(
        text,
        "        s->return_quiescence.observations,\n"
        "        s->hdw->cpu->regs[15]);",
        "        s->return_quiescence.observations,\n"
        "        alias.selected_index,\n"
        "        alias.selected_va,\n"
        "        alias.selected_stock,\n"
        "        alias.selected_current,\n"
        "        alias.changed_count,\n"
        "        alias.first_changed_index,\n"
        "        alias.first_changed_stock,\n"
        "        alias.first_changed_current,\n"
        "        alias.platform_cookie_valid ? \"true\" : \"false\",\n"
        "        alias.platform_cookie,\n"
        "        s->hdw->cpu->regs[15]);",
        "dynamic flash alias JSON arguments",
    )
    text = replace_once(
        text,
        "static void fail(struct state *s, const char *why)\n"
        "{\n"
        "    s->failed = 1;\n"
        "    fprintf(stderr, \"Font Gallery E2E FAIL: %s\\n\", why);",
        "static void fail(struct state *s, const char *why)\n"
        "{\n"
        "    struct flash_alias_diag alias;\n"
        "    scan_flash_alias(s, &alias);\n"
        "    s->failed = 1;\n"
        "    fprintf(stderr,\n"
        "        \"Font Gallery E2E FAIL: %s \"\n"
        "        \"(alias-index=0x%08x va=0x%08x desc=0x%08x \"\n"
        "        \"changed=%u first-change=0x%08x:0x%08x->0x%08x \"\n"
        "        \"cookie=%s0x%08x)\\n\",\n"
        "        why, alias.selected_index, alias.selected_va,\n"
        "        alias.selected_current, alias.changed_count,\n"
        "        alias.first_changed_index, alias.first_changed_stock,\n"
        "        alias.first_changed_current,\n"
        "        alias.platform_cookie_valid ? \"\" : \"unreadable/\",\n"
        "        alias.platform_cookie);",
        "dynamic flash alias failure log",
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
