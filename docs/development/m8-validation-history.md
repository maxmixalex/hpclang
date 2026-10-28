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

## Container duplicate-cache cleanup — 2026-10-29

While #26 CI was running, removed six inactive, byte-identical HP39 Aplet tool
cache copies from old oct24 source snapshots: 487.48 MiB of duplicate contents.
Retained `/work/oct25-watchdog-src/.cache/hp39-aplet`, verified all 1,416 files
by size/SHA-256 before removal and rechecked the retained copy afterward.
Source snapshots, ROM/state evidence, reports and final artifacts remain.
The file manifest and deletion results are retained at
`/work/oct29-duplicate-cache-cleanup.json` and in local `research/`.

## PR18 failed-job inventory correction — 2026-10-29

Exact head `439339b1` has three failed jobs, not only the previously recorded
HP49g+ Direct system-services failure (job `121201126584`). Logs and artifacts
for HP40gs Stock UI (`121233146923`, artifact `12613417322`) and HP39g+ Stock
notebook (`121229660722`, artifact `12611744808`) show completed directory
transactions followed by 90 seconds without acknowledged payload progress.
HP40gs stops at send window 46 with three NAKs/four retries; HP39g+ stops at
window 40 with three NAKs/five retries. Both fail at the host transfer gate
before application entry, not in UI or resource-budget assertions. These are
observed symptoms, not proof of the underlying transport cause. Raw logs and
artifacts are retained under local `research/oct29-pr18-*`; all three failures
remain open. No timeout change or rerun is counted as a fix.

The follow-up packet audit reconstructs both directional byte streams from the
retained socat logs, checks every logged chunk length, and validates Kermit
CRC-16 against a captured NAK before evaluating frames. It checks 2,194 HP40gs
and 1,345 HP39g+ data/ACK/NAK frames without a length, sequence or CRC failure.
The final HP40gs Timeout frame has sequence byte `0x0e` and type `F`; HP39g+ has
sequence byte `0x88` and type `E`. Both lengths and CRCs are valid despite the
invalid sequence fields. This is observation at the PTY bridge, not proof that
all bytes reach the firmware unchanged or that a firmware/timing cause is known.
Raw-stream hashes, parsed final frames and the independent CRC audit are in
local `research/oct29-pr18-packet-audit.{py,json}`. No runtime fix is claimed.

### 2026-10-29 — Explicit native-root address fields (local)

Mutation IO now accepts optional whole-word/low-20-bit native-root masks.
Ordinary shifts preserve unrelated bits; duplicate cells move once and
conflicting masks fail before writes. Catalog capture guards the complete word
and coalesces matching register repairs. Default full-word semantics remain.
The controlled alternate-return fixture supplies its proven R0 through the new
root interface while retaining its original expected-word transaction guard.
This is not automatic register provenance or installed-SDK execution coverage.

- Four ASan/UBSan suites pass: stage_stack, rpl_code, rpl_reference,
  rpl_mutation; 43.16 s total, mutation 41.24 s.
- New unit cases cover ordinary growth/contraction, equal values in separate
  cells, duplicate cell identity, invalid/conflicting masks, default high-bit
  values and catalog preservation/rejection.
- `/work/oct29-native-mask/matrix/matrix.json`: 304 native return cases and
  480 pre-write rejections across HP49g+/HP50g, including 48 continuous cases.
- ARM compilation/static analysis and full tracked-source format/Ruff/shell
  checks pass. Follow-up is local, not CI accepted.
- Previous #26 head `8c2bb27f`: 96 successful, ten running and 248 queued jobs;
  no failures observed. No new push during this turn.

### 2026-10-29 — Versioned launcher root-mask plumbing (local)

RPL4 adds the optional native-root mask pointer after the RPL3 ERAM fence.
Production Stock mutation IO forwards masks only for RPL4. Exact-sized RPL2
and RPL3 allocations under ASan prove that absent fields are not accessed.
Port1 capabilities/fences accept both RPL3 and RPL4. The current launcher
initializes the field to NULL but still publishes RPL3 for whole-word roots,
preserving compatibility with old embedded runtimes. No new registers are
classified as pointers; adding partial-word roots requires proven semantics
and a paired RPL4-aware runtime.

- Six affected sanitizer suites passed in 17.07 s: device, object_store, port,
  object, rpl_stock_memory and port_memory. Added public stack/Port checks then
  passed in 13.41 s. The public stack test uses the host nibble-backend adapter
  and verifies a 13-nibble shift with high bits preserved only for RPL4.
- ARM code-relocation application rebuilt/linked; six raw launcher packages
  compiled/linked in `/work/oct29-root-mask-launch/launchers`.
  This packaging check used the retained payload and is not execution evidence
  for a newly packaged SDK application or whole-Stock E2E.
- Full-source formatting, Ruff and shell checks passed. ARM analysis of port.c
  passed. rpl-stock-memory.c initially reported three existing fixed MMU-address
  accesses; with only core.FixedAddressDereference disabled for that hardware
  backend, remaining analysis passed. No unqualified analyzer-clean claim.
- #26 exact head 8c2bb27f: 105 successful, 12 running, 237 queued; no observed
  failures. Follow-up remains local; no new push this turn.

### 2026-10-29 — UI CI logs complete; fresh Stock integration underway

Exact #21 head 01e1d2cf has all 12 UI jobs successful across six models.
`research/oct29-ui-passed-job-audit.json` and its twelve per-job logs independently
confirm touched stack, heap, physical pool and required/reserved budget checks.
Newly verified job IDs: 122939054452 (HP40gs), 122939061833 (HP49g+),
122939062439 and 122938789507 (HP50g). HP50g reports stack 2432/8192,
heap 20076/22880 and pool required/reserved 122880/385024 bytes. The complete
head audit has 294 successful jobs, one intentional PR publisher skip and
push publisher 122976703588 queued. Branch not yet accepted; these sampled
resource checks do not establish whole-system transient peaks.

Fresh source at 6d786f55 was built into a new ARM application and six wrapped
launchers at `/work/oct29-native-launch-e2e/artifact`. The maintained
`call_roots.verify_calls` suite is running against both models with original
Stock states and actual USB transfer. Both first `native` cases passed;
remaining cases are live, not passed or failed. Resume the existing processes;
do not restart or overwrite their output because an observation wait expires.
Runner: `research/oct29-native-launch-e2e.py`; root output directory:
`/work/oct29-native-launch-e2e`. Final source/artifact hash manifests are written
only after all cases pass. Current source remains unchanged during execution.

#26 head 8c2bb27f audit: 120 successful, nine running, 225 queued jobs,
no observed failures. No new push this turn.

### 2026-10-29 — Masked-root deletion guards and delivery dependencies

Added source-aware root enumeration checks for default, explicit full-word and
low-20-bit masks: aliases enumerate once, conflicting descriptions produce no
callbacks or writes. Catalog contraction now has regression coverage for every
nibble of a live 16-nibble deleted cell plus both adjacent addresses. All deleted
addresses return BUSY without RAM/native-root changes; the following address
moves with its high bits intact. Sanitized mutation suite passes in 41.08 s.
Only the test source changed; running Stock integration sources are unchanged.
Both existing Stock processes have passed native, native-new-message and
native-missing-message; the remaining cases are still live, not accepted.

#21 is explicitly stacked on #20 (m8-resource-observer), not main. #20 exact
head 41e35a36 has 369 successful jobs, one failure, two skipped publishers and
two cancelled publishers across its runs. Job 121190803963 / run 40194091672
fails HP50g Direct/system-services-exit with 0x46417b03; HP49g+ passes. Its
artifact 12604497124 is retained in research/oct29-pr20-pwm. Exact-head
checks.c distinguishes PWM failure 3 from watchdog failures by adding 0x80
only for the latter. Thus the later watchdog waiting fix does not itself
resolve this failure. PWM failure 3 combines IRQ count/completion/error,
deadline, ownership and errno predicates. Saved JSON/images do not retain
those individual fields, so no single cause is established; targeted PWM
observations are required before treating it as a deadline-loop failure.

### 2026-10-29 — PWM IRQ failure observations (local #21 follow-up)

Commit 10e05bc0 on m8-ui-lifecycle-ci adds failure-only PWM snapshot tag 0x7b10
(22 words / 88 bytes), distinct from watchdog tag 0x7b00. It captures IRQ
count, completion/error, timing, alarm/tone ownership, errno and timer/interrupt
registers before foreground cleanup, then uses the existing memory-checkpoint
protocol. Existing CI uploads already retain these JSON records. Original
waiting behavior and failure predicates are unchanged; no historical fix claim.

Fresh system-services-exit builds pass Direct HP49g+ and HP50g execution and
settled return. A separate masked-IRQ build fails as expected with 0x46417b03
and emits the snapshot: hits=0, finished=0, error=0, alarm_owned=1,
tone_owned=1, errno=33, first_failure=3; last_time=5184259,
deadline=5284259, foreground_time=5376968 microseconds. This establishes
observation coverage for missing IRQ delivery, not the original CI root cause.
Artifacts, source snapshot, results and hashes are retained under
/work/oct29-pwm-diagnostic. After confirming processes had exited and no live
file/cwd references used the targets, removed temporary source and intermediate
build trees (313222274 logical bytes); retained ELF/HPC/launchers and evidence.
The follow-up is local/unpushed. #21 publisher 122976703588 remains queued.

Mainline Stock integration continues in the original two live processes;
each model has passed through native-cache-lookup (eight cases). No runtime
source was changed during that run. Remaining cases have not yet been accepted.

### 2026-10-29 — PWM diagnostics submitted in #21

Pushed m8-ui-lifecycle-ci / 10e05bc0064502ea0c44872eee31f1ae2a2b4afb and
updated #21's description to distinguish the new diagnostics from a bug fix.
Pre-push audit of 01e1d2cf rechecked all five exact-head workflows: four succeeded,
one awaited the push publisher; 294 jobs succeeded, one PR publisher was
intentionally skipped and one push publisher was queued. The parent remains
m8-resource-observer/#20. Verified the remote head after push, without querying
new CI. No merge or milestone acceptance. #26 audit separately reports
144 successful, 11 running and 199 queued jobs at 8c2bb27f, with no failures.
Fresh mainline Stock integration passed all nine library cases on both models;
GOSUB and RTNSXM cases also pass on both, while the three remaining
call-return cases per model continue in the original processes. A local draft of the mainline follow-up PR body is prepared, but
mainline code has not been pushed pending completion of that validation.

### 2026-10-29 — Fresh mainline Stock call/catalog integration complete

Both original processes completed successfully without restart: all 14 maintained
call_roots cases per model pass, 28 total. Independent post-run audit checks the
exact case sets, each original entry observation, all unchanged saved Flash
bytes, return flags and source/artifact manifest hashes. Eighteen library cases
retain original returns; ten call fixtures deliberately rewrite controlled
return code. Reports: /work/oct29-native-launch-e2e/{hp49gplus,hp50g}/result.json,
per-model inputs manifests and verified.json. Runner/auditor:
research/oct29-native-launch-e2e.py and oct29-native-launch-audit.py.
The compiled runtime/platform/include/code-relocation sources remain unchanged
from 6d786f55; later changes are unit regressions and tracking documentation.

This uses newly built ARM code and launchers through actual USB/Stock execution,
not the private host-mutation matrix. It covers default RPL3 launch roots;
explicit RPL4/low-field integration remains limited to host/controlled-fixture
checks. It does not establish arbitrary caller or exhausted-stack GC coverage,
public library management completion or hardware acceptance.

Before submitting the follow-up, audited #26 exact head 8c2bb27f: three workflows
successful, five queued; 167 successful jobs, 18 running and 169 queued, no
failures. Earlier incomplete CI is not accepted as passing. The planned push
includes guarded TEMP lookup, masked roots, versioned launch support and their
regressions, and requires new CI. No merge or milestone completion.

## 2026-10-29 — Suspended carry in launcher return discovery (local)

The launcher now supplies canonical SAT_CPU carry at offset 0x970 on supported
HP49g+/HP50g layouts; noncanonical values stay unknown. The existing entry point
retains unknown-carry behavior. Tests cover 144 combinations of stack cursor,
condition, incoming carry and conditional-return/forward/backward branch, plus
432 instruction-read faults, invalid carry, no stack mutation and no writes.
ASan/UBSan stage_stack passes (0.31 s); all six raw launchers build successfully.
This is bounded root discovery, not complete execution-reference coverage.
Original-interpreter integration for the changed launcher is pending; earlier
28-case Stock evidence applies to the previous source only.

Exact-head #26 fb261f17 CI observation: 12 workflows (six running, six queued),
13 successful jobs, seven running and 59 queued; no failures reported.
No CI acceptance or M8 milestone completion is claimed.

## 2026-10-29 — Original-interpreter incoming carry comparison

New maintained incoming_carry.py compiles the production stage-stack collector
and compares its root cells against original Saturn interpreter execution.
Both HP49g+ and HP50g pass 96 controlled cases each: eight stack cursors, two
carry values, two conditions, conditional return and forward/backward branches.
Exact dispatch trace, cursor and all eight post-return cells match; Flash stays
unchanged. Reports retain source, SRAM and Flash hashes under
/work/oct29-initial-carry/{hp49gplus,hp50g}/incoming-carry.json. Ruff passes.
This controlled test is not full launcher or public registration acceptance.
Fresh application and six launchers built at /work/oct29-carry-launch-e2e;
two-model full Stock call/catalog integration is running, not yet passed.
PR21 exact head 10e05bc0 has 17 successful, six running and 170 queued jobs
across seven unfinished workflows at observation; no failure reported.

## 2026-10-29 — Incoming-carry CI integration (local)

The Stock code-relocation job now prepares a dedicated Python virtual environment
with unicorn==2.1.4 and runs incoming_carry.py against its target snapshot.
Reports are included in the existing evidence upload. Other suites and Direct
jobs do not acquire this dependency. Workflow YAML/Stock-only placement and
ShellCheck pass. A fresh local venv reproduces all 192 two-model cases with only
the declared Python dependency. The workflow change is not yet submitted.
Full Stock integration sessions 85305 (HP49g+) and 76272 (HP50g) remain live;
both have reported native, native-new-message and native-missing-message passed,
not the complete suite.
Exact-head PR26 fb261f17 now has 21 successful, three running and 118 queued
jobs; 12 workflows remain unfinished. No failure or acceptance is reported.

Pre-delivery review corrected incoming_carry.py to the repository Ruff settings
(100 columns rather than the standalone formatter default). All tracked source
formatting, Python lint and shell checks pass after the correction. The 192
clean-venv interpreter cases were rerun successfully, refreshing driver hashes
in /work/oct29-initial-carry/ci-clean-{hp49gplus,hp50g}/incoming-carry.json.

## 2026-10-29 — PR21 captured PWM wait failures

Exact head 10e05bc0: jobs 123001306554 (HP40gs system-services) and
123001306568 (HP39gs system-services-immediate), run 40739395357, fail
with 0x46417b03. Downloaded artifacts 12839963078 and 12839622538 contain
the new 0x7b10 diagnostic: one IRQ, unfinished, no IRQ error, both resources
still owned, errno 33. Foreground time is deadline+96 us and deadline-1131 us
respectively. The fixed 20-million-iteration wait does not guarantee the
100 ms playback interval or interrupt delivery margin. Exact decoded data
and artifact hashes: research/oct29-pr21-pwm-failures.json.
A local irq.c change samples elapsed time between bounded busy batches; only
IRQ dispatch advances playback. All completion/ownership/errno assertions
remain. Six-model Direct and masked-IRQ negative checks are running in
session 50816 under /work/oct29-pwm-elapsed; no fix acceptance yet. This is
independent work during live M8.0 integration sessions 85305 and 76272.
PR26 fb261f17 has 24 successful/135 queued jobs, two passed/ten queued
workflows at observation; no failure reported.

## 2026-10-29 — Elapsed PWM wait fix submitted

PR21 969ade88 replaces the fixed instruction-count wait with masked-clock
sampling between busy batches, allowing one second past the playback deadline.
Only IRQ dispatch advances playback; all completion/ownership/errno checks
remain. Fresh six-model normal Direct and HP39gs/HP40gs immediate runs pass
with settled Stock return. Masked-IRQ HP39gs/HP50g controls still fail 7b03,
with zero IRQs, no completion, retained ownership/errno and observed time at
least one second beyond the deadline. Source formatting and artifact audit pass.
Evidence: /work/oct29-pwm-elapsed, including artifact-hashes.json; temporary
source/objects were removed after process checks (315892726 logical bytes).
Previous exact head 10e05bc0 was checked before push: 63 successful, nine
running, 244 queued and the two diagnosed PWM failures. New CI is unobserved.
This local fix does not retrospectively prove the cause of parent #20 or
PR18 historical failures, which lacked these captures. No branch acceptance
or M8 completion is claimed.

Current incoming-carry source reproduction: rebuilding with the retained HPC1
payload matches stage1.bin, memory.json and all six .bin/.hp launchers byte
for byte. /work/oct29-carry-launch-e2e/launcher-reproduction.json binds the
current stage-stack.c/.h and stage1.c hashes to those bytes; temporary rebuild
files were removed automatically. Full Stock sessions 85305 and 76272 remain
live, each having passed 11 of 14 cases through gosub-rtnsxm. No complete-suite
claim is made yet. PR26 exact-head audit: 30 successful, seven running and
219 queued jobs, no reported failures; no push this turn.

## 2026-10-29 — Incoming-carry batch local integration complete

Both Stock processes finished successfully: 14 cases/model, 28 total, including
18 original library returns and ten controlled rewritten call returns. Independent
audit checks exact case sets, pre-rewrite entry observations, all 36 saved Flash
images unchanged, input hashes and current-source byte-identical launcher
reproduction. Reports: /work/oct29-carry-launch-e2e/{verified.json,launcher-reproduction.json}.
The new interpreter CI gate also passes 192 cases against these fresh Stock
snapshots, in addition to the retained fixture and clean-venv checks.
Pre-push fb261f17 audit: 57 successful, 15 running, 316 queued jobs; three
passed and nine unfinished workflows, no reported failures. These are not
accepted gates. Bounded root discovery and controlled internal library tests
do not establish full opaque/banked coverage, public management or M8.0 completion.

## 2026-10-29 — PC exchange semantics (local follow-up)

APCEX/CPCEX inspection returns a jump target from the old low 20 register bits
and a NEXT_PC capture assignment for instruction+4. The existing origin engine
propagates the caller-proven capture tokens. It neither qualifies incoming
register pointers nor authorizes relocation. Next-PC wrap and failures preserve
output. Host tests exhaust all 65,536 four-nibble encodings, validate read faults,
truncation, address boundaries and preservation of unrelated origin cells.
ASan/UBSan rpl_code passes (1.88 s); current-source ARM static analysis passes.
Maintained pc_exchange.py compares 96 cases/model with original execution: three
addresses, A/C, all P values, both carries and all stack cursors represented.
Both models pass, preserving upper register bits, flags and machine-stack state.
Reports: /work/oct29-pc-exchange/{hp49gplus,hp50g}/pc-exchange.json. The next
Stock CI gate includes this test in its existing isolated environment. Formatting,
Ruff, workflow YAML and ShellCheck pass. This follow-up is local, not submitted.
Observed CI: #26 f66b3b97 has 14 queued/two running jobs; #21 969ade88 has
eight queued/three running; no failures reported and no completed acceptance.

PC-exchange movement follow-up: pc_exchange_move.py invokes the production
exchange descriptor and catalog-grow register-repair transaction, then resumes
original interpreter execution. Both A/C and movement before/after the exchange
pass on both models (eight positive paths). Omitting the selected register or
suspended-PC repair produces an exact stale dispatch address in all 16 negative
controls; no timeout is used as the negative oracle. Positive paths preserve
all unrelated CPU fields, upper register bits, flags and return-stack state.
Flash is unchanged. Reports and source/input hashes are under
/work/oct29-pc-exchange-move/{hp49gplus,hp50g}/pc-exchange-move.json.
The pinned caller snapshot supplies provenance; code bytes are patched inside
known caller locations. This does not establish automatic caller discovery,
complete opaque-root coverage, public catalog registration or Stock return.
Ruff/format checks pass; reproduction is documented in sdk-design.md.
PR26 f66b3b97 audit: ten successful/eight running/16 queued jobs; all 12
workflows unfinished, no failures reported. This follow-up remains local.

PC-exchange commit dependencies: both four-nibble instructions now enter the
production guarded catalog transaction. All changed/invalid nibble injections
reject with zero RAM/register writes (64/model, 128 total), while all eight
positive and 16 stale-address paths remain verified. Reports:
/work/oct29-pc-exchange-guarded/{hp49gplus,hp50g}/pc-exchange-move.json.
Fresh ARM application/six-launcher build and full tracked-source format/Ruff/
ShellCheck pass. The new binaries differ from the previous Stock-tested batch,
so fresh full Stock integration is running at /work/oct29-pc-exchange using
app/ artifacts; sessions 68139 (HP49g+) and 74211 (HP50g). The initial wrapper
path typo failed before execution and was corrected before these live runs.
PR26 f66b3b97 observation: 35 successful/eight running/232 queued jobs,
12 unfinished workflows, no failures reported. No follow-up push or acceptance.

The complete PC-exchange application was independently rebuilt in a disposable
directory: HPC1, result address, stage1, launcher memory metadata and all six
raw/wrapped launchers reproduce byte for byte (16 artifacts). Tracked-source
hashes remained stable throughout rebuilding; temporary build files were removed.
ELF debug paths are not compared across build directories. Evidence:
`/work/oct29-pc-exchange/reproduction.json` and `reproduction.log`.
This proves artifact reproduction, not completed Stock integration: both live
matrices have passed their first four native cases and continue on the original
sessions. No follow-up push or milestone acceptance is claimed.

Both PC-exchange Stock matrices completed successfully: 28/28 cases, 18 original
library returns and ten deliberately rewritten call returns. The independent
audit validates exact case sets, pre-rewrite entry observations, all 36 saved
Flash images, current input hashes and the independently reproduced artifacts.
Evidence: `/work/oct29-pc-exchange/verified.json`, `reproduction.json` and each
model's `result.json`/input manifest. Sessions 68139 and 74211 are terminal success.
This completes local integration for this batch, not automatic register-root
discovery, complete execution safety, public management or M8.0 acceptance.
Pre-push f66b3b97 audit: four workflows pass/eight unfinished; 85 successful,
ten running, 197 queued jobs and one intentional skip, no failures reported.
The follow-up is submitted without observing its new CI in the push turn.

## 2026-10-29 — Bounded launcher register roots (local, not submitted)

The shared launcher walk now follows PC=A/C, A/C=PC and APCEX/CPCEX along with
the earlier return/call paths. Only consumed initial register values become
low-20-bit roots; captures kill old origins. The launcher conditionally publishes
RPL4 and retains prior descriptors for all-word roots. The old collector API
retains its existing behavior. Discovery remains bounded and incomplete.

The Port1 mutation entry incorrectly rejected RPL4 despite compatible fence
support elsewhere. A regression fails on the preceding source and passes after
accepting RPL3/RPL4: 48 model/version/fence combinations, with nibble-preserving
I/O and unchanged outputs on rejection. Five maintained ASan/UBSan suites pass
(1.85 s); register paths include 240 use/overwrite cases, 24 read faults and four
capture-wrap rejections. Maintained register_roots.py agrees with ROM execution
in 576 cases. pc_exchange_move.py now requires automatic selected-root/mask
discovery; eight positive, 16 stale-address and 128 guard rejection cases pass.
Evidence: /work/oct29-stage-register/current and /movement, plus unit CTest logs.

Full format/Ruff/ShellCheck and ARM static analysis pass. Fixed-address analysis
is excluded only for the stock hardware mapping backend. Initial launcher link
exposed an unavailable __aeabi_memclr4 from partial array initialization; explicit
initialization of the two fixed masks fixes it. The application and all six
raw/wrapped launchers now build. Fresh Stock integration uses
/work/oct29-stage-register/app: sessions 38127 (HP49g+) and 51605 (HP50g).
These are running, not accepted; the preceding PC-exchange 28-case audit does
not validate this changed launcher. No new-head CI is polled in this push turn.

Linked-ARM collector validation: register_roots.py now optionally executes the
actual hpclang_stage_execution_roots symbol from the built stage1 ELF. A bounded
read callback supplies controlled instruction bytes; the test checks returned
cell identities/masks, untouched output slots, stack balance and unchanged
caller inputs. All 576 two-model cases agree with the host collector and ROM
execution. Reports include ELF and adapter hashes under
/work/oct29-stage-register/arm/{hp49gplus,hp50g}/register-roots.json.
This isolates the linked collector; full stage1/MMU publication and Stock return
are not inferred from it. The pending CI change retains the code-relocation ELF
and runs the ARM comparison in the existing isolated environment. Ruff/format
and workflow YAML checks pass. PR26 1dc590b1 currently has ten queued workflows
and 16 queued jobs, no completed results. Live full Stock runs continue unchanged.

The register-root application and six launchers independently reproduce all 16
compared artifacts byte for byte, including the HPC1 payload, result address,
stage1, memory metadata and raw/wrapped launchers. Temporary rebuilding storage
was removed. Evidence: /work/oct29-stage-register/reproduction.json and its log;
ELF debug-path differences are excluded, as for the preceding batch.
Host ASan/UBSan and linked-ARM tests now cover all ten roots (A/C plus eight
machine-stack cells) for every cursor, preserving masks/order/input and the output
boundary. The host suite passes in 0.02 s; the maintained ROM/ARM gate still
passes 288 cases/model and records eight additional mixed-capacity paths/model
at /work/oct29-stage-register/arm-capacity. Ruff/format checks pass.
PR21 969ade88 observation: four workflows pass/three unfinished, 211 successful,
16 running and 91 queued jobs, no failures reported. Both full Stock matrices
remain live on their original sessions, each past seven cases; no new push or
acceptance. The register-root audit is prepared for their eventual completion.

## 2026-10-29 — Clear simulated return-stack cells after consumption

The first register-root build completed all 28 Stock cases, with 18 original
returns, 36 unchanged Flash snapshots and reproduced artifacts independently
audited at /work/oct29-stage-register/verified.json. Sessions 38127/51605 are
terminal success. That bounded suite missed an exhausted-stack classification
defect, so it does not establish the corrected build's acceptance.

A nine-pop path reproduces the defect on both models/all eight cursors: ROM
clears the consumed cell and dispatches zero; the collector reused its old
conditional branch with changed carry and falsely reported A as a ninth root.
Evidence: /work/oct29-stage-register/underflow-before.json (16 cases). A host
regression fails against the preceding source and passes when private pops clear
their slot. Suspended input is never changed. Existing stack/register sanitizer
suites pass (0.32 s). Maintained return_underflow.py verifies all 16 paths against
ROM and the corrected linked ARM collector, without the false A root.

The corrected build also passes 576 host/ROM/linked-ARM register cases, full
mixed-root capacity, eight positive/16 stale-address catalog cases and 128 guard
rejections. Full format/Ruff/ShellCheck, workflow YAML and ARM analysis pass;
the hardware backend retains its documented fixed-address checker exclusion.
Application/six-launcher builds pass; independent rebuilding reproduces all 16
artifacts and removes temporary storage. Evidence: /work/oct29-stage-underflow
(model reports, movement reports and reproduction.json). New full Stock runs
use its app/ artifacts: sessions 33856 (HP49g+) and 94548 (HP50g), still running.
PR26 1dc590b1 observation: ten unfinished workflows; seven successful, five
running and 44 queued jobs, no failures. This fix remains local, not submitted.

## 2026-10-29 — Port1 public-edit dependency review

Source review confirms internal rpl-port-mutation.c replacement/removal exists,
but port-write.c still rejects public Port1 edits. The required port1_guard
must prove no live banked reference targets the changed suffix; the unit fixture
only supplies a synthetic verdict. Production banked-reference qualification
remains an implementation dependency, not an accepted gate. Once commit begins,
backend failures are INCOMPLETE and retain ownership; there is no rollback.
Current Stock sessions 33856/94548 remain live; no restart or acceptance claim.
PR21 969ade88 has four successful workflows and three unfinished, with 254
successful, 13 running and 51 queued jobs; no failures at this observation.

## 2026-10-29 — Native bank switch mappings and geometry

Maintained bank_switch_maps.py compares bank_lookup/rom_bank_layout with actual
ROM interpreter execution for all 17 switches on each HP49g+/HP50g fixture.
All 34 cases pass. Populated banks match every read-map cell, ERAM selects
physical 0x08020000, and mappings outside the window plus Flash stay unchanged.
Unused indices 14/15 still resolve to switch routines but install one repeated
dummy page; geometry correctly returns absent and preserves output. The first
bank's 96 KiB bound remains required despite its 128 KiB mapped window.
Reports: /work/oct29-bank-switch-maps/{hp49gplus,hp50g}/bank-switch-maps.json,
with source and input hashes. Ruff/format, workflow YAML and diff checks pass.
The test is wired into the existing code-relocation CI/evidence path locally;
it is not submitted or CI-accepted. This proves mapping behavior on these
fixtures, not firmware discovery, complete banked roots or public Port1 edits.
Full Stock sessions 33856/94548 remain live, each through seven successful cases.

## 2026-10-29 — PR26 HP39g+ pre-entry transfer failure

Exact-head 1dc590b1 audit finds job 123048995027 in run 40751033469 failed:
Stock object-mutation/hp39gplus. Directory transfer completed; payload stalled
at send-window 46 with repeated NAK/resends, followed by device Timeout and the
90-second no-progress watchdog. Stock JSON has object_store_phase=0, no pass or
return. This is pre-entry transfer failure, not evidence of object-mutation
execution failure. Source cause remains unproven. All 949 audited data/ACK/NAK
frames have valid lengths, sequence fields and CRCs at the socat boundary. The
device's final Timeout frame has a high-bit sequence byte but valid CRC.
Logs, RAM and application artifacts are retained under
research/oct29-pr26-object-mutation-failure; packet audit is
research/oct29-pr26-packet-audit.json. Exact application/assets/emulator from the
failed run are being replayed in /work/oct29-pr26-object-ci (session 14864).
Shell and sequence CRLF setup failures occurred before a scheduled test and
were corrected separately; their logs remain, not counted as reproductions.
Application/assets/emulator hashes are checked. Other observation counts:
62 successful, 15 running and 197 queued jobs; two workflows successful and
eight unfinished. No new push or CI acceptance is claimed.

Delivery checks after d84f3062 pass full tracked-file formatting, Ruff and
ShellCheck (session 24666 terminal), plus strict Zensical build. Temporary
normalized sources and generated site are removed; logs retained at
/work/oct28-pr25-format-fix and /work/oct29-current-docs-check.log.
Stock sessions 33856/94548 have each completed eleven cases and remain live;
exact CI-artifact HP39g+ reproduction session 14864 also remains live.

## 2026-10-29 — Corrected register-root integration and prior-CI replay

Sessions 33856/94548 are terminal success. Independent
oct29-stage-underflow-stock-audit.py verifies all 28 cases, 18 original library
returns, 36 unchanged Flash states, stable source/artifact hashes and agreement
with the independently reproduced 16 artifacts. Report:
/work/oct29-stage-underflow/verified.json. These full Stock paths exercise RPL3;
explicit masked RPL4 remains covered by controlled fixtures, not arbitrary callers.

Session 14864 is terminal success: exact failed-CI HP39g+ payload, transfer assets
and emulator complete all lifecycle runs and settled return. Independent audit
verifies input hashes and unchanged 2 MiB Flash at
/work/oct29-pr26-object-ci/verified.json. The historical pre-entry transfer
failure is not reproduced; its cause remains unproven, not fixed by this pass.
Pre-push 1dc590b1 audit: 79 successful, 13 running, 182 queued and one failed job;
two successful workflows/eight unfinished. PR21: 277 successful, three running,
39 queued jobs; five workflows pass/two unfinished. No branch acceptance.

Register-root follow-up submitted as bc6596e6ecc0a185b24c29b8eb644314c5693505; new-head CI not polled in the push turn. Prior-head transfer failure remains unresolved; no merge or milestone acceptance.

## 2026-10-29 — Submitted CI observation and finished-build cleanup

First post-push bc6596e6 audit finds 12 unfinished workflows (five queued,
two pending, five running); jobs: two successful, ten running, ten queued,
no failures observed. Prior-head Kermit failure remains unexplained. Corrected
the PR21 summary to two unfinished workflows, matching its five successful
workflows and recorded 277 successful/three running/39 queued jobs.

After confirming no process uses either finished register/underflow build,
removed only /work/oct29-stage-register/app/cmake and
/work/oct29-stage-underflow/app/cmake. Freed 5,159,204 logical bytes; all 2,051
remaining files retain their hashes, including tested artifacts, saved states,
reproduction reports and audits. Report: /work/oct29-cmake-cleanup.json.
No source or hardware data changed; CMake directories can be regenerated.

## 2026-10-29 — Reconcile already-accepted tone compatibility gates

M8.4 status audit finds hpgcc-compat.md still described async/KOS tone gates as
local/unsubmitted despite accepted PR25 head 752b8999. Both Runtime runs
40709124150/40709112604 contain all 27 required jobs each: normal/exit/_Exit
across three Direct families and six Stock models. All 54 pass. Reviewed
source proves async/KOS checks precede PASS and exit cleanup is observed before
launcher restoration. All 36 selected runtime/header/test blobs match accepted
main and current M8.0 branch. Six original HP39-family Direct/HP40gs Stock logs
also confirm actual target execution. Audit report:
research/oct29-sound-acceptance-audit.json; retained logs:
research/oct29-sound-accepted-logs. This records existing acceptance, not new
runtime behavior. It covers all six models including 39/40 tone/beep playback,
not PCM/WAV decoding or physical audibility. M8.4 as a whole stays open and
M8.0 remains the mainline. Historical flaky/failed jobs are not declared fixed.
PR26 bc6596e6 observation: 12 unfinished workflows; 26 successful, eleven
running and 169 queued jobs, no failures. No push/merge this turn.

## Guarded switch-table lookup — 2026-10-29, local only

`hpclang_rpl_bank_lookup_guarded` reports valid read dependencies, including
nonmatching entries, without publishing a binding after any error. Host tests
inject callback failure, read failure and invalid nibble at each of 85 reads;
unknown/ambiguous switches, missing callback and positive callback error reject
without changing the output. ASan/UBSan `rpl_port` and `rpl_reference` pass.
ARMv4T Thumb compilation and Clang static analysis pass for `rpl-port.c`.

The maintained `bank_switch_maps.py` joins this lookup to the actual C reference
guard builder and validator. Both original-ROM fixtures pass 17 switches each:
85 observed nibbles form 17 guards; unchanged tables validate, all 2,890 single
nibble changes reject, and all 34 capacity-shortage cases invalidate the builder
without publishing a binding. Original ROM read mappings, populated geometry,
unused-bank behavior and unchanged Flash checks still pass. Reports and source
hashes: `/work/oct29-guarded-bank-maps/{hp49gplus,hp50g}/bank-switch-maps.json`.
This does not prove switch-code identity, complete live-reference coverage or
authorize Port1 mutation. The implementation is local, not CI accepted.

PR #26 exact head `bc6596e6` observed four successful workflows and eight
unfinished, with 117 successful / 18 running / 253 queued jobs and no failures.
The prior-head pre-entry Kermit failure remains unexplained. No new push.

## Guarded ACPTR dependency composition — 2026-10-29

The guarded resolver records all 15 source nibbles and 85 ROM switch-table
nibbles through separate callback/context pairs. Output is published only after
both streams succeed. Unit coverage includes 51 bank/offset combinations,
200 positive/negative callback errors and 200 failed/invalid nibble reads across
all 100 positions. ASan/UBSan port/reference units and ARMv4T Thumb compilation
and static analysis pass. No target contents or switch code are authenticated.

Both-model maintained bank-map tests retain all prior ROM mapping checks and
combine constructed ACPTRs with the real ROM tables. Source and ROM deliberately
share numeric addresses under distinct mappings. Each of 34 resolutions builds
20 guards covering 100 nibbles; all 3,400 individual changes reject validation.
Reports with source hashes: `/work/oct29-guarded-acptr/{hp49gplus,hp50g}/bank-switch-maps.json`.
This proves dependency composition for controlled objects, not discovery of
all live Stock pointers or production Port1 mutation safety.

Pre-delivery PR #26 head `bc6596e6`: four workflows pass, eight unfinished;
128 jobs successful, 19 running, 241 queued, no failures observed. The earlier
pre-entry transfer failure remains unexplained. No CI acceptance is claimed.

## Guarded ordinary FlashPtr tables — 2026-10-29, local only

Configuration matching now exposes all 85 consumed nibbles (selected logical
configuration plus all 16 physical configurations). Ordinary header lookup
exposes 14 nibbles (identity, count and selected target). Callback failures,
including positive errors, preserve output. Units inject negative/positive
callback errors, unavailable reads and invalid nibbles at all 99 positions:
396 rejection cases pass with unchanged output and exact dependency coverage.

The C guard-builder integration combines both streams into 20 guards under
separate mapping identities. All 99 changed dependencies reject validation;
221 unobserved cells do not affect it. Capacity 19 rejects the lookup and
invalidates the builder without publishing a target. ASan/UBSan port/reference
units and ARMv4T compilation/static analysis pass. The initial analyzer warning
about temporary outputs was addressed using invalid sentinels plus explicit
success-range checks. No complete Stock pointer graph or mutation safety is
claimed; source FlashPtr, special tables and geometry remain separate obligations.

PR #26 head `abf9135c` observed ten unfinished workflows: six queued, two pending,
two running; five running and seven queued jobs, none completed and no failures.
This is not acceptance. No push was made in this turn.
