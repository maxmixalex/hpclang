# hpclang roadmap

Current checkpoint: 2026-11-14. **M8 is incomplete; M8.1–M8.5 are accepted complete.**
Accepted main: `38e9472a` (#43), integrating compatibility, C++/migration, SDK
distribution, applications and UI observation fixes. Exact head `712e60c6` passed
all seven workflows: 201 successful jobs and three configured skips. Remote main
and merged PR were independently verified. Delivery acceptance does not by itself
complete the remaining M8 milestones.
The [development board](https://github.com/maxmixalex/hpclang/issues/1) tracks
current work. Earlier accepted deliveries remain in the validation history.

Container housekeeping (2026-11-14): removed 109 obsolete CMake intermediate directories, 80 superseded host-test build directories and 104 untracked emulator object files, reclaiming 855.7 MiB in total from inside the container. The additional 1.6 MiB object-file cleanup retains sibling C sources, shared emulator binaries and directory aliases; report: /work/nov14-old-emulator-intermediates.json. Current 180-test ASan/UBSan validation, source trees and concise reports remain. No live-process or external-symlink dependencies were found; no archive was created. Old binaries require rebuilding. Reports: /work/nov14-cmake-cleanup-result.json, /work/nov14-old-host-cleanup-result.json and /work/nov14-old-oct-host-cleanup-result.json. Retain shared dependencies /work/oct7-next-probe/cpu/dist/runtime-lib and /work/oct25-watchdog-src/.cache/hp39-aplet. Earlier Windows download deletion remains blocked by execution policy.

Container cleanup (2026-11-13): removed /work/oct30-stage-stack-matrix/runner/build from inside the container after confirming its completed audit, current six-model replacement coverage and absence of active process/external symlink dependencies. Reclaimed 48,915,552 bytes without an archive; source scripts, concise reports, original inputs and shared emulators remain.

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
| M8.6 Applications/release | Stage open | Nontrivial integrated apps, clean SDK use, cross-family resource/return checks, documentation and release acceptance |

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

Rotate bounded delivery batches through M8.0, M8.2/M8.3, M8.4, M8.5 and
M8.6; retain M8.1 regressions. Finish or record a blocker before switching,
without expanding edge-case investigations. Other stages do not wait for
complete Port2 qualification. Apply the approved resource deferral below; other acceptance conditions remain
unchanged. Sound compatibility is independent of Port2.

CI (2026-11-14): #43 / 712e60c6 passed all seven workflows, 201 jobs and three configured skips, and merged as 38e9472a. All six Stock UI and Notebook jobs and both code-relocation jobs pass. Previous-head UI report-schema failures are resolved. Main ref and PR merge verified; report: research/nov14-final-integration-merged.json. No post-merge CI is claimed. M8.4/M8.5 acceptance records, launcher sizing and the independent KOS allocation probe are submitted as draft #46 / 437d45f7. Quality CI fails only Python formatting at the inspected head; repository-configured formatting repair passes locally. Documentation CI passes; remaining workflows are still running.

| Stage / delivery | Current evidence | Next gate |
| --- | --- | --- |
| M8.0 / #31 | Internal Port0 registration and Port1 owned-tail edits pass local host/native gates. Accepted and merged as 2a65079f: exact head c662b236 passes all nine workflows, 219 jobs and three skips. This accepts the bounded implementation, not all M8.0 requirements. | Production reference collection, public library management and Port1 edits remain open. Test-owned-tail guards do not authorize existing-object edits; execution collection in call-roots.c also depends on fixture-specific ROM addresses and trace counts, so it cannot be promoted directly. This investigation is bounded here while other stages advance. Earlier HP39gs transfer failure passes exact replay, original cause unresolved. Evidence: /work/nov14-port1-owned-edit-native-v2/independent-audit.json. |
| M8.0 / Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support is accepted in #25. | Trusted installation, authority lifecycle, startup and reclamation remain. Mutable-Flash configuration execution is unqualified; forty data targets retain separate execution obligations. |
| M8.2/M8.3 / UI | Core/component milestones remain accepted complete. #43 merged the saved-user-SP observer, low-SRAM pool exclusion and report-schema repair; all six Stock UI cases pass at 712e60c6. | Preserve supported-flow budgets, isolation and failure handling. Comprehensive Stock stack capacities and whole-system worst-case memory proof remain deferred. #44 delivery is included in #43. |
| M8.4 / compatibility #41 | **M8.4 accepted complete** on main 38e9472a: all five original requirements reconcile with the maintained adapter ledger, recorded original-source edits, KOS/RTC behavior and window/migration boundaries. #43 passes Storage and configured HPGCC/normal/exit/immediate-exit CI gates. | Retain source, signature and native regressions. Physical SD remains an approved nonblocking follow-up; six-model packaging does not imply SD availability. Final acceptance: docs/development/hpgcc-compat.md and docs/development/m8-validation-history.md. |
| M8.5 / ownership #14 | **M8.5 accepted complete** on main 38e9472a: the original three requirements are reconciled against implementation, host lifecycle/destructor tests, native exit checks and accepted installed-SDK CI. | Retain ownership/error/lifetime, no-exceptions/no-RTTI, C/C++ example and deterministic-resource regressions. Requirement map: docs/development/m8-validation-history.md (M8.5 final acceptance); local reconciliation: research/nov14-m85-final-acceptance.json. |
| M8.5 / migration #27 | Accepted within M8.5: installed migration guidance and read-only HP001–HP015 checker cover historical APIs, precision/ownership, target capabilities, text/fonts and raw hardware retirement. | Retain installed/relocated CLI, unchanged-input, encoding and incomplete-scan regressions. Evidence: /work/nov14-migration-window-lifetime.json and accepted Core run 43589087998. |
| M8.6 / applications #14 | #43 merged; all six Stock Notebook cases and configured backup/export/entry gates pass at 712e60c6. File-open buffer reduction and Port2 README correction are included. Local backup checks cover nine launches across three classic models. | Final release audit and independent-launcher evaluation remain. Earlier export stack is 6656/8192 bytes, relaunch 3712/8192; these are bounded-workload measurements. Evidence: /work/nov14-open-stack-native/independent-audit.json and /work/nov14-final-notebook-backup-native/independent-audit.json. |
| M8.6 / reproducibility #40 | Local KOS pool probe passes HP50g Stock. Follow-up now borrows/restores 28 main-RPL pages (112 KiB) with Stock/interrupts paused; saved-map, return and ten corruption checks pass. Default entry package remains unchanged. | Qualify combined ownership/fences and actual Notebook occupancy before claiming native-service coexistence. HP48/HP39 and release evaluation remain open. #46 / 437d45f7 has a Python-format CI failure, fixed locally using repository configuration; other CI remains pending. RPL follow-up not pushed. Report: /work/nov14-independent-rpl-pool-live/verified.json. |
| Shared Stock harness / #39 | 1db47ca3 fixes premature host readiness timeout while retaining the outer emulator timeout; four tests, old-behavior negative, Bash syntax and Ruff pass. The same fix is included in #14. | Accepted and merged as 7d1a5bb9 on 2026-11-14: exact-head 1db47ca3 passes all ten workflows, with 376 successful jobs and two skips. Preceding #14 failures remain recorded below. |

Current owner audit reports are
/work/nov06-sum/ui-owner-native-runs/independent-audit.json and
/work/nov06-sum/ui-owner-stock/independent-audit.json.
Completed timer validation includes six-model Direct/Stock integrated and
installed-example runs with independent artifact/pixel/return checks; its
branch acceptance passed in #14; integration into main is accepted through #43. Emulator results do not claim physical
hardware acceptance. Detailed historical results belong in the validation
history and PRs, not repeated per-turn narratives here.

## Pending delivery and unresolved failures

- #43 is accepted and merged for compatibility, ownership, migration, SDK
  distribution and applications. #40/#41/#44/#45 are included deliveries;
  acceptance comes from #43, not cancelled duplicate runs.
- #14 passed its allocation-branch CI (396 successful jobs, three skips);
  its main integration is now accepted through #43. This does not close all M8 stages.
- Earlier HP39gs/HP40gs USB transfer failures and HP49g+ system-services PWM IRQ
  failure remain historical unresolved observations. Passing exact replay or
  a later head does not establish the original cause.

Detailed accepted commits, earlier failures and validation scope remain in
[M8 validation history](m8-validation-history.md). All original completion
conditions remain in the [requirements archive](roadmap-history.md#m8-stages).

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
