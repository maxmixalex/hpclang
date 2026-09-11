# Font source package

`fonts.zip` is the canonical custom-font source bundle for hpclang. It is tracked with Git LFS and kept as the upstream/source archive rather than being repacked into a build-specific format.

The archive is **not** consumed by normal application builds. Python code under `tools/` is repository-maintenance tooling only: maintainers use it to inspect source fonts or regenerate checked-in assets. SDK users compiling C/C++ applications do not need Python, Pillow, fontTools, or anything from `tools/`.

The pipeline is:

1. `runtime/fonts/fonts.zip` — canonical raw `fonts/…` source tree stored with Git LFS.
2. `tools/hpclang-font.py` — source-format parsers plus direct raw-package inspection/explicit compilation tooling.
3. `tools/regenerate-font-assets.py` — indexes the raw bundle and batch-generates section-GC-friendly `.S` files plus the CMake catalog under `runtime/fonts/generated/`. Large Unicode faces are split into bounded assembly chunks while keeping one logical font descriptor.
4. normal C/C++ build — consumes only the checked-in generated assets through CMake/clang/lld; no source-font parser runs here.

The current generated catalog contains **308 faces / 1,779,042 glyph mappings / 3,797 assembly sources**.

Two font sources must not be conflated:

- `hpclang_font_default()`, `hpclang_font_mini()`, and `hpclang_font_big()` expose the calculator's active **native stock-ROM fonts** at runtime; they do not embed a gallery font in the application.
- `hpclang_target_font()` selects a **custom/generated font** from the checked-in gallery assets.

For custom fonts, the convenience aliases are fixed as:

- `@mini` → newRPL `Font6_mediumA` (`newrpl_6a`);
- `@normal` → newRPL `Font8_StyleC` (`newrpl_8c`);
- `@large` → newRPL `Font10_StyleA` (`newrpl_10a`).

These aliases are only defaults. Applications may select any generated catalog face by ID, and the bitmap-font ABI supports arbitrary metrics and proportional advances.

## Import modes

`hpclang_target_font()` exposes exactly three repertoire modes.

### `STATIC_AUTO`

Collect the static repertoire from string literals explicitly marked with `HPCLANG_UI_TEXT()` in the target's C/C++ sources. Ordinary unmarked literals are intentionally ignored, so diagnostics, paths, format strings and unrelated metadata cannot silently enlarge the font.

```cmake
hpclang_target_font(app
    NAME ui_font
    FONT @normal
    MODE STATIC_AUTO)
```

```c
const char *title = HPCLANG_UI_TEXT("Statistics");
```

`HPCLANG_UI_TEXT()` is an identity macro at runtime. Its argument must be one or more adjacent narrow UTF-8 string literals. Dynamic/generated/localized text whose complete repertoire cannot be seen this way should use `STATIC_EXPLICIT` or `FULL` instead.

### `STATIC_EXPLICIT`

Retain the exact codepoint set supplied by the application, plus the configured fallback when one is enabled. No source-string discovery occurs.

```cmake
hpclang_target_font(app
    NAME numeric_font
    FONT @normal
    MODE STATIC_EXPLICIT
    CODEPOINTS 32 43 45 46 48 49 50 51 52 53 54 55 56 57)
```

Codepoints may be written as integer values or `U+XXXX` values.

### `FULL`

Retain the selected face's complete mapped repertoire.

```cmake
hpclang_target_font(app
    NAME full_font
    FONT some_generated_face_id
    MODE FULL)
```

The static modes synthesize a descriptor that references only the selected glyphs. Every generated glyph and bitmap lives in its own ELF section, so normal `--gc-sections` removes the rest from the final HPC1 image. `FULL` binds the generated complete-face descriptor instead.

`tools/hpclang-font.py compile` is intentionally narrower: it supports `STATIC_EXPLICIT` and `FULL`. `STATIC_AUTO` belongs to `hpclang_target_font()` because it must inspect the actual CMake target's marked source literals.

## Repository maintenance

Verify or inspect the raw canonical package directly:

```bash
python3 tools/hpclang-font.py --package runtime/fonts/fonts.zip verify
python3 tools/hpclang-font.py --package runtime/fonts/fonts.zip catalog
python3 tools/hpclang-font.py --package runtime/fonts/fonts.zip inspect @normal
```

Regenerate the checked-in catalog only when the source bundle or source-format generator changes:

```bash
python3 tools/regenerate-font-assets.py \
  --package runtime/fonts/fonts.zip \
  --output-dir runtime/fonts/generated \
  --chunk-glyphs 512
```

The `Maintenance / Font assets` workflow checks out the Git LFS source bundle, regenerates the catalog, applies repository-size/diff checks, and commits changed generated assets on `m7-fonts-gallery`.

The checked-in generated catalog is derived build material; `fonts.zip` remains the single source bundle and license/provenance reference.
