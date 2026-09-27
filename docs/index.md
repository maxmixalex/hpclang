# hpclang development

hpclang is an LLVM-native SDK for the ARM-based HP graphing calculators. The project keeps one application ABI and one common ARMv4T runtime, while calculator-specific launch, packaging, and hardware integration are isolated in three platform families.

This site is currently an internal Development reference. It intentionally preserves implementation decisions, validation details, fixture provenance, historical compatibility evidence, and completed milestones that are useful when continuing development.

## Current status

| Model | Platform | Direct runtime | Original-system launch |
| --- | --- | --- | --- |
| HP 39g+ | `hp39` | PASS, verified D1.55 fixture | PASS over USB |
| HP 39gs | `hp39` | PASS, including real hardware | PASS, including real hardware return |
| HP 40gs | `hp39` | PASS | PASS |
| HP 48gII | `hp48` | PASS | PASS |
| HP 49g+ | `hp4950` | PASS | PASS |
| HP 50g | `hp4950` | PASS | PASS |

HP 39g+ uses the verified real-machine D1.55 full-NOR fixture. All six
models now pass canonical original-Stock launch over USB as well as Direct
runtime. The separate Font Gallery and Storage regressions also pass; their
Stock paths intentionally retain UART/SD and SD respectively. See
[Stock USB transport](development/stock-usb-transport.md) for the implementation
and transport scope. Repeated-run performance benchmarking is separate from
functional completion.

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
- [HPGCC Saturn adapter map](development/hpgcc-saturn-adapter.md) — M7.3e source-compatibility boundary for high-level `satdir.h` / `saturn.h`, including implemented adapters and raw-address APIs intentionally kept Migration/Internal.
- [History](development/history.md) — HPGCC lineage and compatibility facts retained by hpclang.
- [Licensing](development/licensing.md) — historical source, firmware, and independent-reimplementation rules.
- [Retired experiments](development/experiments.md) — conclusions preserved from removed bring-up and compiler experiments.
