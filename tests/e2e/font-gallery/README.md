# Font Gallery E2E

Font Gallery is the calculator-side visual regression gate for hpclang fonts.

- The **full** profile links every generated face in `runtime/fonts/generated` plus the two calculator-native faces and is exercised by Direct E2E on the hp39, hp48, and hp4950 families.
- The **stock-smoke** profile links one generated face plus the two native faces and validates the real calculator UI → ARMToolbox → HPC1 launch path on HP48gII and HP50g.
- HP48/49/50 native-font correctness is intentionally a Stock concern because the Direct synthetic launcher does not fabricate a live Saturn `Read[]` mapping. HP39/40 native fonts use the independently verified physical-ROM path and can therefore be validated in Direct.
- Catalog size is derived from the generated font index at configure time. Do not hard-code historical face or capture counts in the test implementation; source-package curation is expected to change them.

The human asset-review/audit path is separate: `tools/audit-font-assets.py` checks canonical-source ↔ generated semantic parity, while `tools/render-font-audit-cn.py` produces the wide visual review report used for font-family curation.
## Closure work still required

- remove the TTF importer and rasterizer-only dependencies/tests/docs;
- normalize Direct/Stock summary structure, include all concern-owned Stock evidence, and add the missing HP39 Direct summary;
- use repertoire/design-sensitive specimens instead of one weak generic phrase;
- evaluate Cubic-11, Fusion Pixel Font, ChillBitmap, and May7/QuanPixel as bitmap-source candidates.

### Current bitmap-only font closure status (2026-09-13)

Canonical bundled assets are bitmap-source only. The generated catalog is **242 faces / 1,597,680 mapped glyphs / 3,386 assembly sources** with **242/242** source-to-generated parity; `CI / Font Assets` run **34748326228 / #20** is green and regeneration produces no drift.

`E2E / Font Gallery` run **34748303030 / #49** is green. Direct traverses **244 entries (242 generated + 2 native) × 2 pages = 488 captures per target** on HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. HP48 Stock reaches HPC1 PASS with distinct nonblank native-mini/native-main/generated-control captures, while visible return remains `return-unsettled`.

Before merge: remove the TTF importer; normalize hp48/hp4950 Stock summaries and add the missing HP39 Direct summary; replace weak generic Gallery specimen text with repertoire/design-sensitive samples; evaluate Cubic-11, Fusion Pixel Font, ChillBitmap, and May7/QuanPixel; rerun Font Assets and Gallery after asset/rendering changes.
