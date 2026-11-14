# hpclang roadmap

Current checkpoint: 2026-11-13. **M8 is incomplete; only M8.1 is accepted complete.**
The development container exited 255 and was restarted after terminal-state confirmation. The resumed Notebook matrix and independent six-model audit now pass; HP39g+ recovery provenance is retained. Earlier ID1701 two-model reservation retests pass guarded
recovery, repeat restart, absent recovery and 16 rejection cases per model.
Supported ID769 reservation matrices now pass on both models; full startup
safety and M8.0 remain incomplete.
Accepted main: `2c2477f5` (#42 Stock resource observers accepted; #39 readiness and #27 migration tooling retained); #14 is CI accepted into `m8-allocation-delivery`, with main integration pending; previous checkpoint `c394fe7c` (#38 original-application/tone acceptance correction accepted; #37 UI acceptance documentation accepted; #36 automatic documentation PR CI and #35 compatibility docs accepted; #34 Windows-checkout lifecycle tests accepted; #32 historical formatted-input adapters, #33 UI lifecycle and borrowed-stack validation, #13 native allocation budgets and Stock release, original HPGCC filesystem gates #19, native execution-origin foundations #26, UI resource observers #28, CI queue/manual dispatch #30, Kermit resend pacing #29, resource snapshots #20, guarded dispatch/origin foundations #25, native catalog/HOME and Port0 fixes #24, physical spans #23, return roots and M0–M7 overview #22, SD Reset #15 and opt-in formatting #17 accepted). Local passing tests, submitted CI, CI acceptance and
milestone completion are separate states. The [development board](https://github.com/maxmixalex/hpclang/issues/1)
tracks current tasks; its compact body now matches independent readback.

Container housekeeping: retain active dependencies, unresolved-failure reproductions and irreplaceable inputs; remove reproducible outputs without archives. Latest cleanup (2026-11-14) removed 66,569,615 logical bytes from the successful USB replay inside the container after independent FIFO/input checks and process/file-use checks. Brief results and necessary diagnostics remain in /work/nov14-pr31-usb-replay; temporary sanitizer builds are removed automatically. The older 32.94 GiB volume measurement is dated 2026-11-12, not a current usage claim. This turn's Windows CI downloads remain because their deletion was rejected by execution policy.

Container cleanup (2026-11-13): removed /work/oct30-stage-stack-matrix/runner/build from inside the container after confirming its completed audit, current six-model replacement coverage and absence of active process/external symlink dependencies. Reclaimed 48,915,552 bytes without an archive; source scripts, concise reports, original inputs and shared emulators remain.

## Scope and retained requirements

Reach practical HPGCC application capability parity, then exceed it with one
LLVM-native SDK/ABI, owned services, modern C++, declarative C UI and cross-family
validation. M0–M7 provide the accepted foundation; they do not complete M8.

The previous roadmap and board snapshot are preserved in the
[frozen requirements/evidence archive](roadmap-history.md). **Every original
completion condition remains in force.** This page shortens status reporting;
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
| M8.2 Declarative C UI core | Design and bounded-workload resource/lifecycle gates accepted; stage open | Native exception-stack reservations and whole-system transient memory admission |
| M8.3 Components/renderers | Six-model rendering, low-memory, application budgets and clean return accepted; stage open | Retained whole-application resource analysis, including native stacks and transient memory |
| M8.4 HPGCC source adapters | SD Reset #15, opt-in formatting #17 and async/KOS tone gates #25 accepted; stage open | Complete retained historical application/API coverage, documented migrations and pending branch acceptance |
| M8.5 Modern C++/migration | Implemented in part; stage open | Complete ownership/error/lifetime contracts, migration tooling/guidance and public downstream examples |
| M8.6 Applications/release | Stage open | Nontrivial integrated apps, clean SDK use, cross-family resource/return checks, documentation and release acceptance |

Detailed stage contracts are preserved under [M8 stages](roadmap-history.md#m8-stages).
UI must retain the user's declarative C design: nested ordinary C declarations,
stable keyed identity, controlled inputs, explicit asynchronous state and
revision-driven canvas. A superficially similar imperative API is not completion.

## Current work: balanced M8 delivery

Rotate bounded delivery batches through M8.0, M8.2/M8.3, M8.4, M8.5 and
M8.6; retain M8.1 regressions. Finish or record a blocker before switching,
without expanding edge-case investigations. Other stages do not wait for
complete Port2 qualification. Scope and original acceptance conditions remain
unchanged; sound compatibility is independent of Port2.

CI queue maintenance (2026-11-14): seven superseded ancestor-commit runs were cancelled and independently confirmed terminal; no current-head, main or manual run was cancelled. Twenty older records still report queued while both normal cancellation and a single force-cancel probe return HTTP 409 (not queued yet). They remain unresolved; do not treat them as cleared or current delivery failures. Report: research/nov14-ci-queue-cancel.json.

| Stage / delivery | Current evidence | Next gate |
| --- | --- | --- |
| M8.0 / #31 | Bounded repairs 2bd7a499 retain accepted CI and two-model Stock/saved-execution evidence. Internal Port 0 registration is integrated, with unchanged executed ARM artifacts. Latest remote 819b1af5 has ten successful workflows/one failure: 372 successful jobs, three skips and an HP39gs pre-entry console transfer failure. Host wire CRC/length and identical retry checks pass. | A single exact-CI replay now passes application/settled return with unchanged inputs; all 2,533 OUT packets are drained with no discarded bytes. This does not establish the intermittent failure cause or accept the failed CI. Local 8f4c8ca9 integrates main 2c2477f5 with documentation-only conflicts; five sanitizer targets, five Python suites and strict docs pass. Guest runtime/build inputs are unchanged from 819b1af5. Submitted as bde42037 after inspecting the preceding failed CI; remote branch and PR description independently verified. New CI is not polled this turn and remains unaccepted. Public registration still needs a complete production collector; public attachment, Port1 edits and retained Port2 requirements remain open. Evidence: /work/nov14-port-main-check/result.json, /work/nov14-pr31-usb-replay/replay.json and usb-out-accounting.json. |
| M8.0 / Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support is accepted in #25. | Trusted installation, authority lifecycle, startup and reclamation remain. Mutable-Flash configuration execution is unqualified; forty data targets retain separate execution obligations. |
| M8.2/M8.3 / UI | #42 / c371d4f8 passes all eight workflows: 331 successful jobs and three skips. Merged into main as 2c2477f5, independently verified against the remote main ref. Six-model Stock heap observations and five-bank stack-initializer checks are now integrated, including CI capture/verifiers and negative input tests. | Native exception-stack reservation capacities and whole-system transient-memory admission remain open. One-allocator observations and initialized stack tops do not prove these requirements. Historical HP39g+ pre-entry USB failure remains an unresolved cause. Evidence: /work/nov06-sum/native-heap-matrix/independent-audit.json, /work/nov14-native-stack/result.json and /work/nov14-stack-ci/result.json. |
| M8.4 / compatibility #41 | Lifecycle, handle, mode and case adapters retain original-reference, host and six-Direct/two-Stock gates. Main integration 979591b7 passes local sanitizer/Python, strict docs and full relocated SDK checks. Current CI: four successful/seven queued/one pending workflows; 95 successful jobs, one skip, 274 queued, ten running and one pending. | Current head is not CI accepted. The compatibility inventory now maps the five original M8.4 criteria to evidence, confirms HPG/tone/RTC/window accepted commits are on main, and corrects stale window/reinitialization status. Strict docs pass. This local reconciliation does not mark M8.4 complete; #41 CI and final review remain pending. Physical SD exercise retains its approved nonblocking status. Evidence: /work/nov14-fsystem-main-check/result.json, /work/nov14-fsystem-main-sdk.json and /work/nov14-fsystem-case-native-v2/independent-audit.json. |
| M8.5 / ownership #14 | #14 was accepted into its allocation base. Main-integration #43 / 3e6023e2 now passes thirteen workflows, 403 successful jobs and three skips. Local 5ef2cbdf integrates accepted main 2c2477f5, preserving both branches' validation records. Public owner/lifetime guidance, HP013 migration and Notebook export improvements are included. | Current integration passes seventeen ASan/UBSan targets, nineteen native-observer and twenty migration Python tests, strict docs and the full installed/relocated SDK check with six-model Notebook builds and three HP39 Aplet packages. Temporary builds removed. Submitted as ad9ffada in #43 after verifying preceding 3e6023e2 CI. Remote branch and updated PR description are verified. Latest ad9ffada CI has one successful/three running/nine queued workflows; 18 successful jobs, one skip, 121 queued and three running. It is not accepted. The original M8.5 acceptance review remains pending. UI names remain provisional. Evidence: /work/nov14-owners-main-check/result.json and /work/nov14-owners-main-sdk.json; original requirements mapping remains in validation history. |
| M8.5 / migration #27 | e6d2fe6b prunes excluded directories before traversal and reports unreadable included directories as incomplete scans. Twelve review categories, eighteen tests and actual installed/relocated CLI checks pass, including OS-directory audit and explicit excluded-file selection; the previous checker fails that gate. | Accepted and merged as a6d77cbf on 2026-11-14: exact-head 3430c995 passes all eight workflows, 204 successful jobs and two skips. Ruff/strict docs pass. Evidence: /work/nov06-sum/migration-pruning-installed-check.json and migration-pruning-negative.json. Local 1d71eb21 adds HP013 for original HPGCC3 sys_map_v2p/sys_map_p2v calls, with actionable physical-address/lifetime migration guidance. Twenty tests, Ruff, strict docs and actual installed/relocated CLI checks pass; temporary SDK removed. Evidence: /work/nov14-migration-address.json. Published in #43 / ad9ffada with a fresh full installed/relocated SDK gate; new CI is pending; broader M8.5 ownership/lifetime and downstream requirements remain open. |
| M8.6 / applications #14 | Notebook retains six-model persistence/backup (18 launches), two-model SD export (4 launches) and independent data/pixel/return checks. Current UI owner Stock maxima: app stack 2432/8192, stage stack 3224/4096 bytes, frame/input 42586/100000 and 2562/5000 us; both native heaps recover their baseline capacity and largest blocks. | #14 delivery is CI accepted on its allocation base branch; integration into main and whole M8.6 release acceptance remain open. Local 81bac197 Notebook export now writes the text and newline separately, removing the 192-byte copy. Three ASan/UBSan Notebook targets pass, including newline-write failure cleanup. Isolated ARMv4T -Os stack reports change from a 224-byte export frame to 32-byte export plus 32-byte writer frames; this is not a whole-system peak or fresh native acceptance. Temporary builds removed; evidence: /work/nov14-notebook-export/result.json. Published in #43 / ad9ffada; fresh full SDK gate passes 613 identical outputs, six Notebook builds and three HP39 Aplet packages. New CI is pending. Independent-launcher entry, allocation/MMU/service coexistence, packaging and migration evaluation remain required. |
| M8.6 / reproducibility #40 | Preceding 4aec9e0d passes all eight workflows, 338 successful jobs and two skips. Independent HP50g entry retains native snapshot/guard/RPL-return validation and six rejecting corruptions. Local 508c359f adds the two-copy artifact/manifest check to Core runtime CI; current f9a2dbf7 integrates main 2c2477f5 with roadmap-only conflict resolution. | Integrated two-copy check passes all five artifacts, including the unchanged previously executed transfer file. Full installed/relocated SDK, six-model Notebook builds, three HP39 Aplet packages and strict docs pass; temporary builds removed. Submitted as 5476624d after verifying preceding 4aec9e0d CI. Remote branch and updated PR description independently verified. New CI is not polled this turn and remains unaccepted. Evidence: /work/nov14-release-main-sdk.json and /work/nov13-independent-entry-live/verified.json. HPC1 loading, allocation/MMU/service coexistence, other families and full release acceptance remain open. |
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

| Delivery | Current acceptance state |
| --- | --- |
| #31 / logical constraints and continuation | Bounded-cost repair 2bd7a499 passed all eleven workflows, 373 jobs and three skips. Main integration and internal registration are locally verified; current CI has an unresolved HP39gs pre-entry transfer failure (see current M8.0 row). Earlier allocation and Stock-return failures remain historical records, not failures of the current uncompleted CI. See the current M8.0 row for the exact integration state and remaining public-registration requirements. |
| #13 / `30cdca4f` | Exact-head audit on 2026-11-12: all 12 workflows pass, 390 successful jobs and two intentional publisher skips. Accepted and merged as `91c631c8`; broader UI resource/lifecycle acceptance remains open. |
| #14 / `35ee317c` | CI accepted and merged into the allocation base branch as 64c4a854; main integration pending. Twelve workflows pass, 396 jobs succeed and three skip. Six-model C++ UI owner Direct/Stock audits, Notebook persistence/backup (18 launches), SD export (4 launches), installed examples and host checks pass. Earlier pre-entry USB/readiness failures remain historical observations; this acceptance does not prove their general root causes or complete native exception-stack, whole-system memory or M8.5/M8.6 requirements. |
| #18 / `c5781e2e` | Accepted and merged as `d47dcf50`: all seven exact-head workflows pass, 195 successful jobs and two intentional publisher skips. Local runtime command environment, argument/exit behavior and missing-tool checks are integrated. Historical transport/IRQ failures remain separately recorded. |
| #19 / `afc9e70f` | Accepted and merged as `89fe4a44`: all seven exact-head workflows and 197 jobs pass, including the failed-download retry. Installed original HPGCC filesystem apps retain two-model Direct/Stock console, raw FAT and settled-return evidence. Broader M8.4 acceptance remains open. |
| #26 / `4571d52c` | Accepted and merged as `26140e1d`: all 12 workflows pass, 390 successful jobs and 2 intentional skips. Local integration tests, strict docs and fresh emulator build pass. Production memory-origin discovery/integration and remaining M8.0 goals stay open. |
| Earlier #27 / `626c2099` | Main integration and installed/relocated SDK checks pass locally; submitted after preceding c4349044 passed both workflows and all 19 jobs. Twelve unit tests, Ruff, strict docs and fresh actual installed/relocated CLI checks pass. HP011 delivery d64c5564 adds KOS tone guidance and passes 13 tests plus actual installed/relocated CLI verification. HP012 delivery 626c2099 adds byte-text/UTF-8 and font guidance; 15 tests and the installed/relocated CLI pass. Its preceding d64c5564 audit has three successful/five queued workflows and 18 successful/90 queued jobs plus one skip; first current-head CI audit has eight workflows and eleven jobs queued, not accepted. Its preceding integration was checked before pushing; new-head CI is not polled in this submission turn and remains unaccepted. Earlier recovery deadline cancellation was addressed by the recovery-only 45-minute budget. Prior HP40gs pre-entry USB failure remains unresolved; no blind retry or broad milestone acceptance claimed. |
| #28 / `92e9e897` | Accepted and merged as `cdd6bd86`: all 11 workflows pass; 383 successful jobs and 2 intentional skips. Evidence publisher retry succeeds, including download/publish steps. M8.2/M8.3 whole-application and native resource-bound acceptance remain open. |
| #29 / `fce14d73` | Accepted and merged as `1c0d1e35`: ten workflows pass, 376 successful jobs and two intentional PR publisher skips. Retransmission pacing is fixed; the original initial packet-loss cause remains unproven. |
| #30 / `97d9eb46` | Accepted and merged as dfb3ee87: four workflows pass, 181 successful jobs and two intentional PR publisher skips. Core manual dispatch and expanded publisher queues are now on main; skipped publishers do not prove live queue behavior. |
| Historical #18 / `439339b1` | Three unresolved failures retained: HP49g+ Direct system-services PWM IRQ `0x46417b03`, HP40gs UI transfer and HP39g+ Notebook transfer. Valid captured packet checks do not establish the original failure causes. |

Accepted deliveries include #15, #17, #20, #22, #23, #24, #25, #26, #28 and #29; #21 was
accepted into #20. Their detailed gates, older heads, failure counts and
artifact evidence remain in [M8 validation history](m8-validation-history.md).
The [prior status snapshot](m8-validation-history.md#2026-10-30-current-status-consolidation)
preserves the detailed text replaced by this current-state summary. All original
stage contracts remain in the [requirements archive](roadmap-history.md#m8-stages).


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
