# M8 validation history

This retained snapshot separates experiment history from current status. It does
not remove completion conditions or establish new acceptance. See the
[current roadmap](roadmap.md) and [original requirements](roadmap-history.md).

## Retained roadmap evidence — 2026-10-28

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

Port2 startup: supported-ID-769 HP49g+ passes guarded low-memory recovery, repeated restart, absent-reservation recovery and all 16 rejection cases, including independent saved-state checks. HP50g now also passes absent-reservation recovery after revalidating its guarded/restart checkpoints; its 16 rejection cases are running. Guarded cases retain 23,900 user elements. Earlier complete two-model results use ID1701. Fourteen configured Stock libraries precede ID769; complete startup safety remains open.

Local explicit-ID follow-up: primary `4c0f6044`, delivery `2dcb4aa5` (not pushed). The harness records the library ID, rejects unsupported IDs before fixture creation and rejects mismatched witness bindings. All 73 recovery Python tests and Ruff checks pass on both branches; ID769 witness packages match both retained native fixtures byte-for-byte. HP49g+ ran the preceding harness; HP50g missing phases use the explicit-ID helper. These are local results, not CI acceptance.

Submitted #24 `6ff8a1e9` passes local validation of all six Stock object-mutation lifecycles: three invocations per classic model and 15 per HP39 model, totaling 54 phases with settled return. The native wrapper reserves 240 bytes instead of 16 without weakening page fences. All three payloads/result addresses and six launchers remain identical after accepted-main integration. Ten fresh HP49g+/HP50g detach, reattach and cache cases also pass with the packed HOME snapshot; saved states, input hashes and every fixture package independently verify. Sanitizers, ARM analysis, envelope checks, formatting and strict docs pass. Last inspected #24 `6ff8a1e9`: 12 workflows (four successful, one running, seven queued); 53 jobs successful, eight running and 135 queued. This is a live CI snapshot, not acceptance.

Prior #24 `0e68b3b1` finished with 345 successful jobs, two skipped, eight failures and one cancelled publisher. All eight failure snapshots confirm two causes: Port0 needs 64 growth nibbles with only 13 available; the test HOME snapshot requests 246,907 bytes against 229,376 total RW bytes. The fixes reserve Stock growth space and pack the complete snapshot into 123,454 bytes. Publisher annotations confirm cancellation by a higher-priority waiting ci-evidence-publisher request. All preceding workflows/jobs were inspected before this delivery; that failed head is not accepted.

Current implementation and evidence:

- Draft [#24](https://github.com/maxmixalex/hpclang/pull/24), `6ff8a1e9`, submits
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

- Submitted Port0 commit-failure handling passes four operation write-cut matrices, root-before-write and allocation/preflight controls, plus four public API terminal-path cases. Delivery sanitizers, ARM analysis and strict docs pass. The 62,088-byte image, both launchers and verifier inputs match four HP49g+/HP50g registered/unregistered message cases; 476 source corruptions reject. Included in #24 `0e68b3b1`; the current CI investigation is recorded above. Fresh delivery CI remains required; both parents are accepted.

Next actions, in order:

1. Complete launcher/opaque-object provenance and bank/register coverage for
   relocation plans before enabling public library management and Port1 edits.
2. PR #22 is accepted as `038b0f41`; M0–M7 summaries remain visible.
   PR #23 is accepted as `f806b299`: all ten exact-head workflows pass,
   with 373 successful jobs and one skipped. #24 integrates this accepted
   parent and passes the local follow-up checks above; fresh CI remains required.
   Earlier unrelated PWM/watchdog and transfer failures retain their documented
   limits; these fixes do not establish their causes.
3. Complete retained Port2 trusted recovery installation, authority lifecycle,
   safe startup and reclamation. Preserve the independent M8 stage requirements.
   During CI wait, UI pool/RW checks pass all six retained Stock CLI cases.
   Submitted #21 `00d5ddf1` adds UI budgets and watchdog diagnostics. Its fresh
   HP39g+ Stock immediate retest passes with settled return; the prior failure
   cause remains unproven. Full CI and total system peaks remain open.


## Retained development board — 2026-10-28

# hpclang development board

M8 is incomplete. Only M8.1 (SDK) is accepted complete. Accepted main: `f806b299` (physical span qualification #23, return-root coverage and M0–M7 overview #22, SD Reset #15 and opt-in legacy formatting #17 accepted). Docker/WSL is restored.

| Stage | State / next gate |
| --- | --- |
| M8.0 Native ports | Active: safe movement and reference coverage, public library management, Port1 edits, Port2 recovery/startup/reclamation |
| M8.1 SDK | Accepted complete; retain regressions |
| M8.2–M8.3 Declarative UI | During verified CI wait: local artifact-bound pool/RW budget checks pass six retained Stock snapshots and invalid-input controls. Whole-system peaks and stage acceptance remain open |
| M8.4 HPGCC adapters | SD Reset accepted; remaining historical coverage and pending CI remain |
| M8.5 C++/migration | Ownership/lifetime contracts, migration guidance and examples remain |
| M8.6 Apps/release | Integrated applications and release acceptance remain |

## Current work

Port2 startup: supported-ID-769 HP49g+ passes guarded low-memory recovery, repeated restart, absent-reservation recovery and all 16 rejection cases, including independent saved-state checks. HP50g now also passes absent-reservation recovery after revalidating its guarded/restart checkpoints; its 16 rejection cases are running. Guarded cases retain 23,900 user elements. Earlier complete two-model results use ID1701. Fourteen configured Stock libraries precede ID769; complete startup safety remains open.

Local explicit-ID follow-up: primary `4c0f6044`, delivery `2dcb4aa5` (not pushed). The harness records the library ID, rejects unsupported IDs before fixture creation and rejects mismatched witness bindings. All 73 recovery Python tests and Ruff checks pass on both branches; ID769 witness packages match both retained native fixtures byte-for-byte. HP49g+ ran the preceding harness; HP50g missing phases use the explicit-ID helper. These are local results, not CI acceptance.

Submitted #24 `6ff8a1e9` passes local validation of all six Stock object-mutation lifecycles: three invocations per classic model and 15 per HP39 model, totaling 54 phases with settled return. The native wrapper reserves 240 bytes instead of 16 without weakening page fences. All three payloads/result addresses and six launchers remain identical after accepted-main integration. Ten fresh HP49g+/HP50g detach, reattach and cache cases also pass with the packed HOME snapshot; saved states, input hashes and every fixture package independently verify. Sanitizers, ARM analysis, envelope checks, formatting and strict docs pass. Last inspected #24 `6ff8a1e9`: 12 workflows (four successful, one running, seven queued); 53 jobs successful, eight running and 135 queued. This is a live CI snapshot, not acceptance.

Prior #24 `0e68b3b1` finished with 345 successful jobs, two skipped, eight failures and one cancelled publisher. All eight failure snapshots confirm two causes: Port0 needs 64 growth nibbles with only 13 available; the test HOME snapshot requests 246,907 bytes against 229,376 total RW bytes. The fixes reserve Stock growth space and pack the complete snapshot into 123,454 bytes. Publisher annotations confirm cancellation by a higher-priority waiting ci-evidence-publisher request. All preceding workflows/jobs were inspected before this delivery; that failed head is not accepted.

Submitted guarded dispatch decoding retains both caller-proven schemas' read dependencies without interpreting scalar type IDs as pointers. Sanitizer and ARM checks pass; retained two-model fixtures cover 42 entries and 332 observer interruptions. Prior Quality formatting failures were corrected without changing the Python AST. Current native CI failures and local fixes are tracked below. Complete handler identity, embedded-code provenance and execution-reference coverage remain open.

1. Draft [#24](https://github.com/maxmixalex/hpclang/pull/24), `6ff8a1e9`, delivers guarded internal catalog insertion/removal, register/embedded repairs, physical mapping and alias checks, native-page fences and Port1 commit-failure handling. Removal validates bindings, contracts metadata/TEMP/return-stack storage and publishes count last. Candidate sanitizer fault tests, ARM analysis and strict docs pass. The submitted follow-up validates the entire HOME attachment prefix and rejects still-attached IDs before removal writes; seven no-write rejection cases, prefix read/encoding controls, sanitizers, ARM analysis and docs pass. The preceding 56,016-byte build passes all four HP49g+/HP50g original-return cases, including BUSY/zero-write checks against genuinely attached library 275; 750 binding and 200 entry corruption controls reject. Internal HOME detachment now preserves following directory-object addresses while moving and repairing the stack/HOME prefix; first/middle/last/sole cases, no-write rejection and every commit write boundary pass under sanitizers, with ARM analysis and docs. Both HP49g+/HP50g native detachment cases now pass with original Stock return, unchanged HOME directory body and preserved surviving attachments; independent checks reject 372 binding, 100 entry and 66 attachment corruptions. Detachment implementation/tests are submitted: the preceding 56,800-byte candidate image and both launchers match the two tested cases, and candidate sanitizers/docs pass; public unload, complete roots/provenance and CI acceptance remain open. Submitted PrRUN helper execution now follows real SVC allocation/free services to stage0: 32 two-model baseline/move cases, 16 legacy prefix cases and eight corrupt-input controls pass. It preserves 78 initial pages and appends 20. The direct-helper mode simulates payload return. Submitted stage0 rejection now executes real ROM free and the original interpreter return: 48 cases across two models, eight cursors and page counts 0/1/257 pass, alongside 32 baseline cases and two omitted-free controls. The pool table/vector and return-state invariants hold. Submitted stage0-to-stage1 entry now passes 16 two-model/cursor cases and six omitted-copy/clear or wrong-model controls, requiring actual writes across the complete image/zero tail. It models physical page aliases and stops before stage1 executes, without a real or simulated return. Normal application execution/exit, whole-heap leak freedom, complete launcher execution and the helper MMU branch remain open. Candidate-local reruns pass; the private-state matrix is explicitly separate from ordinary CI, whose standard fixture fails the pinned cache-layout check.
2. Submitted HOME attachment appends caller-proven fields, repairs the moving prefix and publishes count last. ASan/UBSan covers empty/populated tables, attach/detach round trips, eight no-write rejections and every commit write boundary; ARM analysis and strict docs pass. Both HP49g+/HP50g native reattachment cases return to Stock with unchanged directory contents and complete expected attachment records. Independent checks reject 372 binding, 100 entry and 72 attachment corruptions. The 57,680-byte candidate image, both launchers and verifier inputs match the tested reattachment artifacts; candidate sanitizers/docs pass. Submitted library-ID-checked hash/message resolution matches 24 original HOME records after real ROM bank switching; 312 header corruptions reject. Sanitizers, ARM analysis and docs pass. Banked HOME fields require separately owned DOACPTR wrappers, while direct fields retain their original table pointers. Submitted DOACPTR cache insertion/removal initializes complete 15-nibble objects before publishing geometry and rejects live references before removal. Sanitizers pass round trips, adjacent-object retention, exact field/register boundaries, no-write rejections and every write cut; ARM analysis and strict docs pass. Local HP49g+/HP50g native cache round trips allocate and attach two new wrappers, reject removal while attached, detach/reclaim them and restore original fields, cache extent, accounting and roots before Stock return. The delivery image, both launchers and verifier inputs match the tested 59,280-byte build; candidate sanitizer tests, ARM analysis and strict docs pass; independent controls reject 372 bindings, 100 entry records and 72 attachment fields. Local ROM command-name lookup through retained new wrappers matches the complete unmodified Stock program encoding on HP49g+/HP50g; 62 corrupt program nibbles reject. An HP50g control removing only HOME's hash pointer yields the unresolved global name PrRUN instead of ROMPTR 275/0. Local message compatibility matches original Stock on both models; 108 corrupt message-object nibbles reject. HP50g still returns the original message when HOME's message field is zero or redirected, so this does not prove use of the new message wrapper. Submitted lookup and message tests now include a new Port0 library that returns its unique message on HP49g+/HP50g only while its catalog record exists: four registered/unregistered cases pass locally, and 476 source corruptions reject. The submitted 59,352-byte image and launchers match those four cases and also pass fresh command-name lookup on both models; candidate ARM analysis, lint and strict docs pass. Built-in message routing, lifetime/transaction integration, library configuration, public registration/provenance and CI acceptance remain open. The current delivery and CI investigation are recorded above; earlier artifact-specific results remain retained.
3. Submitted guarded TEMP traversal protects allocation geometry/footer dependencies: candidate sanitizer fault tests, ARM analysis and strict docs pass. Its 59,656-byte image and launchers match both HP49g+/HP50g native cache round trips. Complete opaque/banked provenance, public library management, Port1 edits and Port2 trusted installation, authority lifecycle, startup and reclamation remain required. Recovery bounds and Port1 fault-injection evidence remain retained. The submitted public adapter now treats incomplete native publication as terminal: IRQ/FIQ are masked and ownership is retained without cleanup or Stock return. Ten publication cuts, two recoverable body faults and both exit regressions pass; ARM build/analysis and strict docs pass. Port1 replace/remove remain unavailable pending complete references. Port2 compromises affect sequencing, not M8 scope; sound compatibility is independent. Reproducible M8.0 details belong in tests, development docs and PR #24. The independent UI budget follow-up passes six exact workflow commands, archived-report selection and stale-report rejection. Submitted #21 `00d5ddf1` also retains existing watchdog diagnostics: prior Runtime job 121228920601 failed HP39g+ Stock system-services-immediate with 0x46417b8e; another same-head workflow passed. The fresh candidate retest passes with settled return; original cause remains unproven. Preceding-head workflows and jobs were inspected before push; new CI acceptance remains pending. This does not reduce Stock reservations or close M8.0/M8.2/M8.3.

Submitted Port0 commit-failure handling passes four operation write-cut matrices, root-before-write and allocation/preflight controls, plus four public API terminal-path cases. Delivery sanitizers, ARM analysis and strict docs pass. The 62,088-byte image, both launchers and verifier inputs match four HP49g+/HP50g registered/unregistered message cases; 476 source corruptions reject. Included in #24 `0e68b3b1`; see the current CI investigation above. Fresh delivery CI remains required; both parents are accepted.

## Pending delivery

| PR / head | Current acceptance state |
| --- | --- |
| #17 / `9e967d55` | Accepted and merged as `88aae022`: all 11 workflows pass, 379 successful checks and two skipped. Explicit opt-in legacy formatting and compact roadmap/archive delivered. |
| #18 / `439339b1` | HP49g+ Direct PWM IRQ failure `0x46417b03`; root cause unproven. |
| #20 / `41e35a36` | HP50g Direct system-services-exit PWM IRQ failure `0x46417b03`; root cause unproven. |
| #22 / `0c9db1cf` | Accepted and merged as `038b0f41`: all nine exact-head workflows pass, 354 successful jobs and two skipped. M0–M7 overview restored on main. |
| #23 / `7dadd8a0` | Accepted and merged as `f806b299`: all ten workflows pass, 373 successful jobs and one skipped. |
| #24 / `6ff8a1e9` | Accepted parents integrated. Growth reservation and packed HOME snapshot fixes pass 54 six-model lifecycle phases and ten native library cases locally. CI inspected: four workflows pass, one running and seven queued; not accepted. |

Earlier ID1701 recovery fixture retests pass on both models against the failed CI artifact: guarded/repeated/absent recovery and 16 rejection cases each. The measured free-space bounds remain valid. Earlier HP39 transfer stalls remain a separate acceptance gate. Submitted #24 includes native-page fence validation from `4c5546df`; later native-entry and continuation changes are test instrumentation. Native-entry observations are delivered in `c9dba0c2`, not CI-accepted.

## Scope and tracking

M0–M7 summaries are restored on main by accepted PR #22. Keep these summaries visible in the roadmap. All original milestone requirements remain in `docs/development/roadmap-history.md`. Keep current state here, reproducible details in tests/PRs, and history in the archive. Local passes, submitted CI, accepted delivery and milestone completion are distinct.

Before every push, inspect the preceding exact-head CI and investigate failures. Do not poll newly pushed CI in the same turn or merge unfinished gates. HP39gs/40gs ROM-write capability remains deferred research; no hardware flashing is authorized. Legacy formatting uses explicit `HPCLANG_HPGCC_LEGACY_FORMAT=1`.

## Resource observer main integration — local, 2026-10-29

PR20 head 1ce01859 passes all four workflows and 169 jobs, including publication.
GitHub reports conflicts with accepted main 3c484673 in roadmap and PWM IRQ test.
The local merge retains main's accepted IRQ/watchdog implementations unchanged
(both files byte-identical to main), which already include elapsed-time waits,
clock-error handling and failure preservation. Observer capture, independent
capacity verifier, UI lifecycle checks and workflow evidence remain present.
The roadmap keeps M0-M7 summaries and current compact status; branch-specific
historical evidence remains in prior commits, sdk-design and the history archive.
Six snapshot tests, seven verifier tests and five UI memory tests pass (18 total),
as do full tracked-file format/Ruff/ShellCheck and strict docs. Logs are retained
in `/work/oct29-resource-integration-quality` and `/work/oct29-resource-docs-check.log`.
The merged version requires fresh CI; neither parent CI nor this integration
completes M8.2/M8.3 or proves unsampled peak capacity.

## 2026-10-30 — UI native heap return regression

The optional `--ui-return` lifecycle-verifier mode is enabled for Stock UI CI.
It requires the HP39 fetch-complete or classic ARMToolbox baseline before PASS,
and checks each heap's usable bytes and largest free block after settled return
against that baseline. Reduced capacity or increased fragmentation fails even
when linked-list structure and the normal return status are valid.
Fifteen snapshot/verifier tests pass, including missing/late baselines, a valid
but depleted free list and equal total bytes split into smaller free blocks.
All 79 original SRAM captures across six models replay with unchanged existing
proof fields; each model restores both capacity measures exactly. Evidence:
`/work/oct30-ui-return-capacity.json`. This is saved-capture validation, not a
new live emulator run, total-memory accounting or a peak bound. M8.2/M8.3 remain
open. Ruff passes with its cache disabled for the read-only source mount.

Preceding CI audit: #26 `6a582958`, 157 successful/eighteen running/203 queued
jobs, no failures; #27 `b59fb56d`, ten queued workflows and sixteen materialized
queued jobs. Neither has completed CI acceptance.

UI return-capacity delivery: `8849b85f` submitted in draft PR #28 after the
preceding CI audit. New-head CI was not polled this submission turn.

### Application SP observation control (2026-10-30, local only)

The resource-observer worktree adds opt-in `HPCLANG_X50NG_STACK_POINTER`
instrumentation to translated application SP writes. Records separate CPSR modes;
native firmware instructions and exception bank switches are outside this scope.
Four host tests cover untouched reservations, mode separation, disabled output,
existing evidence preservation and invalid destinations. A linked-helper symbol
check ensures the QEMU archive was rebuilt, rather than merely relinking stale
objects. The initial op_helper stdio conflict was corrected by placing the helper
in helper.c; the first stale-archive build is not runtime evidence.
HP39g+ Direct completes full UI pixel/resource/return gates in three runs:
baseline, a temporary control reserving 4096 untouched bytes, and tracing disabled.
Independent checks retain identical memory reports (2424 touched stack bytes),
but SP excursions are 2424 and 4144 bytes. The reported deepest PC is verified
against the control ELF's `mov sp, r2` instruction. Disabled tracing creates no
trace. The control does not alter the shipped application source.
Evidence: `/work/oct30-stack-pointer/independent-verified.json`, retained traces,
runtime reports and control disassembly; drivers `research/oct30-stack-pointer-*.py`.
The follow-up is not submitted and does not establish six-model/Stock coverage,
complete stack bounds or whole-system peak memory admission.

The SP follow-up now has a maintained budget verifier and both Direct/Stock
UI workflow gates. CI artifacts retain raw JSONL traces and budget reports.
Eight observer/budget tests and five existing UI memory tests pass; Ruff,
ShellCheck and workflow structure checks pass. The gate rejects absent,
duplicate, truncated, invalid-mode, inconsistent-watermark and exhausted-stack
reports and removes stale success output. A fresh current-branch emulator
contains the linked helper; its UI HPC matches the earlier accepted local image.
All six Direct runs pass UI pixels, update timing, SP budget and return. Current
verifier replay checks all six stored cases and unchanged application/emulator
hashes. Each observes 2424 bytes of System-mode SP use. The original matrix
process continues into Stock; these six Direct cases are not a completed matrix.
Evidence: `/work/oct30-stack-pointer/matrix-independent-verified.json` and
`matrix-runtime.log`; runner `research/oct30-stack-pointer-matrix.py` and replay
`research/oct30-stack-pointer-matrix-audit.py`. Setup first rejected obsolete
firmware filenames before running any target; the corrected setup uses the
model profiles' `boot-49g+.bin` and `boot-50g.bin`. No new push or CI acceptance.

## 2026-10-30 — P=1 bank-exit recognition and Stock SP gate failure

Local, unsubmitted M8.0 recognition now covers the reviewed P=1 bank-exit
shape, including its replacement selector and consumed dispatch bytes.
P=0/P=1 unit regressions pass ASan/UBSan and ARM compilation/static
analysis. This does not restore an arbitrary prior bank, qualify startup
execution, or enable public storage mutation.

The maintained unit fixture follows the reviewed comparison order
(0,2,3,4,5,6,7,8,9,10,1), distinguishes entry/exit selectors, and
rejects each consumed unreadable nibble, callback stop, truncated exit,
body mismatch and absent matching test without changing output.
Host ASan/UBSan and ARM compile/static analysis pass locally; no push.
Artifacts: `/work/oct30-bank-exit/`; reproduction script:
`research/oct30-bank-exit-check.py`.

The Stock matrix stopped at HP39g+: execution/UI/return passed, but the
SP gate rejected the recorded User-mode application because it requires
System mode. Mode qualification and the remaining Stock cases are open.
The process is terminal, not waiting. The retained trace records modes
16 and 19; the gate requires 31. Six Direct passes remain valid; no
Stock SP acceptance is inferred. Runtime log and trace remain under
`/work/oct30-stack-pointer/`.

## 2026-10-30 — Explicit exit planning and shared-stack gate

Local, unsubmitted M8.0 planning now accepts explicit P=0/P=1 and retains
the existing P=0 entry point. Both-model original-interpreter comparisons
pass 32 P=1 page-map predictions and 96 unsupported-P rejection controls.
ASan/UBSan, ARM compile/analysis and current-source artifact checks pass.
This is mapping prediction, not arbitrary-bank restoration, startup
qualification or authorization for public storage mutation.

The maintained `alias_guards.py` runs 16 P=1 ROM entries per model
through the original ARM interpreter, checks return/P reset and compares
all 256 predicted page mappings. P=2/15/16 preserve plan output and emit
no dependencies. Existing P=0, overlay, alias and startup tests pass.
Reports and source hashes: `/work/oct30-bank-exit/plan-verified.json`.
PR #26 exact head `b8d38497` remains ten queued workflows/16 jobs.

SP use). The gate now combines the shared User/System SP bank, matching
bootstrap caller-mode preservation. Nine host tests and replay of all six
Direct cases plus HP39g+ Stock pass, including its nine lifecycle snapshots.
The remaining five Stock cases have resumed; their acceptance stays open.
The original failed gate/log is retained. Replayed artifacts retain
application/emulator hashes; no failed runtime was replaced.
Proof: `/work/oct30-stack-pointer/matrix-user-mode-verified.json`;
remaining Stock runner: `research/oct30-stack-mode-resume.py`.
Shared-bank transition and overflow regressions pass. Exception-stack
and whole-system peak coverage remain outside this local result.

Delivery: PR #26 now points to `af7d7e8b`, including functional
`84fa1a70`. Push and remote head readback succeeded. Pre-push `b8d38497`
had ten queued workflows/16 jobs. New-head CI was not polled; no merge
or M8.0 acceptance is claimed. Full tracked-file quality and strict
docs passed before push.

## 2026-10-30 — Read caller-qualified predicted mappings

`hpclang_rpl_stock_view_mapped` reads through a stable caller-owned
256-page array without changing native mappings. The live reader and
mapped reader share the same physical SRAM/immutable-ROM bounds.
Invalid pages, mutable Flash/MMIO, stale descriptors and invalid
arguments preserve the output. Caller retains plan dependencies,
exclusive lifetime and distinct mapping IDs; this is not a resolver
policy or complete reference guard.

Maintained unit tests exercise different live/predicted maps, every
logical page boundary, descriptor invalidation and unchanged live
page words. ASan/UBSan and ARM compilation/static analysis pass.
Static analysis uses the existing stock-view exclusion for intentional
fixed hardware address reads; no new checker exclusion was introduced.
Saved-state replay covers 17 maps per HP49g+/HP50g model: each has
15,360 successful nibble comparisons, 2,048 rejected reads and 1,024
page selections differing from the live map. Inputs remain unchanged.
Proof/source hashes: `/work/oct30-mapped-view/snapshots-verified.json`.
This is local validation, not a new device execution or submitted CI.

## 2026-10-30 — Device mapped-read integration in progress

The maintained ROM-bank probe now feeds each accepted P=0 plan and
16 P=1 exit plans into the mapped reader (32 plans per launch). It
checks both end bytes of every logical page against direct physical
SRAM/immutable-ROM reads, rejects unsupported pages, and checks the
complete 0xc60-byte native state remains unchanged. Successful return
requires all plan/read checks; this is still read-only prediction.
The fresh application and all six launchers build; eight ARM analyses
pass. Independent rebuild matches all 16 delivery artifacts; the
main frame remains 1064 bytes below the 2048-byte gate. Temporary
rebuild output was removed. Two-model 14-case Stock matrices run in
`/work/oct30-mapped-view/`; no runtime acceptance is claimed yet.

Current UI gate independently replays 8/12 cases: six Direct and
HP39g+/HP39gs Stock, including 17 lifecycle snapshots. HP40gs is live.
Proof: `/work/oct30-stack-pointer/matrix-user-mode-independent-verified.json`.
PR #26 af7d7e8b: eight queued/two pending workflows, 16 queued jobs.
PR #28 8849b85f: six successful/two queued workflows; 300 successful,
one skipped, five running, 49 queued jobs. Neither is fully accepted.

## 2026-10-30 — Clarify UI implementation versus remaining acceptance

Rechecked the frozen M8.2/M8.3 contracts: M8.2a/b/c and the original
sketch review are checked; M8.3 reusable controls, default renderer,
independent backend/theme and keyboard/surface adaptation are checked.
Current UI implementation files have no local changes against the
submitted observer branch. The current roadmap had misleadingly
listed reusable controls/theme independence as remaining work. It now
separates implemented functionality from the still-open smallest-target
memory/update-cost, whole-application resource/lifecycle, six-model
low-memory/return and follow-up CI acceptance. No criterion is removed
and neither UI milestone is marked complete.

M8.0 mapped-reader Stock matrices each passed native and
native-new-message cases and continue on the same live processes.
PR #27 exact head 5d5195dc has four successful/six queued workflows,
138 successful/230 queued jobs; no complete CI acceptance yet.

## 2026-10-30 — Complete local UI SP matrix

The UI SP follow-up is locally complete and prepared for PR #28.
All six Direct and six Stock UI/SP/return cases pass; independent replay
checks all 79 lifecycle snapshots, pool/artifact budgets and unchanged
application/emulator identities. Shared User/System stack use is
2424–2432 bytes of 8192. Nine host tests and full source-quality/strict
docs checks pass. The untouched 4 KiB control detects 4144 bytes while
the watermark stays 2424. CI, native/exception stacks and whole-system
peak admission remain open.

Complete proof: `/work/oct30-stack-pointer/matrix-user-mode-independent-verified.json`.
All Direct and HP39-family Stock cases observe 2424 bytes; HP48/49/50
Stock observe 2432 bytes. Original gate failure and trace are retained.
The replay originally tried to recreate an immutable lifecycle report;
it now recomputes and compares the retained report instead. Final
`--require-complete` replay passes 12/12 and 79 snapshots. No skipped
model, changed application or relaxed stack limit was used.
Pre-push PR #28 audit: exact 8849b85f, six successful/two queued
workflows, 309 successful/one skipped/six running/39 queued jobs.
M8.0 mapped-reader matrices have passed native, new-message,
missing-message, remove and detach cases on both models; they remain live.

UI delivery `79a5fe36` was pushed to PR #28 and the remote head verified.
The preceding exact-head CI was audited before push; new-head CI was
not polled. The nine observer/gate tests are included by the existing
Python unittest discovery. Full tracked-file format/Ruff/ShellCheck
and strict docs passed before push. No merge or UI milestone completion.

## 2026-10-30 — Complete mapped-read device matrix; C data-target composition

The mapped-reader application passes all 14 call/return cases on both
models. Independent `/work/oct30-mapped-view/verified.json` checks
28 post-call markers, 18 original/ten rewritten returns, 36 unchanged
Flash states and 16 independently reproduced artifacts. Every launch
requires 32 P=0/P=1 plan/read checks and unchanged native state.

Separately added `rpl-bank-target.c` joins the actual bank planner to
bounded object-span decoding and a reader for subsequent graph work.
It requires Stock source callbacks over the same captured view,
distinct mapping IDs and a nonzero-switch ACPTR; source-edge and
exclusive-lifetime qualification remain caller obligations. Plan and
payload reads retain mapping-qualified dependencies; output publishes
only after successful parsing. EXECUTION edges are rejected.

Initial real-state comparison resolved 26/40 targets; 14 were blocked
by immutable-only reads. A separate bounded mutable-Flash data reader
now checks identity descriptors on each read and emits no Flash
commands. Immutable-ROM identification stays unchanged. All 40 HOME
targets now match prior independent spans/banks and every payload
nibble matches the saved physical bytes, including 14 mutable-Flash
targets. Outside-object reads preserve output; all 40 execution edges
remain unsupported. Inputs are unchanged. Evidence/source hashes:
`/work/oct30-bank-target/targets-verified.json`.

Registered the new runtime sources and host test in CMake. Composition
unit tests use an isolated planner stub; the saved-state test uses
the real planner and identified ROM. ASan/UBSan, CTest and ARM compile/
analysis pass, including final Flash nibble bounds, callback errors,
stale descriptors, mapping collisions and unchanged failed outputs.
This newer resolver was not part of the completed device artifact;
its device validation, full graph policy and execution closure remain
open. These changes are local/unsubmitted and do not enable edits.

## 2026-10-30 — C data-target device integration started

The updated Stock probe walks actual HOME ACPTR tables, resolves their data
objects through the C planner/reader, reparses their bounded spans, checks the
first out-of-object read, and rejects execution edges. It requires mutable
Flash coverage and preserves the captured native state. The working image
passes ten ARM analyses; an independent temporary build reproduces all sixteen
delivery artifacts and is removed afterwards. Main stack frame: 1064 bytes.
Evidence: `/work/oct30-bank-target/reproduction.json`. HP49g+ and HP50g each
pass the first native call/return case; remaining cases are running, not accepted.
The independent audit now explicitly binds the new resolver, Flash data reader,
code decoder and profile to the reproduced sources. No public edits are enabled.

Exact submitted-head CI audit: UI #28 `79a5fe36` has twelve queued workflows
and twenty queued materialized jobs. Migration #27 `5d5195dc` has four successful,
five queued and one running workflow; 159 successful, 205 queued and four
running jobs. Neither branch is accepted or merged by this audit.

## 2026-10-30 — UI all-PC stack-region observation

Added an optional `HPCLANG_X50NG_STACK_REGIONS` observer and translator opt-in
in the UI follow-up worktree. Unlike the application-PC filter, it records
translated SP writes by ROM and launcher instructions. Fixed-size host storage
separates 32 CPU modes and 4096 address regions; output records extrema, their
PCs and write counts. Zero/end-of-address-space values and boundary crossings
remain visible. Independent opt-in, application-trace coexistence, native PCs,
mode separation, boundary crossings and output failures are covered by four
new host tests; all thirteen stack-observer tests pass, as do Ruff and strict
docs. Disabled instrumentation emits no helper calls unless the existing
application observer is enabled. Guest code and memory are unchanged.

A freshly prepared emulator passes HP39g+ Direct UI/LCD/return checks and the
existing application stack gate using the unchanged `gated-app` image. Nine
mode/region groups are retained, including stage-region SP extrema
`0x0131d3b0–0x0131dff8` and application SP use of 2424 bytes. Native boot and
runtime assignments are included; extrema must not be interpreted as a single
stack when the region contains multiple stacks or initialization phases.
Evidence and artifact hashes: `/work/oct30-stack-regions/direct-verified.json`.
This is local/unsubmitted. Stock runtime integration, implicit mode-bank changes,
non-translated writes, stack-reservation bounds and whole-system peak admission
remain open. No milestone is closed by this diagnostic.

## 2026-10-30 — Direct launcher stack bound reconciliation

Independent `direct-stage-budget.json` binds the raw region trace, tested image,
ELF, launcher layout, Direct runner and settled-return result. The requested
30-page pool places the stack top at `0x0131e000`; observed SP reaches
`0x0131d3b0`, using 3152 of 4096 bytes including eight saved-entry bytes.
The deepest PC resolves to `tinf_getbits` in the linked stage1 decompressor.
This validates the observed Direct workload only. HP39g+ Stock all-PC tracing
is running separately; portal/native/exception stacks and full peak admission
remain open. Evidence directory: `/work/oct30-stack-regions`.

## 2026-10-30 — Stock all-PC observation and stage reconciliation

HP39g+ Stock UI, LCD, application SP, pool and settled-return gates pass with
the optional all-PC observer and unchanged application image. Eight CPU-mode/
region groups include native firmware assignments; the application uses 2424
bytes. Independent reconciliation verifies both pool snapshots (34 stage pages
plus two native workspace pages) and the linked stage ELF: stage SP use is
3152/4096 bytes, deepest in `tinf_getbits`, matching the Direct workload.
Evidence: `/work/oct30-stack-regions/stock-verified.json` and
`stock-stage-budget.json`. These are observed-workload bounds, not complete
native/exception-stack or whole-system admission. Full tracked-source format,
Ruff and ShellCheck pass for both the UI and data-target worktrees; their
temporary normalized source trees are removed. No milestone is closed.
