# hpclang roadmap

Current checkpoint: 2026-11-12. **M8 is incomplete; only M8.1 is accepted complete.**
The development container exited 255 and was restarted after terminal-state confirmation. The resumed Notebook matrix and independent six-model audit now pass; HP39g+ recovery provenance is retained. Earlier ID1701 two-model reservation retests pass guarded
recovery, repeat restart, absent recovery and 16 rejection cases per model.
Supported ID769 reservation matrices now pass on both models; full startup
safety and M8.0 remain incomplete.
Accepted main: `4dda177d` (#36 automatic documentation PR CI and #35 compatibility docs accepted; #34 Windows-checkout lifecycle tests accepted; #32 historical formatted-input adapters, #33 UI lifecycle and borrowed-stack validation, #13 native allocation budgets and Stock release, original HPGCC filesystem gates #19, native execution-origin foundations #26, UI resource observers #28, CI queue/manual dispatch #30, Kermit resend pacing #29, resource snapshots #20, guarded dispatch/origin foundations #25, native catalog/HOME and Port0 fixes #24, physical spans #23, return roots and M0–M7 overview #22, SD Reset #15 and opt-in formatting #17 accepted). Local passing tests, submitted CI, CI acceptance and
milestone completion are separate states. The [development board](https://github.com/maxmixalex/hpclang/issues/1)
tracks current tasks; its compact body now matches independent readback.

Container housekeeping (2026-11-12): measured volume allocation is 32.94 GiB (initial 41.18 GiB). Removed two completed, reproducible recovery-test output trees, reclaiming 490.14 MiB without archives; generation scripts and original inputs remain. Earlier duplicate font archives and superseded outputs were removed. Keep only active dependencies, unresolved-failure reproductions and irreplaceable inputs.

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

## Current work: applications/release acceptance running

User-directed sequencing (2026-11-12): rotate bounded batches across M8.0, M8.2/M8.3, M8.4, M8.5 and M8.6; retain M8.1 regression coverage. Other stages do not wait for every native-storage edge case. UI lifecycle/stack-observer validation (#33) and historical scanning adapters (#32) are accepted. Checkout-portability tests (#34) are accepted. Migration/tooling (#27) and Notebook integration (#14) await their current-head CI. M8.0's 271-step repair passes replay, ARM build and analysis, but native Stock call-root completion remains unresolved; keep that investigation bounded. Preserve all original requirements; sound compatibility remains independent of Port2.

| Work | Proven locally / accepted | Next acceptance gate |
| --- | --- | --- |
| M8.0 native storage, PRs #26/#31 | Accepted foundations and submitted 270-step evidence remain. Local dc9343fb propagates AVMEM changes through qualified native-word producers and advances the maintained fixture to 271 steps/20 stores. Both HP49g+/HP50g actual catalog moves now pass all 271 original-ROM transitions, full SRAM comparison and unchanged Flash; four injected faults per model reject with zero writes. Reference/shadow sanitizer tests, five rounding cases, lineage rejection tests, return verifier and Ruff pass. | dc9343fb is not pushed or CI-accepted. Fresh ARM build passes with unchanged source fingerprints. ARM analysis passes all 144 owned source units using the host Git inventory. Fresh HP49g+/HP50g Stock verification in /work/nov06-sum/native271-stock stopped: HP49g+ base launch/settled return and 88 native reference/control cases pass, but the first call-root case (native) fails require_stock_pc before follow-up probes. Saved PC 0x012215ee maps to hpclang_rpl_reference_edits_add, CPSR mode 0x10 (not abort/undefined). The copied-state 60-second continuation finished without returning to Stock; Flash is unchanged. PC remains in the packed-edit metadata-validation loop, but registers and SP changed, so a fixed dead loop is unproven. ENTRYOBS is populated while EXECOBS retains its initial bytes. Cost investigation completed on both models: 20,021,918 add calls and 2,229,412,280 cumulative packed-table checks per 271-step replay. Local e77ad904 memoizes successful native-word proofs only within one top-level payload expansion and matching dependency context; token/value/lineage checks and the underlying validator remain. Counts fall to 141,196 / 6,552,046. Both models produce byte-identical constraints, guards and full predicted SRAM; reference/shadow ASan/UBSan and fresh-call invalidation checks pass. ARM build passes with unchanged input hashes; ARM static analysis passes all 144 source units using an isolated compile database. Fresh two-model Stock verification is running in session 59474 at /work/nov06-sum/native271-memo-stock: HP49g+ base launch/settled return passes, relocation/call-root checks are still pending. Not yet native Stock or CI accepted. No timeout was extended. Original state remains in native271-stock/verified-cases; HP50g has not run. Full acceptance remains pending. Production discovery, public libraries, Port1 and retained Port2 gates remain open; Stage1 stays at 0x67d2. |
| M8.0 Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support accepted in #25. | Trusted recovery installation, transaction authority lifecycle, safe startup and space reclamation. Mutable-Flash configuration execution remains unqualified; forty resolved data targets retain separate execution obligations. |
| M8.2/M8.3 UI | Accepted #28/#33 implementation and resource gates. Re-read #33 Runtime logs at 236e792b: all six Direct/all six Stock cases pass independent pixels, pressure/recovery, 36 KiB RW admission, stack/heap checks, nine timed frames and 34 key edges. Maxima: stack 2432/8192 bytes, heap 20076/22880 bytes, frame 16351/100000 us, input 2441/5000 us. Stock also passes native capacity recovery and settled return. Shipped-entry host cleanup/restart and async success/error checks remain accepted. | Whole-stage acceptance remains open for native exception-stack reservations and whole-system transient memory admission; sampled application/pool checks do not prove those bounds. Documentation correction 1421b201, integrated with main as 055f2857, is submitted in PR #37. Strict docs pass; CI audit has one workflow passed, one running and one queued (two successful jobs, one deployment skip, one running and 17 queued). Original completed items and all remaining gates are preserved. |
| M8.4 compatibility | Scanning adapters accepted in #32 / 2f60c739; earlier batches #15/#17/#19/#25 remain accepted. Fresh original HPGCC 1.1 window simple.c validation is complete locally: host ASan/UBSan, relocated-SDK six-model packaging, archive hashes/exact migration reversal, and independent all-six Direct/all-six Stock interaction, 15-frame pixel, input-hash and settled-return audits. Documented responsive-layout edits remain explicit. | This window batch is closed; rotate to other M8 work. Documentation correction #35 / 550619ee passed strict Documentation CI with upload/deploy skipped and is accepted as 10ba1de3. Temporary SDK/build outputs (307.27 MiB) and completed Direct/Stock runners (82.35 MiB) were removed inside the container without archives. Audit conclusions, source/migration scripts and native artifact remain; raw runtime evidence requires regeneration. Broader historical coverage/migrations and full M8.4 acceptance remain open; no physical-hardware claim. |
| M8.5 C++ ownership, PRs #13/#14 | Shipped timer example is integrated into system-services E2E. All six Direct and six Stock cases pass the complete independent input/artifact/pixel/settled-return audit. ARM build/analysis, source quality, strict docs and API-conventions installation pass. | Prior timer head 41bfa65d passed seven workflows (337 successful jobs, one skipped). Parent/child branch acceptance and broader ownership/lifetime/application requirements remain open. |
| M8.5 migration, PR #27 | Submitted c4349044 adds HP009/HP010 guidance for legacy SD filesystem and native RPL directory entry points, with explicit model/context limits and per-operation error checks. Twelve unit tests, Ruff and strict docs pass; comments/literals, identifier boundaries, guarded/inactive code and source locations are covered. Actual SDK install/relocation verification passes (session 20998 exit 0): installed source hashes, HP009/HP010 and prior diagnostics, explicit cp1252, exit status and input preservation. Temporary SDK outputs were removed. Prior 44a54abe installed CLI checks pass for HP008, explicit cp1252, exit status and unchanged inputs. | Preceding 44a54abe passed its workflow and all 18 jobs. c4349044 is pushed to PR #27; current-head audit has one workflow passed and one queued; three jobs passed, five are running and 11 queued; acceptance remains pending. Its recovery-only 45-minute budget addresses the prior 5fcdc978 30-minute cancellation, not a proven cleanup stall. Prior HP40gs pre-entry USB failure and broader ownership/migration acceptance remain open. |
| M8.6 applications/release | Notebook runtime b1f7037b passes all six current-source Stock entry audits: focused editor, ON exit, settled return and source/artifact hashes. Stack/heap observations: HP39 models 1544/8192 and 9460/(43432 or 39336) bytes; HP48gII 1904/8192 and 9924/27048; HP49g+/HP50g 1912/8192 and 13660/237224. All 173 host tests have passing coverage after test-only CRLF fix 39ecb6f5; runtime inputs are unchanged. Storage-error retry/lifecycle tests and earlier persistence/backup/export evidence remain. Local 3dfad53e integrates accepted main 4dda177d, resolves roadmap/history conflicts, and retains both Notebook and UI demo lifecycle targets. Strict docs pass; full host sanitizer/Python validation is running in session 71699 at /work/nov06-sum/notebook-main-unit. This merge is not pushed or CI-accepted. | PR #14 / 39ecb6f5 CI: one workflow passed/one running; 18 jobs passed and one running. Previous b1f7037b passed three workflows/161 jobs. Fix PR #34 / 5ff14c4a passed all four workflows and 38 jobs; accepted and merged as 3225188b. Notebook #14 remains unaccepted. Release workflow fix #36 / b1527a4a adds automatic strict documentation builds on documentation PRs; main-only upload/deployment conditions remain. Actionlint, strict docs and diff checks pass locally. PR #36 / b1527a4a passed its automatic strict Documentation build; upload and deployment were skipped as intended. Accepted and merged as 4dda177d. Broader release resource/error/return, documentation and application requirements remain open. |

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
| #31 / logical constraints and continuation | Submitted batch through 699ba4f2 includes the 270-step continuation, qualified native-push execution and f4902802 allocation fix; exact-head 8146537c CI finished: eight workflows pass and two fail; 368 jobs pass, two skip and four fail. All four HP49g+/HP50g Stock code-relocation logs report 'Execution has not returned to Stock ROM'. This is not established as the preceding allocation failure; failure summaries are retained and the local cost investigation is running. Not accepted; no blind retry. Preceding ebefdecf finished: 8 workflows pass, 2 fail; 368 jobs pass, 2 skip, 4 fail. All four HP49g+/HP50g failures halt with allocation error 0x46410005; both CI-SRAM replays retain the same 2,344 guards under a 70,000-byte ceiling after replacing doubling with 512-entry growth. Bank-switch checks pass remotely. Fresh HP49g+ CI-input end-to-end replay stopped before entry at USB send-window 11; that input's full return remains unverified. Earlier HP39gs pre-entry USB failure remains unresolved. |
| #13 / `30cdca4f` | Exact-head audit on 2026-11-12: all 12 workflows pass, 390 successful jobs and two intentional publisher skips. Accepted and merged as `91c631c8`; broader UI resource/lifecycle acceptance remains open. |
| #14 / `b1f7037b` | Integration 3ce8acb0 passes fresh six-model persistence/maximum-length backup and independent audit (18 launches), plus two-model SD export (4 launches) with retained raw-card re-read. All 172 C/C++ sanitizer tests, 25 focused Python tests and strict docs pass. Previous 3312be9c CI: 9 successful workflows/1 failure, 383 successful jobs/2 skips/1 failure; HP40gs system-services USB failure before entry remains unresolved. Fresh delivery remains unaccepted. Parent acceptance and broader M8.5/M8.6 completion remain open. Prior 384ae39d audit: 11 successful workflows/1 failure; 400 successful jobs/2 skips/2 failures. Latest API lookup returns no runs/checks for that old head; retain the observed failures. Previous b1f7037b exact-head CI passed all three workflows and 161 jobs. Current 39ecb6f5 CI has one workflow passed and one queued; seven jobs passed, three running and nine queued, not accepted. Two Stock failures before application entry: hp39gs system-services and hp39gplus display USB host-gate-failed. Logs retained; root causes unresolved, no blind retry. |
| #18 / `c5781e2e` | Accepted and merged as `d47dcf50`: all seven exact-head workflows pass, 195 successful jobs and two intentional publisher skips. Local runtime command environment, argument/exit behavior and missing-tool checks are integrated. Historical transport/IRQ failures remain separately recorded. |
| #19 / `afc9e70f` | Accepted and merged as `89fe4a44`: all seven exact-head workflows and 197 jobs pass, including the failed-download retry. Installed original HPGCC filesystem apps retain two-model Direct/Stock console, raw FAT and settled-return evidence. Broader M8.4 acceptance remains open. |
| #26 / `4571d52c` | Accepted and merged as `26140e1d`: all 12 workflows pass, 390 successful jobs and 2 intentional skips. Local integration tests, strict docs and fresh emulator build pass. Production memory-origin discovery/integration and remaining M8.0 goals stay open. |
| #27 / `c4349044` | Capability hints submitted after 44a54abe passed all 18 jobs. Twelve unit tests, Ruff, strict docs and fresh actual installed/relocated CLI checks pass. New-head CI not polled in the submission turn; not accepted. Earlier recovery deadline cancellation was addressed by the recovery-only 45-minute budget. Prior HP40gs pre-entry USB failure remains unresolved; no blind retry or broad milestone acceptance claimed. |
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
