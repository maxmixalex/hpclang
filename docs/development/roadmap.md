# hpclang roadmap

Current checkpoint: 2026-11-14. **M8 is incomplete; M8.1, M8.2 and M8.3 are accepted complete.**
Accepted main: `2a65079f` (#31). The combined compatibility, C++ and SDK delivery
is submitted as #43 / `00fb45ac`; local integration `58e21c5e` also includes
accepted main and the UI low-SRAM exclusion. Local validation, submitted CI,
accepted delivery and milestone completion remain distinct.
The [development board](https://github.com/maxmixalex/hpclang/issues/1) tracks
current work. Earlier accepted deliveries remain in the validation history.

Container housekeeping (2026-11-14): removed 109 obsolete CMake intermediate directories and 80 superseded host-test build directories, reclaiming 854.1 MiB in total from inside the container. Current 180-test ASan/UBSan validation, source trees and concise reports remain. No live-process or external-symlink dependencies were found; no archive was created. Old binaries require rebuilding. Reports: /work/nov14-cmake-cleanup-result.json, /work/nov14-old-host-cleanup-result.json and /work/nov14-old-oct-host-cleanup-result.json. Retain shared dependencies /work/oct7-next-probe/cpu/dist/runtime-lib and /work/oct25-watchdog-src/.cache/hp39-aplet. Earlier Windows download deletion remains blocked by execution policy.

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
| M8.4 HPGCC source adapters | SD Reset #15, opt-in formatting #17 and async/KOS tone gates #25 accepted; stage open | Complete retained historical application/API coverage, documented migrations and pending branch acceptance |
| M8.5 Modern C++/migration | Implemented in part; stage open | Complete ownership/error/lifetime contracts, migration tooling/guidance and public downstream examples |
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
new hardware or universal-memory-bound claim. #43 follow-ups remain pending.

## Current work: balanced M8 delivery

Rotate bounded delivery batches through M8.0, M8.2/M8.3, M8.4, M8.5 and
M8.6; retain M8.1 regressions. Finish or record a blocker before switching,
without expanding edge-case investigations. Other stages do not wait for
complete Port2 qualification. Apply the approved resource deferral below; other acceptance conditions remain
unchanged. Sound compatibility is independent of Port2.

CI (2026-11-14): #43 / 00fb45ac failed Core job 132401987116 (run 43579804552): isolated HPC1 input manifest omitted image-capabilities.h after ABI integration. Failure reproduced locally; adding the header makes HP49g+/HP50g two-copy, seven-output reproducibility checks pass. Fix is local, not yet submitted. Other gates remain pending. Current main is 2a65079f; this failure does not revoke already accepted UI core/components.

| Stage / delivery | Current evidence | Next gate |
| --- | --- | --- |
| M8.0 / #31 | Internal Port0 registration and Port1 owned-tail edits pass local host/native gates. Accepted and merged as 2a65079f: exact head c662b236 passes all nine workflows, 219 jobs and three skips. This accepts the bounded implementation, not all M8.0 requirements. | Production reference collection, public library management and Port1 edits remain open. Test-owned-tail guards do not authorize existing-object edits; execution collection in call-roots.c also depends on fixture-specific ROM addresses and trace counts, so it cannot be promoted directly. This investigation is bounded here while other stages advance. Earlier HP39gs transfer failure passes exact replay, original cause unresolved. Evidence: /work/nov14-port1-owned-edit-native-v2/independent-audit.json. |
| M8.0 / Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support is accepted in #25. | Trusted installation, authority lifecycle, startup and reclamation remain. Mutable-Flash configuration execution is unqualified; forty data targets retain separate execution obligations. |
| M8.2/M8.3 / UI | #42 accepted on main; corrected six-model saved-user-SP observation and boot decoder are submitted in combined #43 / 00fb45ac. Local follow-up excludes the native vector/runtime-stack page from the Stock UI pool and allocation block. Eleven tests, old-behavior negative and six-model retained-report replay pass. | Core/component milestones are accepted on main under the approved scope. The additional observer/exclusion follow-up is submitted in 00fb45ac and awaits CI; #44 delivery acceptance is routed through #43. Comprehensive Stock stack capacities and whole-system worst-case memory research are deferred by user decision; supported-flow budgets, isolation and failure handling remain required; replay is not new native execution. Evidence: /work/nov14-ui-native-page-replay.json and /work/nov14-user-stack-remaining/independent-audit.json. |
| M8.4 / compatibility #41 | Adapters and original signatures are included in combined #43 / 00fb45ac. Fresh relocated-SDK checks pass all 76 original/repaired window exports in C/C++ and both original filesystem programs with six-model launcher envelopes; exact migration diffs retained. Accepted HPG/tone/RTC/window commits remain main ancestors. | New-head CI has the reproduced isolated-build failure above; local fix awaits submission. Final stage/main acceptance remains open; packaging does not prove SD availability or native behavior. Physical SD remains an approved nonblocking follow-up. Evidence: /work/nov14-combined-compat-source and /work/nov14-fsystem-native-provenance.json. |
| M8.5 / ownership #14 | #14 is accepted on its allocation base; main integration #43 / 00fb45ac is in CI with the isolated-build failure above. The original three M8.5 requirements have local implementation/evidence. Public C++ wrappers/examples match accepted 35ee317c. Local integration 58e21c5e includes accepted main 2a65079f and the UI exclusion follow-up; executable sources are unchanged from validated 81412db5. 295 Python tests and strict docs pass. | All 180 host tests pass under ASan/UBSan; temporary build removed. Report: /work/nov14-final-integration-host/result.json. Full relocated SDK now passes: reproducible 4,121-file archives, nine headers, fourteen documentation links, migration and assembly checks pass; 613 byte-identical release outputs and all six model-specific Notebook builds now pass; HP39g+/HP39gs/HP40gs C/C++ Aplet packaging also passes. Sealed-input check passes and temporary SDK builds are removed; report: /work/nov14-final-integration-sdk.json. Submitted as #43 / 00fb45ac; new-head CI has the reproduced isolated-build failure above; local fix awaits submission. Full native/resource acceptance remains open. Logs: /work/nov14-final-integration-host/commands.log and /work/nov14-final-integration-sdk.log. Earlier evidence: research/nov14-cpp-delivery-reconciliation.json and /work/nov14-sdk-compat-owners-integration.json. |
| M8.5 / migration #27 | Directory pruning/incomplete-scan reporting accepted on main via #27. HP013 raw-address guidance and HP014/HP015 borrowed window-string hints are submitted together in #43 / 00fb45ac. | Twenty-one migration tests and actual installed/relocated read-only CLI pass; all 295 Python regressions, Ruff and strict docs pass after integration. New-head CI has the reproduced isolated-build failure above; local fix awaits submission; main acceptance remains open. Evidence: /work/nov14-migration-window-lifetime.json. |
| M8.6 / applications #14 | Notebook retains six-model persistence/backup gates. Submitted #43 / 00fb45ac includes c478171a, which removes an unused file-open result buffer: ARMv4T -Os frame falls from 344 to 56 bytes; ASan/UBSan storage metadata and read-only create/exclusive checks pass. Fresh HP49g+/HP50g export/relaunch passes all four launches with independent source/artifact, exact raw-card, pixel and settled-return checks. | Remote ref and updated PR body verified; new-head CI has the reproduced isolated-build failure above; local fix awaits submission. Main integration remains open. Local 81412db5 Stock Notebook backup passes all nine phases across HP48gII, HP49g+ and HP50g; independent source/artifact/report audit passes. A driver-only missing-verifier error was corrected without restarting HP48gII. These are functional checks, not new pixel/resource measurements; evidence: /work/nov14-final-notebook-backup-native/independent-audit.json. Export stack remains 6656/8192 bytes; relaunch falls from 3944 to 3712/8192 on both models. Heap remains 12412/245504. Strict docs and formatting pass. Full release/resource acceptance remains open. Evidence: /work/nov14-open-stack-native/independent-audit.json and /work/nov14-open-stack-check/result.json. |
| M8.6 / reproducibility #40 | Independent HPC1 prototype/family evaluation and SDK distribution are now included in combined #43 / 00fb45ac, together with the installed-documentation fix. The combined SDK passes 4,117 archived files, nine C++ headers, fourteen local links, 613 identical outputs, six-model Notebook and three HP39 Aplet families. | New-head #43 CI has the reproduced isolated-build failure above; local fix awaits submission. Original #40 acceptance is now routed through #43; duplicate runs are cancelled; main integration and remaining release/resource acceptance stay open. Evidence: /work/nov14-sdk-compat-owners-integration.json and research/nov14-combined-sdk-push.json. |
| Shared Stock harness / #39 | 1db47ca3 fixes premature host readiness timeout while retaining the outer emulator timeout; four tests, old-behavior negative, Bash syntax and Ruff pass. The same fix is included in #14. | Accepted and merged as 7d1a5bb9 on 2026-11-14: exact-head 1db47ca3 passes all ten workflows, with 376 successful jobs and two skips. Preceding #14 failures remain recorded below. |

Current owner audit reports are
/work/nov06-sum/ui-owner-native-runs/independent-audit.json and
/work/nov06-sum/ui-owner-stock/independent-audit.json.
Completed timer validation includes six-model Direct/Stock integrated and
installed-example runs with independent artifact/pixel/return checks; its
branch acceptance passed in #14; integration into main remains pending. Emulator results do not claim physical
hardware acceptance. Detailed historical results belong in the validation
history and PRs, not repeated per-turn narratives here.

## Pending delivery and unresolved failures

- #43 is the pending main integration for compatibility, ownership, migration,
  SDK distribution and applications. #40/#41/#44/#45 are included deliveries;
  cancelled duplicate CI runs do not count as acceptance.
- #14 passed its allocation-branch CI (396 successful jobs, three skips);
  its main integration is included in #43. This does not close all M8 stages.
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
