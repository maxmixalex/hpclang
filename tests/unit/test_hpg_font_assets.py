"""Exercise HPG byte text using actual packaged glyphs and the clean renderer."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SYMBOLS = [
    0x2220,
    0x78,
    0x2207,
    0x221A,
    0x222B,
    0x3A3,
    0x25B6,
    0x3C0,
    0x2202,
    0x2264,
    0x2265,
    0x2260,
    0x3B1,
    0x2192,
    0x2190,
    0x2193,
    0x2191,
    0x3B3,
    0x3B4,
    0x3B5,
    0x3B7,
    0x3B8,
    0x3BB,
    0x3C1,
    0x3C3,
    0x3C4,
    0x3C9,
    0x394,
    0x3A0,
    0x3A9,
    0x25A0,
    0x221E,
    0x20AC,
]


def load_face(name):
    glyphs = {}
    for line in (
        (ROOT / "runtime/fonts/generated" / f"{name}.S").read_text().splitlines()
    ):
        if line.startswith("G "):
            cp, *values = map(int, line[2:].split(","))
            glyphs[cp] = values
    return glyphs


def host_face(name, glyphs):
    """Rebase the checked-in ARM asset's pointers for the host ABI, unchanged pixels."""
    lines = []
    for cp, (xo, yo, w, h, a, stride, *bits) in glyphs.items():
        ident = f"{name}_{cp}"
        lines.append(
            f"static const uint8_t {ident}_bits[] = {{{','.join(map(str, bits))}}};"
        )
        lines.append(
            f"static const struct hpclang_bitmap_glyph {ident} = "
            f"{{{cp},{ident}_bits,{xo},{yo},{w},{h},{a},{stride}}};"
        )
    refs = ",".join(f"&{name}_{cp}" for cp in sorted(glyphs))
    lines.append(
        f"static const struct hpclang_bitmap_glyph *const {name}_list[] = {{{refs}}};"
    )
    height, advance = (6, 4) if name == "newrpl_font6_menus" else (8, 6)
    lines.append(
        f"static const struct hpclang_bitmap_font_data {name}_data = "
        f"{{{name}_list,{len(glyphs)},63,{height},0,{height},{advance}}};"
    )
    lines.append(
        f"const struct hpclang_font hpclang_font_{name}_full = "
        f"HPCLANG_BITMAP_FONT({name}_data);"
    )
    return "\n".join(lines)


DRIVER = r"""
#include <stdio.h>
#include <assert.h>
#include <hpgraphics.h>
#include <hpclang/abi.h>
#include <hpclang/display.h>
#include <hpclang/font-package.h>
#include "saturn-view.h"
static struct hpclang_boot_info boot;
const struct hpclang_boot_info *hpclang_boot_info(void) { return &boot; }
struct hpclang_surface *hpclang_display_surface(void) { return NULL; }
int hpclang_display_begin_pages(uint32_t l, unsigned n) { (void)l; (void)n; return -1; }
int hpclang_display_replace_pages(uint32_t l, unsigned n) { (void)l; (void)n; return -1; }
int hpclang_display_publish(void) { return -1; }
int hpclang_display_present(uint32_t t) { (void)t; return -1; }
void hpclang_display_end(void) {}
int hpclang_display_indicator_set(unsigned s, uint8_t c) { (void)s; (void)c; return -1; }
int hpclang_saturn_read_nibble(struct hpclang_saturn_address a, uint8_t *v)
{ (void)a; (void)v; return 0; }
int hpclang_saturn_read_byte(struct hpclang_saturn_address a, uint8_t *v)
{ (void)a; (void)v; return 0; }
int main(void) {
    hpg_t *image = hpg_alloc_mono_image(8, 8);
    assert(image);
    for (boot.model = 1; boot.model <= 6; ++boot.model)
        for (int large = 0; large < 2; ++large) {
            hpg_set_font(image, large ? hpg_get_bigfont() : hpg_get_minifont());
            for (unsigned code = 0; code < 256; ++code) {
                hpg_clear_on(image);
                hpg_draw_letter_on(image, (char)code, 0, 0);
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                        putchar(hpg_get_pixel(image, x, y) != 0);
            }
        }
    hpg_free_image(image);
    return 0;
}
"""


@unittest.skipUnless(shutil.which("cc"), "host C compiler required")
class HpgFontAssets(unittest.TestCase):
    def test_all_bytes_on_six_models_without_native_mapping(self):
        names = ["newrpl_font6_menus", "newrpl_font8_styleb"]
        faces = [load_face(name) for name in names]
        expected = bytearray()
        for face, width, height in zip(faces, (4, 6), (6, 8)):
            for code in range(256):
                cp = (
                    SYMBOLS[code - 128]
                    if 128 <= code <= 160
                    else 0x3B2
                    if code == 223
                    else 0x25A0
                    if code < 32
                    else 0x2592
                    if code == 127
                    else code
                )
                if code == 128:
                    pixels = {(x, height - 2) for x in range(width - 1)}
                    pixels |= {(x, height - 2 - x) for x in range(width - 1)}
                else:
                    self.assertIn(cp, face, f"missing HP byte {code:#x}")
                    xo, yo, w, h, _, stride, *bits = face[cp]
                    pixels = {
                        (x + xo, y + yo)
                        for y in range(h)
                        for x in range(w)
                        if bits[y * stride + x // 8] & (128 >> (x % 8))
                    }
                    if xo < 0 or yo < 0 or xo + w > width or yo + h > height:
                        pixels = {
                            (
                                x * width // w if w > width else x,
                                y * height // h if h > height else y,
                            )
                            for x, y in ((x - xo, y - yo) for x, y in pixels)
                        }
                    if code == 129:
                        pixels |= {(x, 0) for x in range(width - 1)}
                expected.extend(
                    int((x, y) in pixels and x < width and y < height)
                    for y in range(8)
                    for x in range(8)
                )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source = path / "probe.c"
            source.write_text(
                DRIVER
                + "\n"
                + "\n".join(host_face(name, face) for name, face in zip(names, faces))
            )
            executable = path / "probe"
            command = [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I" + str(ROOT / "include"),
                "-I" + str(ROOT / "runtime/core"),
                str(source),
            ]
            command += [
                str(ROOT / "runtime/core" / f"{name}.c")
                for name in (
                    "hpg-context",
                    "hpg-draw",
                    "hpg-font",
                    "font",
                    "display-surface",
                )
            ]
            subprocess.run(
                command + ["-o", str(executable)], check=True, capture_output=True
            )
            actual = subprocess.check_output([str(executable)])
        self.assertEqual(actual, bytes(expected) * 6)
