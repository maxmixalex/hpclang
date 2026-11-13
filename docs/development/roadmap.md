# hpclang roadmap

Current checkpoint: 2026-11-13. **M8 is incomplete; only M8.1 is accepted complete.**
The development container exited 255 and was restarted after terminal-state confirmation. The resumed Notebook matrix and independent six-model audit now pass; HP39g+ recovery provenance is retained. Earlier ID1701 two-model reservation retests pass guarded
recovery, repeat restart, absent recovery and 16 rejection cases per model.
Supported ID769 reservation matrices now pass on both models; full startup
safety and M8.0 remain incomplete.
Accepted main: `7d1a5bb9` (#39 Stock readiness and #27 migration tooling accepted); #14 is CI accepted into `m8-allocation-delivery`, with main integration pending; previous checkpoint `c394fe7c` (#38 original-application/tone acceptance correction accepted; #37 UI acceptance documentation accepted; #36 automatic documentation PR CI and #35 compatibility docs accepted; #34 Windows-checkout lifecycle tests accepted; #32 historical formatted-input adapters, #33 UI lifecycle and borrowed-stack validation, #13 native allocation budgets and Stock release, original HPGCC filesystem gates #19, native execution-origin foundations #26, UI resource observers #28, CI queue/manual dispatch #30, Kermit resend pacing #29, resource snapshots #20, guarded dispatch/origin foundations #25, native catalog/HOME and Port0 fixes #24, physical spans #23, return roots and M0–M7 overview #22, SD Reset #15 and opt-in formatting #17 accepted). Local passing tests, submitted CI, CI acceptance and
milestone completion are separate states. The [development board](https://github.com/maxmixalex/hpclang/issues/1)
tracks current tasks; its compact body now matches independent readback.

Container housekeeping (2026-11-12): measured volume allocation is 32.94 GiB (initial 41.18 GiB). Removed two completed, reproducible recovery-test output trees, reclaiming 490.14 MiB without archives; generation scripts and original inputs remain. Earlier duplicate font archives and superseded outputs were removed. On 2026-11-13, removed 6,285,928 bytes of completed filesystem-example build and negative archive fixtures from inside the container; original inputs, scripts and reports remain, and deleted build evidence can be regenerated. Keep only active dependencies, unresolved-failure reproductions and irreplaceable inputs.

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

| Stage / delivery | Current evidence | Next gate |
| --- | --- | --- |
| M8.0 / #31 | PR #31 / 2bd7a499 submits runtime 34034bd9's 271-step native repair cost and collector allocation fixes without dropping guards. Snapshot-inclusive replay peaks at 59,054 requested bytes under 60,000; sanitizer tests and 144 ARM analysis configurations pass. Both HP49g+/HP50g full Stock and independent replay pass: 88 basic cases and 19 saved executions per model; independent-audit.json records complete=true. | Sessions 7501/95157 exit 0; reports are under /work/nov06-sum/native271-snapshot-stock. Preceding CI failures were checked before pushing; the 2026-11-14 exact-head 2bd7a499 audit passes all eleven workflows, with 373 successful jobs and three skips. The newer local registration batch is not yet accepted. Local 80c23240 adds RAM-library registration planning and uses it in the native registration/roundtrip fixture. ASan/UBSan port tests and fresh ARM app/launcher build pass with unchanged inputs; not pushed. Fresh sealed-input Stock session 58785 is running under /work/nov14-library-plan-stock; HP49g+ initial application/settled return passes, registration and saved-execution matrix still running. HP50g and independent replay remain pending. Public registration commit/attachment, Port1 and retained Port2 requirements remain open. |
| M8.0 / Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support is accepted in #25. | Trusted installation, authority lifecycle, startup and reclamation remain. Mutable-Flash configuration execution is unqualified; forty data targets retain separate execution obligations. |
| M8.2/M8.3 / UI | #28/#33/#37 and #14 owner/lifecycle deliveries accepted; broader stage gates remain open. Observer series 7698776a/83ba2a83/f10c2c89 passes six-model Stock UI/settled return and independent sealed-input/ROM/trace audit. All 24 real-trace negative variants reject; ten trace/verifier tests and strict docs pass. Sampled minima are 1,256–1,928 usable bytes. | PR #42 now includes automatic Stock UI heap capture (6afc0d82/6294250c): six-model firmware/profile equivalence, fifteen host tests, fresh HP50g UI/settled return and sixteen qualified observations pass. Bash syntax/strict docs pass; Actionlint has only the unchanged unsupported queue-key diagnostic. Exact-head 77b51db1 audit finds ten successful/three failed workflows, 388 successful/three failed jobs and three skips. 85b69df4 fixes the two ShellCheck SC1090 failures by declaring the generated environment source; baseline reproduces the warning, fixed ShellCheck and fifteen heap tests pass. Submitted after inspecting 77b51db1; new CI not polled or accepted. HP39g+ failure artifact 14030257057 confirms directory transfer completed, then payload send-window 23 stalled for 90 seconds (three NAKs/resends), before UI entry. Transport root cause remains unresolved; logs retained under research/nov14-pr42-usb. Completed matrix session 45058 exited 0; /work/nov06-sum/native-heap-matrix/independent-audit.json has complete=true. This measures one allocator, not whole-system peaks. Native exception stacks and whole-system transient admission remain open. |
| M8.4 / compatibility #41 | 257475a0 adds FSChMode without reopening/seeking/truncating, with flush-before-read and historical APPEND/MODIFY normalization. Eight unchanged-original comparisons, three production FatFs/lease sanitizer tests, ARM analysis and strict docs pass. Six Direct/two Stock SD runs and independent source/report/raw-card audit pass; prior original FSShutdown coverage remains. | Preceding 011aa93d CI was checked before pushing: nine workflows pass and two quality workflows fail; 368 jobs pass, two skip and two fail. 6e02b094 fixes the test-only saved_disk early-return leak with fixed-lifetime storage. The original source reproduces the analyzer warning; the fix passes storage_metadata ASan/UBSan, host Clang analysis and formatting. Submitted; new CI not polled or accepted. Evidence: /work/nov14-fsystem-test-fix/result.json and before-analysis.log. Preceding 1fb7e87b was checked before pushing. Evidence: /work/nov13-fsystem-mode/native/independent-audit.json. Removed 9,214,210 bytes of reproducible host build/extraction outputs inside the container. Broader historical and physical coverage remain open. |
| M8.5 / ownership #14 | 35ee317c submits C++ UI owner/entry cleanup, prior Port owners and timers, plus the explicit Stock readiness fix. Owner tests, installed/relocated SDK counter packaging and six-model Direct/Stock audits pass. Local owner acceptance is recorded in 6437244d. | CI accepted and merged into m8-allocation-delivery as 64c4a854 on 2026-11-14; not yet on main: exact-head 35ee317c passes all twelve workflows, with 396 successful jobs and three skips. Six-model independent Direct/Stock audits remain complete. Broader M8.5 ownership/lifetime contracts remain open. |
| M8.5 / migration #27 | e6d2fe6b prunes excluded directories before traversal and reports unreadable included directories as incomplete scans. Twelve review categories, eighteen tests and actual installed/relocated CLI checks pass, including OS-directory audit and explicit excluded-file selection; the previous checker fails that gate. | Accepted and merged as a6d77cbf on 2026-11-14: exact-head 3430c995 passes all eight workflows, 204 successful jobs and two skips. Ruff/strict docs pass. Evidence: /work/nov06-sum/migration-pruning-installed-check.json and migration-pruning-negative.json. Broader M8.5 ownership/lifetime and downstream requirements remain open. |
| M8.6 / applications #14 | Notebook retains six-model persistence/backup (18 launches), two-model SD export (4 launches) and independent data/pixel/return checks. Current UI owner Stock maxima: app stack 2432/8192, stage stack 3224/4096 bytes, frame/input 42586/100000 and 2562/5000 us; both native heaps recover their baseline capacity and largest blocks. | #14 delivery is CI accepted on its allocation base branch; integration into main and whole M8.6 release acceptance remain open. Independent-launcher entry, allocation/MMU/service coexistence, packaging and migration evaluation remain required. |
| M8.6 / reproducibility #40 | Six-model repeated Notebook builds and three specialized HP39 Aplet bundles pass with the installed/relocated SDK. ff3665c1/4aec9e0d add a rebuildable independent HP50g entry fixture and snapshot verifier. Fresh USB/Stock execution from the copied no-library-275 baseline, 128 C results, owned 4 KiB stack/guards, RPL return 124275 and unchanged Flash pass; six corrupted snapshots reject. Ruff and strict docs pass. | Preceding 9c9a3a95 CI passes all five workflows: 39 successful jobs and one skip. Submitted 4aec9e0d; first audit finds one successful/five running/two queued workflows, with sixteen successful/twenty running/eighteen queued jobs and one skip. Not accepted. Evidence: /work/nov13-independent-entry-live/verified.json and maintained-negative.json. Generic install-harness return flags are not used as proof. This closes the bounded entry-fixture batch; HPC1 integration, allocation/MMU/service coexistence, other families and full release acceptance remain open. |
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
| #31 / logical constraints and continuation | Current 2bd7a499 submits the locally accepted bounded-cost/allocation repair; both Stock matrices and independent replays pass. New CI pending. Submitted batch through 699ba4f2 includes the 270-step continuation, qualified native-push execution and f4902802 allocation fix; exact-head 8146537c CI finished: eight workflows pass and two fail; 368 jobs pass, two skip and four fail. All four HP49g+/HP50g Stock code-relocation logs report 'Execution has not returned to Stock ROM'. This is not established as the preceding allocation failure; failure summaries are retained and the local cost investigation is running. Not accepted; no blind retry. Preceding ebefdecf finished: 8 workflows pass, 2 fail; 368 jobs pass, 2 skip, 4 fail. All four HP49g+/HP50g failures halt with allocation error 0x46410005; both CI-SRAM replays retain the same 2,344 guards under a 70,000-byte ceiling after replacing doubling with 512-entry growth. Bank-switch checks pass remotely. Fresh HP49g+ CI-input end-to-end replay stopped before entry at USB send-window 11; that input's full return remains unverified. Earlier HP39gs pre-entry USB failure remains unresolved. |
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
