# hpclang roadmap

Current checkpoint: 2026-11-28. **M8.1 through M8.5 are accepted complete. M8.0 and M8.6 remain open.**

Accepted main is `8f225f5b` after [#78](https://github.com/maxmixalex/hpclang/pull/78).

- **M8.0 native ports.** Main includes Port 2 cross-bank append planning and sector journal staging
  ([#66](https://github.com/maxmixalex/hpclang/pull/66)), and Port 1 reference checks
  ([#70](https://github.com/maxmixalex/hpclang/pull/70), [#75](https://github.com/maxmixalex/hpclang/pull/75)).
  Guarded internal library removal and replacement are in CI ([#79](https://github.com/maxmixalex/hpclang/pull/79),
  [#81](https://github.com/maxmixalex/hpclang/pull/81)). Both passed remote Quality after the edit validation fix.
  Local checks cover 180 host tests and HP49g+/HP50g original-ROM edits. [#84](https://github.com/maxmixalex/hpclang/pull/84)
  plans a deleted-suffix sector at the installed recovery library's bound journal slot. Both original-ROM
  models verified the complete 4 KiB image and unchanged Flash. CI is pending. Public library management, execution
  roots, Port 2 recovery installation, authority, automatic startup and reclamation remain open.
- **M8.1 through M8.5** remain accepted. The local integration at `5d0b90b6` passed all 180 host tests,
  six original-ROM Notebook regressions and the relocated SDK checks.
- **M8.6 applications and release.** The HP39/40 editor frees 4 KiB
  ([#76](https://github.com/maxmixalex/hpclang/pull/76)). On HP48gII, reopening the editor with the
  caller stack preserved left 92,753 bytes of Stock workspace from HOME and 90,682 bytes from Port 0.
  Both pull requests passed all five workflows. HP49g+/HP50g measured 40,980 bytes of peak KOS memory
  and passed moving GC after editor exit. [Launcher measurements](architecture.md#notebook-first-install-transfer-size).
  HP39 transfer failures now appear in CI logs ([#78](https://github.com/maxmixalex/hpclang/pull/78)).
  Notebook entry after moving GC with mappings held ([#80](https://github.com/maxmixalex/hpclang/pull/80))
  awaits CI acceptance after an HP39gs Kermit transfer failure. The SDK export
  ([#82](https://github.com/maxmixalex/hpclang/pull/82)) is in CI. Its candidate on main `8f225f5b`
  reproduced 613 outputs and passed local Notebook save/reopen on all six original-ROM models. Resuming a running editor
  across GC and [final release acceptance](roadmap-history.md#m86-application-and-release-acceptance) remain open.

## Scope and retained requirements

Reach practical HPGCC application capability parity, then exceed it with one
LLVM-native SDK/ABI, owned services, modern C++, declarative C UI and cross-family
validation. M0–M7 provide the accepted foundation; they do not complete M8.

The previous roadmap and board snapshot are preserved in the
[frozen requirements/evidence archive](roadmap-history.md). **Requirements remain in force except for the user-approved deferral below.** This page shortens status reporting;
it does not remove requirements, reopen accepted M7 scope or replace full
acceptance with focused experiments.

## M0–M7 — Completed milestones

The completed stages remain part of the project roadmap. M0–M6 establish the
execution foundation; M7 establishes application-facing services within its
accepted scope. Detailed criteria, checklists and evidence remain in the linked
archive sections. M8 extends this foundation; it does not replace these stages.

### M0 — Unified application ABI

**Complete.** One HPC1 application contract: AAPCS32, Thumb-1/ARMv4T, soft float,
fixed virtual layout and stable model IDs 1–6.
[Criteria and evidence](roadmap-history.md#m0-unified-application-abi).

### M1 — Modern language/runtime stack

**Complete.** LLVM builtins, ARMv4T-safe arithmetic, picolibc, allocation, static
TLS, constructors and common C/C++ linkage, without legacy HPGCC runtime objects
as normal-build dependencies.
[Criteria and evidence](roadmap-history.md#m1-modern-languageruntime-stack).

### M2 — Common ARM bootstrap

**Complete for the validated platforms.** Install HPC1 mappings, enter the
application, restore temporary state and return to the calculator OS through
the common bootstrap. Includes HP39gs hardware and supported emulator gates.
[Criteria and evidence](roadmap-history.md#m2-common-arm-bootstrap).

### M3 — HP39 platform

**Direct runtime and original-Stock USB launch complete for HP39g+/39gs/40gs.**
Includes the HP39gs hardware runtime/return check, HP39g+ verified D1.55 ROM
fixture, model-specific packages and automated calculator-side launch/return.
[Criteria and evidence](roadmap-history.md#m3-hp39-platform).

### M4 — HP48 platform / HP 48gII

**Complete.** Model-4 hardware profile, genuine full-NOR fixture, 256 KiB RAM,
131×64 display and common HPC1 execution with responsive Stock return through
the ARMToolbox48 launch path.
[Criteria and evidence](roadmap-history.md#m4-hp48-platform-hp-48gii).

### M5 — hp4950 platform / HP 49g+ and HP 50g

**Complete.** Shared hp4950 platform and application ABI, model-specific boot
metadata, Direct execution and original-Stock File Manager/ARMToolbox launch
and return on both models.
[Criteria and evidence](roadmap-history.md#m5-hp4950-platform-hp-49g-and-hp-50g).

### M6 — Automated emulator E2E

**Direct runtime and original-Stock USB launch complete for all six models.**
Keep execution, control-flow return and settled Stock repaint as distinct gates.
UART/SD remain explicit secondary transports; optional repeated-run USB timing
and stability research remains recorded separately.
[Criteria and evidence](roadmap-history.md#m6-automated-emulator-e2e).

### M7 — Application-facing calculator services

**Complete within the individually approved scope.** Accepted functional head
`2988e1de`; [final acceptance](validation.md#m7-final-acceptance) records the gates.
The stage covers the API/compatibility inventory (M7.0), device/display/input/fonts
(M7.1), byte-stream files and stdio (M7.2), calculator objects/RPL/stack and native
catalog services (M7.3), common system services (M7.4), and display/runtime parity
(M7.5). Hardware and family capability boundaries remain explicit.

Native Port 0/1/2 implementation goals continue in M8. Whole-calculator reset
and reliable hardware serial-number discovery remain approved follow-ups;
M7 acceptance does not imply those capabilities are implemented.
[Criteria, substage checklists and evidence](roadmap-history.md#m7-application-facing-calculator-services).

## M8 — HPGCC source compatibility, SDK UX, and post-HPGCC extensions

| Stage | Current state | Remaining completion gate |
| --- | --- | --- |
| M8.0 Native Port 0/1/2 | Implemented in part; active mainline | Complete safe reference handling and movement, public library management and Port 1 edits; Port2 trusted recovery installation, authority lifecycle, safe startup and reclamation |
| M8.1 Installable SDK | **Accepted complete** | Preserve installed external C/C++ builds, relocatability, packaging and reproducibility |
| M8.2 Declarative C UI core | **Accepted complete** under the approved resource scope | Preserve declarative identity/layout/input/effects and supported-flow resource regressions |
| M8.3 Components/renderers | **Accepted complete** under the approved resource scope | Preserve six-model rendering, low-memory recovery, budgets and clean return |
| M8.4 HPGCC source adapters | **Accepted complete** | Preserve original-source adapters, explicit migrations, tone/RTC ownership and supported-model regressions |
| M8.5 Modern C++/migration | **Accepted complete** | Preserve ownership/error/lifetime, migration tooling, installed C/C++ examples and deterministic resources |
| M8.6 Applications/release | Production Notebook release checks accepted | Complete the independent-launcher evaluation across three families |

Detailed stage contracts are preserved under [M8 stages](roadmap-history.md#m8-stages).
UI must retain the user's declarative C design: nested ordinary C declarations,
stable keyed identity, controlled inputs, explicit asynchronous state and
revision-driven canvas. A superficially similar imperative API is not completion.

## Approved resource scope — 2026-11-14

The user approved deferring comprehensive Stock exception-stack capacity and
whole-system worst-case transient-memory research. These are follow-up research,
not blockers for M8.2/M8.3 or application/release acceptance. Acceptance still
requires budgets and isolation for supported hpclang launch/run/return flows,
allocation-failure handling, low-memory recovery and correction of observed
faults. Measurements remain bounded-workload evidence, not universal bounds.
Avoid expanding marginal cases while ordinary supported flows need work.
This decision does not defer Port2 recovery/reclamation or change other M8 goals.
The subsequent acceptance audit confirms M8.2/M8.3 complete on main 2a65079f:
29 UI source/header/example/test paths match accepted 236e792b; all six Direct
and six Stock UI cases pass in Runtime 43130132986. Identity/layout/input/effects,
renderer/theme/font/canvas behavior, 36 KiB RW admission, allocation pressure,
reconstruction, stack/heap and update budgets, and settled return are covered.
This uses existing accepted execution plus source-identity reconciliation; no
new hardware or universal-memory-bound claim. #43 follow-ups are accepted on main 38e9472a.

## Current work: balanced M8 delivery

Advance native ports and release qualification in parallel. Keep the installed
SDK, declarative UI, HPGCC and C++ regression suites running. Port 2 automatic
recovery and space reclamation stay within M8. At the current checkpoint the
unexplained HP39gs/HP40gs USB stalls and HP49g+ PWM IRQ failure remain historical
observations. Their original causes are unknown.

The [development board](https://github.com/maxmixalex/hpclang/issues/1) tracks
open deliveries. Detailed per-frame measurements, earlier CI failures and
historical release checks remain in the
[November checkpoint](https://github.com/maxmixalex/hpclang/blob/119a779dd712d5f9937d4204cf7a837206a8e17e/docs/development/roadmap.md#current-work-balanced-m8-delivery)
and [M8 validation history](m8-validation-history.md).

## Delivery and evidence policy

Before each push, inspect the preceding push's exact-head CI and investigate
failures. Do not poll newly pushed CI in the same turn or merge incomplete gates.
Private saved-state/ROM interpreter checks supplement full Stock tests; they do
not establish hardware behavior or complete milestone acceptance.

Keep M0–M7 summaries and their acceptance links on this page. Keep this page concise. Put reproducible checks in tests and link their reports
from the relevant PR. Preserve requirements and concise accepted conclusions in the
frozen archive; discard superseded reproducible build/test artifacts instead of
archiving them. Do not append per-turn experiment narration to the roadmap or board. Record only
current decisions, meaningful state changes, blockers and next acceptance gates.

## Deferred research — HP39gs / HP40gs ROM storage

Retain investigation of ROM-write capability inspired by
[HP39GS-Explorer](https://github.com/Yanye0xFF/HP39GS-Explorer), with user-provided
ROM dumps as research input. HP39g+ is excluded by its hardware limitation.
This is a deferred idea, not authorization to flash hardware now.

## Preserved approval boundaries

Port2 compromises may affect sequencing, but automatic recovery/reclamation stay
inside M8. The chosen HPGCC numeric-format behavior is explicit opt-in through
`HPCLANG_HPGCC_LEGACY_FORMAT=1`; standard C behavior remains the default.
Any actual scope reduction requires the user's agreement. No repeated approval
is needed for an already authorized implementation or a proven status update.
