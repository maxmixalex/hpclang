#!/usr/bin/env python3
"""Add the focused Font Gallery frontend to an already prepared x50ng tree."""

# Changes to generated font rasters intentionally retrigger this focused Gallery gate.
# Catalog cardinality is derived from generated assets; source curation must not
# require a hard-coded face count in this x50ng frontend.
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
        "#define BOOT_PLATFORM_COOKIE_VA 0x0110000cu\n"
        "#define HP39_NOR_PROBE_OFFSET 0x0004335au\n"
        "#define L1_INJECT_VA 0x07f00c00u",
        "HP39 alias diagnostic constants",
    )
    text = replace_once(
        text,
        "    uint32_t old_stage;\n    uint32_t old_inject;",
        "    uint32_t old_stage;\n"
        "    uint32_t old_flash_alias[FLASH_ALIAS_L1_COUNT];\n"
        "    uint32_t old_inject;",
        "HP39 alias stock snapshot",
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
        "HP39 alias stock snapshot loop",
    )
    text = replace_once(
        text,
        "static void write_json(struct state *s, const char *status)\n{",
        "struct flash_alias_diag {\n"
        "    uint32_t cookie;\n"
        "    uint32_t index;\n"
        "    uint32_t stock_descriptor;\n"
        "    uint32_t current_descriptor;\n"
        "    uint32_t probe_va;\n"
        "    uint8_t virtual_byte;\n"
        "    uint8_t physical_byte;\n"
        "    int cookie_valid;\n"
        "    int descriptor_valid;\n"
        "    int virtual_probe_valid;\n"
        "};\n"
        "\n"
        "static void scan_flash_alias(\n"
        "    struct state *s, struct flash_alias_diag *diag)\n"
        "{\n"
        "    memset(diag, 0, sizeof(*diag));\n"
        "    diag->cookie = 0xffffffffu;\n"
        "    diag->index = 0xffffffffu;\n"
        "    diag->stock_descriptor = 0xffffffffu;\n"
        "    diag->current_descriptor = 0xffffffffu;\n"
        "    diag->cookie_valid = guest_read32(\n"
        "        s->hdw, BOOT_PLATFORM_COOKIE_VA, &diag->cookie);\n"
        "\n"
        "    if (diag->cookie_valid &&\n"
        "        (diag->cookie & 0x000fffffu) == 0u) {\n"
        "        uint32_t index = diag->cookie >> 20;\n"
        "        if (index >= FLASH_ALIAS_FIRST_L1_INDEX &&\n"
        "            index <= FLASH_ALIAS_LAST_L1_INDEX) {\n"
        "            diag->index = index;\n"
        "            diag->stock_descriptor =\n"
        "                s->old_flash_alias[index - FLASH_ALIAS_FIRST_L1_INDEX];\n"
        "            diag->descriptor_valid = guest_read32(\n"
        "                s->hdw, L1_BASE_VA + index * 4u,\n"
        "                &diag->current_descriptor);\n"
        "            diag->probe_va = diag->cookie + HP39_NOR_PROBE_OFFSET;\n"
        "            diag->virtual_probe_valid = cpu_memory_rw_debug(\n"
        "                s->hdw->cpu, diag->probe_va,\n"
        "                &diag->virtual_byte, 1, 0) == 0;\n"
        "        }\n"
        "    }\n"
        "\n"
        "    cpu_physical_memory_read(\n"
        "        HP39_NOR_PROBE_OFFSET, &diag->physical_byte, 1);\n"
        "}\n"
        "\n"
        "static void write_json(struct state *s, const char *status)\n{",
        "HP39 alias diagnostic helper",
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
        "    uint32_t boot_diag[5] = {\n"
        "        0xffffffffu, 0xffffffffu, 0xffffffffu,\n"
        "        0xffffffffu, 0xffffffffu,\n"
        "    };\n"
        "    int boot_diag_valid = 1;\n"
        "    for (uint32_t i = 0u; i < 5u; ++i) {\n"
        "        if (!guest_read32(\n"
        "                s->hdw, 0x01100020u + i * 4u, &boot_diag[i])) {\n"
        "            boot_diag_valid = 0;\n"
        "            break;\n"
        "        }\n"
        "    }\n"
        "    scan_flash_alias(s, &alias);\n"
        "    s->failed = 1;\n"
        "    fprintf(stderr,\n"
        "        \"Font Gallery E2E FAIL: %s \"\n"
        "        \"(pc=0x%08x lr=0x%08x dfsr=0x%08x dfar=0x%08x \"\n"
        "        \"cookie=%s0x%08x index=0x%08x l1=%s0x%08x \"\n"
        "        \"stock=0x%08x probe=0x%08x virt=%s0x%02x phys=0x%02x \"\n"
        "        \"boot=%s[%08x,%08x,%08x,%08x,%08x])\\n\",\n"
        "        why, s->hdw->cpu->regs[15], s->hdw->cpu->regs[14],\n"
        "        s->hdw->cpu->cp15.c5_data, s->hdw->cpu->cp15.c6_data,\n"
        "        alias.cookie_valid ? \"\" : \"unreadable/\", alias.cookie,\n"
        "        alias.index,\n"
        "        alias.descriptor_valid ? \"\" : \"unreadable/\",\n"
        "        alias.current_descriptor, alias.stock_descriptor,\n"
        "        alias.probe_va,\n"
        "        alias.virtual_probe_valid ? \"\" : \"unreadable/\",\n"
        "        alias.virtual_byte, alias.physical_byte,\n"
        "        boot_diag_valid ? \"\" : \"unreadable/\",\n"
        "        boot_diag[0], boot_diag[1], boot_diag[2],\n"
        "        boot_diag[3], boot_diag[4]);",
        "HP39 alias failure log",
    )
    text = replace_once(
        text,
        "    if (timer_get_clock() >= s->deadline) {\n"
        "        fail(s, \"timeout\");\n"
        "        return;\n"
        "    }\n\n"
        "    if (guest_read32(s->hdw, s->result_addr, &v)) {",
        "    if (timer_get_clock() >= s->deadline) {\n"
        "        fail(s, \"timeout\");\n"
        "        return;\n"
        "    }\n\n"
        "    if (!s->saw_pass && s->hdw->cpu->regs[15] == 0x00000010u) {\n"
        "        fail(s, \"data-abort\");\n"
        "        return;\n"
        "    }\n\n"
        "    if (guest_read32(s->hdw, s->result_addr, &v)) {",
        "Data Abort fast-fail",
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
