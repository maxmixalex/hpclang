# hpclang development

hpclang is an LLVM-native SDK for the ARM-based HP graphing calculators. The project keeps one application ABI and one common ARMv4T runtime, while calculator-specific launch, packaging, and hardware integration are isolated in three platform families.

This site is currently an internal Development reference. It intentionally preserves implementation decisions, validation details, fixture provenance, historical compatibility evidence, and completed milestones that are useful when continuing development.

## Current status

| Model | Platform | Direct runtime | Original-system launch |
| --- | --- | --- | --- |
| HP 39g+ | `hp39` | **Blocked** | **Blocked** |
| HP 39gs | `hp39` | PASS, including real hardware | PASS, including real hardware return |
| HP 40gs | `hp39` | PASS | PASS |
| HP 48gII | `hp48` | PASS | PASS |
| HP 49g+ | `hp4950` | PASS | PASS |
| HP 50g | `hp4950` | PASS | PASS |

HP 39g+ support builds and the read-only flash-dump mechanism has been verified, but automated E2E requires a trustworthy stock flash image from a real HP 39g+. Further HP 39g+ E2E work is intentionally paused until that hardware is available.

## Development reference

- [Architecture](development/architecture.md) — supported build model, runtime responsibilities, platform responsibilities, and rejected legacy execution models.
- [Application ABI](development/abi.md) — the stable HPC1 execution format and boot record.
- [Platforms](development/platforms.md) — model identities, family grouping, and model-specific differences.
- [Validation](development/validation.md) — what the continuous gates prove and what constitutes PASS.
- [API reference and parity](development/api-reference.md) — current APIs, validation state, and version-by-version HPGCC progress.
- [E2E internals](development/e2e.md) — target profiles, sequence files, x50ng integration, and stock transfer mechanisms.
- [Fixture provenance](development/fixtures.md) — where firmware/full-NOR inputs come from and how they are verified.
- [Roadmap](development/roadmap.md) — M0–M8 milestones and remaining work.
- [Calculator object service](development/object-service.md) — M7.3 owned-object, Saturn/RPL stack, and named-store ownership contract.
- [History](development/history.md) — HPGCC lineage and compatibility facts retained by hpclang.
- [Licensing](development/licensing.md) — historical source, firmware, and independent-reimplementation rules.
- [Retired experiments](development/experiments.md) — conclusions preserved from removed bring-up and compiler experiments.
