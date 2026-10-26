# hpclang roadmap

Current checkpoint: 2026-10-27. **M8 is incomplete; only M8.1 is accepted complete.**
Docker/WSL is restored. Both-model reservation retests pass guarded recovery,
repeat restart, absent-reservation recovery and all 16 rejection cases per model.
Accepted main: `038b0f41` (PR #22 return-root coverage, elapsed-time PWM witness and M0–M7 overview accepted; SD Reset #15 and opt-in legacy formatting #17 retained). Local passing tests, submitted CI, CI acceptance and
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
| M8.4 HPGCC source adapters | SD Reset #15 and opt-in legacy formatting #17 accepted; stage open | Complete retained historical application/API coverage, documented migrations and pending branch acceptance |
| M8.5 Modern C++/migration | Implemented in part; stage open | Complete ownership/error/lifetime contracts, migration tooling/guidance and public downstream examples |
| M8.6 Applications/release | Stage open | Nontrivial integrated apps, clean SDK use, cross-family resource/return checks, documentation and release acceptance |

Detailed stage contracts are preserved under [M8 stages](roadmap-history.md#m8-stages).
UI must retain the user's declarative C design: nested ordinary C declarations,
stable keyed identity, controlled inputs, explicit asynchronous state and
revision-driven canvas. A superficially similar imperative API is not completion.

## Current work: M8.0

Port reads, several CRUD operations, Port2 append/replacement/logical deletion and
selected recovery/retirement flows already have evidence. Those do not complete
native port services. The active dependency is execution-reference coverage for
safe movement and library registration; sound compatibility is not its prerequisite.
Submitted guarded TEMP traversal records allocation boundaries/footer dependencies;
candidate sanitizers/analysis/docs pass and its 59,656-byte image and launchers
match both-model native cache round trips. Full opaque/banked
provenance and public registration remain open.

Submitted guarded dispatch decoding retains both caller-proven schemas' read dependencies without interpreting scalar type IDs as pointers. Sanitizer and ARM checks pass; retained two-model fixtures cover 42 entries and 332 observer interruptions. Prior Quality formatting failures were corrected without changing the Python AST. Current native CI failures and local fixes are tracked below. Complete handler identity, embedded-code provenance and execution-reference coverage remain open.

Port2 startup: a real supported-ID-769 HP50g fixture now passes owned installation, guarded low-memory recovery and repeat restart, retaining 23,900 user elements. Absent-reservation/rejection cases and HP49g+ remain unrun for this binding. Earlier traces are explicitly ID1701, not ID4; 14 configured Stock libraries precede minimum supported ID769, and complete startup safety remains open.

Port0 growth-reservation fix remains local (`5771b470`, integrated with parent as `96852e99`): the wrapper requests 240 bytes instead of 16 and retains native-page fences. All six models pass their complete Stock object-mutation lifecycle: three invocations each on HP48gII/HP49g+/HP50g and 15 each on HP39g+/HP39gs/HP40gs, totaling 54 checked phases with settled return. After accepted-main integration, all three family payloads/result addresses and six launchers are byte-identical to the tested artifacts. Envelope tests, Ruff and strict docs pass. No new #24 CI or acceptance is claimed.

Prior #24 `0e68b3b1` fails both code-relocation jobs 121826775448/121826775572 at native-detach; exact HP49g+/HP50g snapshots establish the same fixture allocation failure. Exact CI RAM and matching ELF identify a fixture allocation failure before home_detach: 246,907 requested snapshot bytes exceed the entire 229,376-byte application RW region; TLS errno is ENOMEM (12). The local fixture now packs two nibbles per byte (123,454 bytes for this case), retaining comparison of every nibble. ARM build, format and static analysis pass; fresh two-model native detach/reattach/cache validation is now running sequentially; no passing result is claimed yet. The latest prior-head audit has five failed jobs, 285 successful, 49 queued, 14 running and one skipped. Additional HP50g object-mutation job 121836646822 reproduces the same 13-nibble capacity versus 64-nibble growth, with ENOSPC in captured TLS. All five failures map to the two investigated causes; CI remains unaccepted. Integration ASan/UBSan passes port, mutation, reference and physical-mapping tests.

Current implementation and evidence:

- Draft [#24](https://github.com/maxmixalex/hpclang/pull/24), `0e68b3b1`, submits
  guarded internal catalog insertion/removal, proven-reference/register repairs,
  physical mapping/alias checks, Port1 commit errors and native-page fences.
  Removal revalidates bindings, contracts metadata/TEMP/return-stack storage and
  publishes count last. Sanitizers cover no-write rejection and commit failures;
  ARM analysis and strict docs pass. The submitted follow-up rejects HOME-attached
  IDs and malformed/unreadable attachment prefixes before removal writes;
  sanitizer and ARM checks pass. The preceding 56,016-byte build matches four
  original Stock cases on HP49g+/HP50g, including BUSY/no-write rejection for
  genuinely attached library 275. Internal HOME detachment now moves its prefix
  while preserving directory-object addresses; sanitizer fault tests and ARM
  analysis pass. Both HP49g+/HP50g native detachment cases now return to Stock
  with the directory body and surviving attachments preserved; 372 binding,
  100 entry and 66 attachment corruption controls reject. The preceding 56,800-byte
  candidate and both launchers match those two tested detachment cases; candidate
  sanitizers and strict docs pass. Public unload/provenance and CI remain open.
- Submitted HOME attachment now appends caller-proven fields, repairs the moving
  prefix and publishes count last. Sanitizers cover empty/populated tables,
  attach/detach round trips, eight no-write rejection cases and every commit
  write boundary. ARM analysis/docs pass. Both HP49g+/HP50g native reattachment
  cases return to Stock with directory contents and all attachment fields intact;
  372 binding, 100 entry and 72 attachment corruption controls reject.
  The 57,680-byte candidate and both launchers match the tested reattachment
  artifacts; candidate sanitizers/docs pass. Submitted identity-checked hash/message
  resolution matches 24 original HOME records after real ROM bank switches;
  312 header corruptions reject, with sanitizers/ARM analysis/docs passing.
  Submitted DOACPTR cache insertion/removal now initializes 15-nibble objects before
  publishing geometry and rejects live references on removal. Sanitizers cover
  round trips, adjacent objects, exact field boundaries and every write cut; ARM
  analysis/docs pass. HP49g+/HP50g native cache round trips allocate and
  attach two wrappers, reject removal while attached, reclaim them and restore
  original fields/cache/roots before Stock return. Independent controls reject
  372 bindings, 100 entry records and 72 attachment fields. The delivery image
  and both launchers match the tested 59,280-byte build; candidate checks pass.
  Submitted ROM name lookup through new wrappers now matches unmodified Stock on
  both models; 62 corrupt program encodings reject. Removing HOME's hash pointer
  on HP50g yields an unresolved global name instead of the library command.
  Message compatibility also passes on both models with 108 corruption controls,
  but clearing/redirecting HOME's message field still returns the original message.
  This does not prove use of the new message wrapper. A new Port0 library now
  returns its unique message on both models only while its catalog record exists;
  all four registered/unregistered cases pass, with 476 source corruptions rejected.
  The submitted 59,352-byte image and launchers match those cases and also pass
  fresh command-name lookup on both models. Candidate analysis/lint/docs pass.
  Built-in message routing, lifetime/transaction integration, configuration,
  public registration/provenance and CI acceptance remain open.
- Original-ROM checks cover selected transfers, descriptors, MMU preparation
  and returns. Submitted PrRUN helper execution now runs real SVC allocation/free
  services to stage0: 32 two-model baseline/move cases, 16 legacy prefix cases
  and eight corrupt-input controls pass. The direct helper branch grows the
  pool from 78 to 98 pages. Submitted stage0 rejection now executes the original
  free service and real interpreter return: 48 invalid-pool cases, 32 baseline
  cases and two omitted-free controls pass. Submitted stage0-to-stage1 entry now
  passes 16 two-model/cursor cases and six copy/clear/ABI omission controls;
  every image/zero-tail byte must be written. Translation uses modeled physical
  page aliases. Normal application execution/exit and the helper MMU branch
  remain open; this entry check neither executes nor simulates a return.
  Candidate-local reruns pass; this private-state matrix is separate from CI,
  whose ordinary fixture is rejected by the pinned cache-layout check.
  Native-entry observations and original Stock returns provide separate evidence.
  See the [maintained checks](https://github.com/maxmixalex/hpclang/blob/0e68b3b1/docs/development/e2e.md#offline-original-rom-threaded-return-check)
  for exact preconditions, controls and remaining opaque/banked-reference gaps.
- Recovery retests retain guarded/repeated/absent recovery and 16 rejection
  cases per model, with measured admission/recovery bounds. Port1 append retains
  109 write-failure boundaries and two success cases. The submitted public adapter
  now stops on incomplete native publication rather than returning a normal error;
  ten publication cuts, two recoverable body faults and both exit regressions
  pass, with ARM build/analysis and docs. Public library registration, Port1
  replace/remove and Port2 startup/reclamation remain open.

- Submitted Port0 commit-failure handling passes four operation write-cut matrices, root-before-write and allocation/preflight controls, plus four public API terminal-path cases. Delivery sanitizers, ARM analysis and strict docs pass. The 62,088-byte image, both launchers and verifier inputs match four HP49g+/HP50g registered/unregistered message cases; 476 source corruptions reject. Included in #24 `0e68b3b1`; the current CI investigation is recorded above. New CI and parent acceptance remain required.

Next actions, in order:

1. Complete launcher/opaque-object provenance and bank/register coverage for
   relocation plans before enabling public library management and Port1 edits.
2. PR #22 is accepted as `038b0f41`: all nine workflows pass, with 354
   successful jobs and two skipped. M0–M7 summaries are restored on main.
   #23 `7dadd8a0` merges accepted main after all seven preceding-head workflows
   passed. Integration sanitizer checks and strict docs pass; new CI is submitted.
   #24 remains unaccepted pending the growth-reservation validation described above. Earlier unrelated PWM/watchdog and HP39 transfer failures
   retain their documented limits; later local passes alone do not prove causes.
3. Complete retained Port2 trusted recovery installation, authority lifecycle,
   safe startup and reclamation. Preserve the independent M8 stage requirements.
   During CI wait, UI pool/RW checks pass all six retained Stock CLI cases.
   Submitted #21 `00d5ddf1` adds UI budgets and watchdog diagnostics. Its fresh
   HP39g+ Stock immediate retest passes with settled return; the prior failure
   cause remains unproven. Full CI and total system peaks remain open.

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
