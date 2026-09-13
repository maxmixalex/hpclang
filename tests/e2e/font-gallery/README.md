# Font Gallery E2E

Font Gallery is the calculator-side visual regression gate for hpclang fonts.

- The **full** profile covers every generated face in `runtime/fonts/generated` plus the two calculator-native faces and is exercised by Direct E2E on the hp39, hp48, and hp4950 families.
- The **stock-smoke** profile links one generated face plus the two native faces and validates the real calculator UI → ARMToolbox → HPC1 launch path on HP48gII and HP50g.
- HP48/49/50 native-font correctness is intentionally a Stock concern because the Direct synthetic launcher does not fabricate a live Saturn `Read[]` mapping. HP39/40 native fonts use the independently verified physical-ROM path and can therefore be validated in Direct.
- Catalog size is derived from the generated font index at configure time. Do not hard-code historical face or capture counts in the test implementation; source-package curation is expected to change them.

## Exhaustive Direct sharding

The exhaustive Gallery is a visual-coverage workload, not one realistic application image. Keeping full printable ASCII plus the common Unicode/PUA probes across the complete catalog makes a monolithic Gallery much larger than the hpclang v1 launcher can map.

Two independent size limits are therefore enforced instead of weakening the specimen:

- the public HPC1 code image starts at `0x01220000` inside the `0x01200000..0x012fffff` coarse-table section, leaving **224 × 4 KiB = 917,504 bytes** for the initialized code image;
- the x50ng Direct Gallery fixture maps 224 synthetic injection pages and reserves three around the stage0 image, so a raw Direct launcher must not exceed **221 × 4 KiB = 905,216 bytes**.

`tools/hpclang-pack.py` rejects an HPC1 image that exceeds the first limit. `tools/build-e2e.sh` also rejects an individual Direct Gallery launcher that exceeds the second limit.

By default `tools/build-e2e.sh font-gallery` therefore builds **20 deterministic round-robin shards**. The shard count is configurable with `HPCLANG_GALLERY_SHARD_COUNT`; it is a packaging parameter, not a repertoire parameter. Every generated face still receives the same printable-ASCII/common-Unicode/private-use selection it would receive in an unsharded build. CMake assigns a sorted catalog entry to shard `index % shard_count`, so catalog growth does not require a hard-coded face count.

The build exports all shard-specific raw launchers through the existing `launchers/*.bin` artifact interface. `run-gallery-shards.sh` executes every shard for a target and continues collecting diagnostics after an individual shard failure. Native mini/main remain the first two Gallery entries in every shard so each launch validates the same runtime contract; aggregate evidence counts those duplicate native entries only once.

A formal Direct result is successful only when every expected shard completes with PASS, returns to the calculator, and settles. The aggregate JSON and merged Gallery image represent unique catalog entries across all shards.

## Asset audit versus calculator E2E

The human asset-review/audit path is separate. `tools/audit-font-assets.py` checks canonical-source ↔ generated semantic parity, while `tools/render-font-audit-cn.py` produces the wide visual review report used for font-family curation.

As of 2026-09-13, the canonical bundled package is bitmap-source only and contains **242 generated faces / 1,597,680 mapped glyphs / 3,386 assembly sources** with **242/242** source-to-generated parity. The current asset audit reports zero quick-overflow, glyph-collapse, blank-heavy, and Powerline-variant failures; remaining ASCII-gap and loose-spacing findings are human-review signals rather than generator failures.

The previous monolithic Direct Gallery regression demonstrated why sharding is required: its full linked text was about 11.95 MB and all Direct families failed before face 0, while the compact Stock payload and both Stock launch paths remained healthy. The next formal Gallery regression must therefore validate the sharded Direct path; no sharded run is claimed green here until that regression completes.

Before merge, retained bitmap families still require human visual review. In particular, locale/region variants and spacing outliers should be judged from the current wide review report rather than deleted from machine heuristics alone.
