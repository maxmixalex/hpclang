# Font Gallery E2E

Font Gallery is the calculator-side visual regression gate for hpclang fonts.

- The **full** profile covers every generated face in `runtime/fonts/generated` plus the concrete ROM fonts exposed by the active target and is exercised by Direct E2E on the hp39, hp48, and hp4950 families.
- The **stock-smoke** profile links one generated face plus the target's concrete ROM fonts and validates the real calculator UI → ARMToolbox → HPC1 launch path on HP39gs, HP40gs, HP48gII, and HP50g.
- HP48/49/50 native-font correctness is intentionally a Stock concern because the Direct synthetic launcher does not fabricate a live Saturn `Read[]` mapping. HP39/40 native fonts use the independently verified physical-ROM path and can therefore be validated in Direct.
- Catalog size is derived from the generated font index at configure time. Do not hard-code historical face or capture counts in the test implementation; source-package curation is expected to change them.

## Exhaustive Direct sharding

The exhaustive Gallery is a visual-coverage workload, not one realistic application image. Keeping full printable ASCII plus the common Unicode/PUA probes across the complete catalog makes a monolithic Gallery much larger than the hpclang v1 launcher can map.

Two independent size limits are therefore enforced instead of weakening the specimen:

- the public HPC1 code image starts at `0x01220000` inside the `0x01200000..0x012fffff` coarse-table section, leaving **224 × 4 KiB = 917,504 bytes** for the initialized code image;
- the x50ng Direct Gallery fixture maps 224 synthetic injection pages and reserves three around the stage0 image, so a raw Direct launcher must not exceed **221 × 4 KiB = 905,216 bytes**.

`tools/hpclang-pack.py` rejects an HPC1 image that exceeds the first limit. `tools/build-e2e.sh` also rejects an individual Direct Gallery launcher that exceeds the second limit.

By default `tools/build-e2e.sh font-gallery` builds **20 deterministic round-robin shards**. The shard count is configurable with `HPCLANG_GALLERY_SHARD_COUNT`; it is a packaging parameter, not a repertoire parameter. Every generated face still receives the same repertoire-sensitive specimen it would receive in an unsharded build. CMake assigns a sorted catalog entry to shard `index % shard_count`, so catalog growth does not require a hard-coded face count.

The build exports all shard-specific raw launchers through the existing `launchers/*.bin` artifact interface. `run-gallery-shards.sh` executes every shard for a target and continues collecting diagnostics after an individual shard failure. The target's concrete native prefix is repeated in every shard so each launch validates the same runtime contract: three entries (mini/main/EditLine) on HP39gs/40gs and two (minifont/bigfont) on HP48/49/50. Aggregate evidence counts that repeated prefix only once.

A formal Direct result is successful only when every expected shard completes with PASS, returns to the calculator, and settles. The aggregate JSON and merged Gallery image represent unique catalog entries across all shards. The sharded path has already been exercised successfully across the automated families; any canonical font-census change still requires a fresh formal Gallery regression.

## Current bitmap catalog and visual review

The canonical package is bitmap-source only. The current generated catalog contains **245 faces / 1,639,485 mapped glyphs / 3,472 assembly sources** with **245/245** source-to-generated semantic parity.

The final CJK curation adds three visually reviewed families without restoring outline-font support to the production pipeline:

- `cubic-11/Cubic11-otf2bdf-9pt.bdf` — a pinned one-time bitmap derivation of Cubic 11 v1.500;
- `chill-bitmap/ChillBitmap7x-otf2bdf-6pt.bdf` — a pinned one-time bitmap derivation of ChillBitmap 7px v2.500;
- `quan-pixel/QuanPixel-May7.bdf` — the native BDF from the reviewed May7 package, byte-identical to the pinned public copy used for reproducibility.

The source bundle carries license/provenance sidecars for these additions. Those files are metadata rather than font faces. Normal application builds and normal font regeneration still consume only BDF, FON strikes, and newRPL BMP+mapping bitmap assets; no TTF importer or outline rasterizer is part of the maintained font path.

`tools/audit-font-assets.py` checks canonical-source ↔ generated semantic parity. `tools/render-font-audit-cn.py` produces the wide human-review page. The current audit reports zero exact-duplicate sets, quick-overflow, glyph-collapse, blank-heavy, and Powerline-variant failures. ASCII-gap and loose-spacing counts remain review signals rather than automatic deletion rules.

The review page deliberately uses repertoire/design-sensitive specimens instead of one generic phrase: uppercase/lowercase Latin, digits and punctuation, ambiguous-width strings such as `Il1| / 0OQ / rn / AV / To`, diacritics, mathematical/symbol characters, pangrams, kana/CJK/Hangul where present, and common locale-sensitive Han characters. Ark Pixel and Fusion Pixel locale variants are therefore judged from real glyph differences rather than their family names alone.

Some faces intentionally make non-alphanumeric repertoires very visible. Spleen, for example, contains box-drawing and Braille codepoints; Gallery renders those real glyphs as repertoire probes. They are not TAB control characters and should not be treated as mojibake. Locale specimen rows are glyph bags rather than prose and should avoid accidental geopolitical or other semantic phrasing.

The Gallery native section is a census of the concrete ROM fonts hpclang exposes, not a semantic role test. HP39gs/40gs show mini, main/default, and the independently established fixed 6×10 EditLine font. HP48gII/49g+/50g show minifont and bigfont/default. HP39g+ remains excluded from the EditLine census until its flash layout is verified.

## Current Gallery census

With 245 generated faces, fully-green Gallery **#111** (`35295358487`) proves the family-dependent census: HP39gs/40gs traverse **248 entries / 496 captures**, while HP48gII/49g+/50g remain **247 / 494**. The HP39 Direct and Stock paths both verify the EditLine table fingerprint and 6×10 pixels against the official 2.22 NOR images.

The compact Stock Gallery remains concern-specific. HP50g supplies authoritative hp4950 live-Saturn native-font evidence. HP48gII supplies the independent 64-line hp48 live-native path. HP48 now requires the same strict settled-return result as the canonical Runtime gate: HPC1 PASS, `status=pass`, control-flow return, `return_settled=true`, and x50ng exit 0. Gallery #111 is fully green across all four Stock targets and retains the strict settled-return gate.

Before merge, the current 245-face census must have a green `CI / Font Assets` regeneration/audit and a green formal `E2E / Font Gallery` run. Broader canonical Runtime Stock-return/keyboard harness issues remain separate gates and must not be silently reclassified as font correctness.
