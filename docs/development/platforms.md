# Platforms

hpclang supports six calculator model profiles through three platform families.

| Model | Model ID | Family | Direct runtime | Original-system launch |
| --- | ---: | --- | --- | --- |
| HP 39g+ | 1 | `hp39` | **Blocked** | **Blocked** |
| HP 39gs | 2 | `hp39` | PASS, including real hardware | PASS, including real hardware return |
| HP 40gs | 3 | `hp39` | PASS | PASS |
| HP 48gII | 4 | `hp48` | PASS | PASS |
| HP 49g+ | 5 | `hp4950` | PASS | PASS |
| HP 50g | 6 | `hp4950` | PASS | PASS |

## HP 39g+, 39gs, and 40gs

All three models use the same hp39 application packaging and ARMToolbox39 lineage. HP 39gs and HP 40gs have complete automated runtime and stock-launch coverage.

HP 39g+ support builds and the hardware-validation package is available, but the model has no trustworthy public stock ARM firmware fixture. A read-only dumper has been validated against emulated NOR and is retained under `research/hp39gplus/`. E2E remains blocked until a real HP 39g+ is available and its stock flash can be dumped and verified.

This blocker is a test-fixture problem, not a separate compiler or runtime implementation task.

## HP 48gII

HP 48gII uses the dedicated `hp48` launcher implementation. The emulator gate uses a verified 2 MiB full-NOR image, 256 KiB physical RAM, and a 131×64 display. Both Direct runtime and the original Serial Kermit → ARMToolbox48 → `PrRUN` path pass.

## HP 49g+ and HP 50g

HP 49g+ and HP 50g share the `hp4950` platform implementation, the same official 2.15 OS image, and the same stock File Manager/ARMToolbox test sequence. Their native ARM bootloaders remain model-specific fixture metadata: `boot-49g+.bin` and `boot-50g.bin`.

## Out of scope

Saturn-only HP 39G/40G, classic HP 48, and HP 49G are outside the native hpclang runtime scope.
