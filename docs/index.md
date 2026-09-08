# hpclang development

hpclang is an LLVM-native SDK for the ARM-based HP graphing calculators. The project keeps one application ABI and one common ARMv4T runtime, while calculator-specific launch and packaging details are isolated in three platform families.

This documentation is currently maintained for development work. It records the supported architecture, platform status, automated validation, roadmap, and the historical HPGCC facts that remain relevant to the implementation.

## Current status

Five calculator models have complete emulator E2E coverage:

| Model | Direct runtime | Original-system launch |
| --- | --- | --- |
| HP 39gs | PASS | PASS |
| HP 40gs | PASS | PASS |
| HP 48gII | PASS | PASS |
| HP 49g+ | PASS | PASS |
| HP 50g | PASS | PASS |
| HP 39g+ | **Blocked** | **Blocked** |

HP 39g+ support builds and the read-only flash-dump mechanism has been validated, but automated E2E requires a trustworthy dump from a real HP 39g+. Further HP 39g+ E2E work is intentionally paused until that hardware fixture is available.

## Development documents

- [Architecture](development/architecture.md) describes the common runtime and the three platform families.
- [Application ABI](development/abi.md) records the stable HPC1 execution format.
- [Platforms](development/platforms.md) records model differences and current support.
- [Validation](development/validation.md) explains what each CI and E2E gate proves.
- [Roadmap](development/roadmap.md) records remaining development milestones.
- [History](development/history.md) keeps the HPGCC lineage, provenance, and licensing rules that still matter.
