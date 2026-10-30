# hpclang roadmap

Current checkpoint: 2026-10-30. **M8 is incomplete; only M8.1 is accepted complete.**
Docker/WSL is restored. Earlier ID1701 two-model reservation retests pass guarded
recovery, repeat restart, absent recovery and 16 rejection cases per model.
Supported ID769 reservation matrices now pass on both models; full startup
safety and M8.0 remain incomplete.
Accepted main: `1c0d1e35` (Kermit resend pacing #29, resource snapshots #20, guarded dispatch/origin foundations #25, native catalog/HOME and Port0 fixes #24, physical spans #23, return roots and M0–M7 overview #22, SD Reset #15 and opt-in formatting #17 accepted). Local passing tests, submitted CI, CI acceptance and
milestone completion are separate states. The [development board](https://github.com/maxmixalex/hpclang/issues/1)
tracks current tasks; its compact body now matches independent readback.

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
| M8.2 Declarative C UI core | Declaration, layout, input and effects implemented; design reviewed; stage open | Smallest-target memory/update-cost and whole-application resource/lifecycle acceptance; pending follow-up CI |
| M8.3 Components/renderers | Reusable controls and renderer/theme independence implemented; stage open | Complete six-model low-memory, resource-budget and clean-return acceptance; pending follow-up CI |
| M8.4 HPGCC source adapters | SD Reset #15, opt-in formatting #17 and async/KOS tone gates #25 accepted; stage open | Complete retained historical application/API coverage, documented migrations and pending branch acceptance |
| M8.5 Modern C++/migration | Implemented in part; stage open | Complete ownership/error/lifetime contracts, migration tooling/guidance and public downstream examples |
| M8.6 Applications/release | Stage open | Nontrivial integrated apps, clean SDK use, cross-family resource/return checks, documentation and release acceptance |

Detailed stage contracts are preserved under [M8 stages](roadmap-history.md#m8-stages).
UI must retain the user's declarative C design: nested ordinary C declarations,
stable keyed identity, controlled inputs, explicit asynchronous state and
revision-driven canvas. A superficially similar imperative API is not completion.

## Current work: M8.0 mainline and independent UI/C++ acceptance

M8.0 remains the mainline: complete execution-reference coverage and safe
movement for public library management and Port1 edits, then Port2 trusted
recovery installation, authority lifecycle, safe startup and reclamation.
Sound compatibility is independent. Port2 sequencing compromises do not remove
its requirements from M8.

| Work | Proven locally / accepted | Next acceptance gate |
| --- | --- | --- |
| M8.0 native storage, PR #26 | Accepted Port0/HOME and reference foundations; prior `2e7885a4` passes both 14-case Stock matrices. Submitted `51b4566e` adds guarded continuation fetch and accepted Kermit integration, with controlled ROM/source audits and full ARM build passing locally. | New-head CI not polled this turn. Complete launcher/opaque/banked provenance, CONFIG/interrupt semantics, continuation qualification, RAM roots across collection, exclusive production integration and public library/Port1 operations. Prediction alone never authorizes mutation. |
| M8.0 Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support accepted in #25. | Trusted recovery installation, transaction authority lifecycle, safe startup and space reclamation. Mutable-Flash configuration execution remains unqualified; forty resolved data targets retain separate execution obligations. |
| M8.2/M8.3 UI, PR #28 | Six Direct/six Stock UI, application-stack and return checks pass locally. Fixed workload timing records pass the new processing ceilings. | Head `4bcd3fb6` awaits CI acceptance; finish native/exception stack bounds, whole-application peak memory, low-memory/lifecycle and resource acceptance. Saved timing replay is not hardware latency evidence. |
| M8.4 compatibility | SD Reset #15, explicit opt-in formatting #17 and async/KOS sound adapters #25 accepted. | Retained historical application/API coverage and remaining migration/branch acceptance. |
| M8.5 C++ ownership, PRs #13/#14 | Six-model Object Basics and Stock UI lifecycle checks pass locally; submitted SDK gate reproduces 517 outputs. | Parent `97d5ba74` and child `d3e27922` await CI; complete ownership/error/lifetime and downstream application acceptance. |
| M8.5 migration, PR #27 | HP007 explicit-format guidance and installed checker pass locally. Documentation now matches accepted decimal-format and KOS sound behavior; follow-up prepared for submission. | CI remains queued at the last audit; retain complete migration guidance and application coverage. |
| M8.6 applications/release | Existing applications provide integration foundations. | Nontrivial integrated applications, clean SDK consumption, cross-family resources/return, documentation and release acceptance remain open. |

Local M8.0 guarded instruction-fetch checks pass sanitizer tests and ARM analysis.
The maintained ROM gate now retains original return cases and adds controlled
returns to an inspected instruction boundary, with remapping rejected before reads.
Both models pass sixty added cases, sixty retained original returns and 896
CPU comparisons. Arbitrary continuation provenance, device qualification and
production integration remain open.

M8.5 cooperative timer implementation `e0f167de` passes local validation:
real C/C++ sanitizer tests, five host/ARM analysis configurations, and the
installed SDK gate (549 identical outputs). All six Direct/six Stock system-services
cases and six Direct installed-example cases pass independent source/artifact
and settled-return audits. Integration retains the original input-pixel gates;
the example observer checks stdout progress and at least five seconds elapsed.
Standalone-example Stock execution and hardware testing are not claimed.
Remote acceptance and broader M8.5 completion remain open.

## Pending delivery and unresolved failures

| Delivery | Current acceptance state |
| --- | --- |
| #13 / `97d5ba74` | Latest audit: nine successful, two queued and one pending workflow; 265 successful, 102 queued, 21 running, one skipped and one pending job. No failed/cancelled jobs; acceptance remains incomplete. |
| #14 / `d3e27922` | Latest audit: eight queued workflows and twelve queued jobs; no failed/cancelled jobs. Timer local validation is complete; remote acceptance remains open. |
| #26 / `51b4566e` | Submitted guarded continuation fetch and accepted main integration; full ARM application/six launchers build and ROM source-identity audit pass locally. Prior head `2e7885a4` had ten queued workflows/sixteen queued jobs. New-head CI not polled this turn; earlier failures remain in history. |
| #27 / `583f38e6` | Latest audit: two successful/five queued workflows; ten successful/78 queued jobs. No failed/cancelled jobs; not accepted. Earlier cancellation remains in history. |
| #28 / `4bcd3fb6` | Latest audit: eight queued/two pending workflows and sixteen queued jobs. No failed/cancelled jobs; local UI evidence does not replace remote acceptance. |
| #29 / `fce14d73` | Accepted and merged as `1c0d1e35`: ten workflows pass, 376 successful jobs and two intentional PR publisher skips. Retransmission pacing is fixed; the original initial packet-loss cause remains unproven. |
| Historical #18 / `439339b1` | Three unresolved failures retained: HP49g+ Direct system-services PWM IRQ `0x46417b03`, HP40gs UI transfer and HP39g+ Notebook transfer. Valid captured packet checks do not establish the original failure causes. |

Accepted deliveries include #15, #17, #20, #22, #23, #24, #25 and #29; #21 was
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
from the relevant PR. Keep historical evidence in the frozen archive; do not
append per-turn experiment narration to the roadmap or board. Record only
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
