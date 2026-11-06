# hpclang roadmap

Current checkpoint: 2026-11-06. **M8 is incomplete; only M8.1 is accepted complete.**
The development container exited 255 and was restarted after terminal-state confirmation. The resumed Notebook matrix and independent six-model audit now pass; HP39g+ recovery provenance is retained. Earlier ID1701 two-model reservation retests pass guarded
recovery, repeat restart, absent recovery and 16 rejection cases per model.
Supported ID769 reservation matrices now pass on both models; full startup
safety and M8.0 remain incomplete.
Accepted main: `d47dcf50` (original HPGCC filesystem gates #19, native execution-origin foundations #26, UI resource observers #28, CI queue/manual dispatch #30, Kermit resend pacing #29, resource snapshots #20, guarded dispatch/origin foundations #25, native catalog/HOME and Port0 fixes #24, physical spans #23, return roots and M0–M7 overview #22, SD Reset #15 and opt-in formatting #17 accepted). Local passing tests, submitted CI, CI acceptance and
milestone completion are separate states. The [development board](https://github.com/maxmixalex/hpclang/issues/1)
tracks current tasks; its compact body now matches independent readback.

Container housekeeping: removed 16,962 inactive CMake object files (313 MiB allocated blocks); sources, final artifacts, saved-state evidence and active validations retained. Manifest: `/work/nov04-old-object-cleanup.json`.

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
| M8.2 Declarative C UI core | Declaration, layout, input and effects implemented; design reviewed; stage open | Smallest-target memory/update-cost and whole-application resource/lifecycle acceptance |
| M8.3 Components/renderers | Reusable controls and renderer/theme independence implemented; stage open | Complete six-model low-memory, resource-budget and clean-return acceptance |
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
| M8.0 native storage, PRs #26/#31 | Local d09703ef adds selected-field increment/decrement provenance and repair constraints; 24,576 C/original-ROM cases pass. Numeric continuation reaches 92 steps (stop 0xb2ab), with relocation qualification still at 87 steps. Both models pass actual catalog-movement ROM replay and maintained Stock collection through 87 steps: 39 address uses, 159 source constraints, six stores/32 final digits. Fourteen sanitizer targets, 27,648 predicate ROM cases, launcher/ARM checks pass; fixture remains capped at 40 KiB. | Qualify the new arithmetic producer in relocation replay, close remaining execution references and integrate production with whole-application memory acceptance. Production Stage1 remains at 0x67d2. Public libraries, Port1 and retained Port2 goals remain open. |
| M8.0 Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support accepted in #25. | Trusted recovery installation, transaction authority lifecycle, safe startup and space reclamation. Mutable-Flash configuration execution remains unqualified; forty resolved data targets retain separate execution obligations. |
| M8.2/M8.3 UI, PR #28 | Accepted in cdd6bd86 after all 11 workflows pass. Application/launcher stack observers, return-capacity checks and update-cost gates are merged. | Finish borrowed-stack attribution, reservation bounds and whole-application resource/lifecycle acceptance. |
| M8.4 compatibility | SD Reset #15, explicit opt-in formatting #17 and async/KOS sound adapters #25 and original filesystem gates #19 accepted. | Retained historical application/API coverage and remaining migration/branch acceptance. |
| M8.5 C++ ownership, PRs #13/#14 | Shipped timer example is integrated into system-services E2E. All six Direct and six Stock cases pass the complete independent input/artifact/pixel/settled-return audit. ARM build/analysis, source quality, strict docs and API-conventions installation pass. | Prior timer head 41bfa65d passed seven workflows (337 successful jobs, one skipped). Parent/child branch acceptance and broader ownership/lifetime/application requirements remain open. |
| M8.5 migration, PR #27 | Installed migration checker, numeric-separator regression and ownership guidance implemented; installation/relocation previously pass. UI-main integration passes seven container tests, Ruff and strict host docs. | Submitted `54b5291f` for fresh CI; branch and broader migration acceptance remain open. |
| M8.6 applications/release | Notebook ON fix and maintained entry gate pass all six Stock models; independent audit verifies inputs, artifacts, focused-editor pixels and settled return. Lifecycle sanitizer tests cover six model IDs/three storage configurations. Independent-launcher packaging evaluation is documented in local 6abcb849; option retained. | Notebook follow-ups submitted in 384ae39d; validate CI; complete release resource/error/return and documentation acceptance. |

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
All six standalone installed-example Stock executions pass the full independent source/artifact/settled-return audit (/work/oct31-timer-example-stock/independent-audit.json). This complements the six Direct cases; physical hardware acceptance is not claimed.
Remote acceptance and broader M8.5 completion remain open.

## Pending delivery and unresolved failures

| Delivery | Current acceptance state |
| --- | --- |
| #31 / `7b9f1d35` | Latest inspected exact-head CI workflows: 6 queued, 5 success, 1 in_progress; jobs: 185 success, 192 queued, 12 in_progress, 1 skipped; no remote acceptance yet. Submitted batch contains native 81BE, four-store checks and the locally verified large-HOME detach fix. Local HST/stack/CONFIG/payload/maintained-continuation/predicate/constant/arithmetic batch through d09703ef is validated but not yet pushed. Numeric continuation reaches 92 steps; qualified relocation replay and maintained Stock remain at 87 steps/six stores. Full production continuation remains open. |
| #13 / `30cdca4f` | Submitted integration 7cf08668; latest exact-head CI workflows: 10 success, 1 queued, 1 pending; jobs: 376 success, 5 queued, 8 in_progress, 1 skipped, 1 pending.  Previous exact head ab7842f2 passes all seven workflows: 195 successful jobs and two intentional publisher skips. Latest main d47dcf50 is integrated; only documentation conflicts were resolved, preserving both histories. Runtime/UI/build inputs remain unchanged; the two setup-runtime files equal accepted main. Four script tests and strict docs pass. Integration delivery requires fresh CI before merge; broader resource/lifecycle acceptance remains open. |
| #14 / `384ae39d` | Integration 3ce8acb0 passes fresh six-model persistence/maximum-length backup and independent audit (18 launches), plus two-model SD export (4 launches) with retained raw-card re-read. All 172 C/C++ sanitizer tests, 25 focused Python tests and strict docs pass. Previous 3312be9c CI: 9 successful workflows/1 failure, 383 successful jobs/2 skips/1 failure; HP40gs system-services USB failure before entry remains unresolved. Fresh delivery remains unaccepted. Parent acceptance and broader M8.5/M8.6 completion remain open. Latest exact-head CI workflows: 11 success, 1 failure; jobs: 400 success, 2 skipped, 2 failure. Two Stock failures before application entry: hp39gs system-services and hp39gplus display USB host-gate-failed. Logs retained; root causes unresolved, no blind retry. |
| #18 / `c5781e2e` | Accepted and merged as `d47dcf50`: all seven exact-head workflows pass, 195 successful jobs and two intentional publisher skips. Local runtime command environment, argument/exit behavior and missing-tool checks are integrated. Historical transport/IRQ failures remain separately recorded. |
| #19 / `afc9e70f` | Accepted and merged as `89fe4a44`: all seven exact-head workflows and 197 jobs pass, including the failed-download retry. Installed original HPGCC filesystem apps retain two-model Direct/Stock console, raw FAT and settled-return evidence. Broader M8.4 acceptance remains open. |
| #26 / `4571d52c` | Accepted and merged as `26140e1d`: all 12 workflows pass, 390 successful jobs and 2 intentional skips. Local integration tests, strict docs and fresh emulator build pass. Production memory-origin discovery/integration and remaining M8.0 goals stay open. |
| #27 / `54b5291f` | HP40gs USB failure reproduced with exact CI inputs before application entry. CI/replay fail at different decoded-wire offsets (8646/21710); sent prefixes and retries match the staged source. All 1011 replay OUT packets are fully drained; complete failed-packet copies remain in SRAM. Buffer ownership/parser consumption and root cause remain unresolved. No blind CI retry or acceptance claimed. |
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
