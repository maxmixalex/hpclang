# hpclang roadmap

Current checkpoint: 2026-10-29. **M8 is incomplete; only M8.1 is accepted complete.**
Docker/WSL is restored. Earlier ID1701 two-model reservation retests pass guarded
recovery, repeat restart, absent recovery and 16 rejection cases per model.
Supported ID769 reservation matrices now pass on both models; full startup
safety and M8.0 remain incomplete.
Accepted main: `3c484673` (guarded dispatch/origin foundations #25, native catalog/HOME and Port0 fixes #24, physical spans #23, return roots and M0–M7 overview #22, SD Reset #15 and opt-in formatting #17 accepted). Local passing tests, submitted CI, CI acceptance and
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
| M8.2 Declarative C UI core | Implemented substantially; stage open | Complete whole-application resource/lifecycle acceptance and pending CI; original design reviewed, six host UI regressions pass locally |
| M8.3 Components/renderers | Implemented substantially; stage open | Complete reusable components, renderer/theme independence and six-model interaction/resource acceptance |
| M8.4 HPGCC source adapters | SD Reset #15, opt-in formatting #17 and async/KOS tone gates #25 accepted; stage open | Complete retained historical application/API coverage, documented migrations and pending branch acceptance |
| M8.5 Modern C++/migration | Implemented in part; stage open | Complete ownership/error/lifetime contracts, migration tooling/guidance and public downstream examples |
| M8.6 Applications/release | Stage open | Nontrivial integrated apps, clean SDK use, cross-family resource/return checks, documentation and release acceptance |

Detailed stage contracts are preserved under [M8 stages](roadmap-history.md#m8-stages).
UI must retain the user's declarative C design: nested ordinary C declarations,
stable keyed identity, controlled inputs, explicit asynchronous state and
revision-driven canvas. A superficially similar imperative API is not completion.

## Current work: M8.0

Guarded references, bounded ROM readers and mapping/guard composition are
submitted in #26 (`dc6fbd39`); CI acceptance is pending. The reader's 28 Stock
cases and independent Flash/artifact audit remain scoped to that submitted work.

A local mapped-graph scheduler now follows inline/compact children and explicitly
resolved cross-mapping edges, deduplicates cycles, and records mapping-qualified
read dependencies. Sanitizer tests and ARM compile/analysis pass. Actual root
classification, firmware/calling-context qualification, complete reference
resolution, public libraries and Port1 edits remain open. Both saved HOME states resolve 20 attachment
objects. Local original-ROM regression now covers two additional switch entries
(banks 2/4), including ERAM precedence and bank-2 exit. These entries read
mutable RAM selectors: original-ROM counterexamples change their selected bank
without changing code. Guarded alias lookup is local; production integration must
guard selector data as well as code. Payload/execution coverage remains open.

The active dependencies are complete execution-reference handling for safe
movement/library management, and Port2 trusted installation, authority lifecycle,
startup safety and reclamation. Sound compatibility is independent.

| Work | Verified state | Remaining gate |
| --- | --- | --- |
| Native references and libraries | #25 accepted; #26 follow-up submitted. Guarded TEMP lookup and masked roots pass 304 controlled return cases/480 rejections, sanitizer/API checks and ARM builds. The corrected register-root build passes all 28 HP49g+/HP50g Stock call/catalog cases, with source/artifact hashes and unchanged Flash independently checked. This exercises default RPL3; explicit RPL4 masks retain host/controlled-fixture coverage only. | Complete launcher/opaque/banked provenance, full handler semantics, RAM continuations across exhausted-stack collection, library configuration/public management and a production banked-reference guard for Port1 replace/remove |
| Port0 growth / HOME snapshots | #24 fixes pass 54 six-model lifecycle phases and ten HP49g+/HP50g native library cases locally; sanitizers, ARM analysis, package checks and strict docs pass | Accepted in #24; retain lifecycle regressions |
| Port2 supported binding 769 | Both HP49g+ and HP50g pass guarded/repeated/absent recovery and all 16 rejection cases per model; independent saved-state checks pass | Complete trusted installation, authority lifecycle, safe startup and reclamation |
| Explicit recovery ID | Local primary `4c0f6044`, delivery `2dcb4aa5`; 73 recovery tests and Ruff pass on both; generated witnesses match retained native packages | Accepted in #25. HP49g+ used the preceding harness, HP50g continuation uses the explicit-ID helper |

Guarded ID769 tests retain 23,900 user elements. Earlier complete two-model
matrices used ID1701. Fourteen configured Stock libraries precede supported
ID769; passing these recovery fixtures does not prove complete startup safety.
All prior artifact-specific evidence and limitations are retained in
[M8 validation history](m8-validation-history.md).

## Pending delivery

| PR / head | State |
| --- | --- |
| #26 / `dc6fbd39` | **CI pending:** 3 successful and 7 unfinished workflows; 119 successful, 19 running and 233 queued jobs, no failures observed. Earlier HP39g+ pre-entry Kermit failure remains unexplained. Graph/alias work remains local. |
| #25 / `752b8999` | **Accepted and merged as `3c484673`.** All eight workflows pass: 355 successful jobs and one intentionally skipped PR publisher. Push evidence publication succeeded. |
| #24 / `6ff8a1e9` | **Accepted and merged as `fee4e4b9`.** All 12 workflows pass: 390 successful jobs; two PR evidence publishers intentionally skipped. Follow-ups are tracked below. |
| #21 / `969ade88` | **CI accepted and merged into #20 as `1ce01859`.** Seven workflows pass: 319 successful jobs and one conditional PR evidence-publisher skip. This does not complete M8.2/M8.3 or accept the parent branch. |
| #18 / `439339b1` | **Three failed jobs remain unresolved.** HP49g+ Direct system-services fails (recorded PWM IRQ `0x46417b03`). HP40gs UI and HP39g+ notebook stall in pre-entry payload transfer. All 3,539 audited data/ACK/NAK frames pass length/sequence/CRC checks at the socat capture point; final device Timeout frames have invalid sequence fields but valid CRCs. Root causes remain unproven. |
| #20 / `1ce01859` | **CI pending:** 2 successful and 2 unfinished workflows; 167 successful, 1 running and 1 queued jobs, no failures observed. Historical HP50g PWM failure remains unproven; mainline and lifecycle acceptance remain open. |

Accepted deliveries: #25 merged as `3c484673` (355 successful jobs, one conditional publisher skip); #24 merged as `fee4e4b9` (390 successful jobs, two conditional publisher skips); #22 merged as `038b0f41` (354 successful jobs, two skipped),
#23 as `f806b299` (373 successful jobs, one skipped). The earlier #24 head
`0e68b3b1` failed eight jobs: four insufficient Stock-growth reservations and
four oversized test snapshots. All eight saved states were investigated; the
current submitted head contains both fixes. A separate publisher cancellation
was caused by a higher-priority pending request, not accepted as a passing job.

Next: complete execution-reference coverage
and public library/Port1 operations; inspect outstanding CI before delivery and
accept only proven gates. Continue Port2 installation/startup/reclamation within
M8, and retain every M8.2–M8.6 requirement above.

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
