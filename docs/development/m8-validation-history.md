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

## Source and special-table FlashPtr guards — 2026-10-29, local only

Guarded decoding records the 12 source nibbles without conflating decoding with
selector resolution; selector 0xfff and index 0xffff remain representable.
Guarded special-table lookup records the count and selected target after the
caller separately proves table location/format. Both preserve output on failure.
The two new paths add 88 per-position callback/read/invalid-nibble rejections,
plus absent-index, unsupported-prologue and missing-callback controls. Together
with ordinary/configuration paths, 484 per-position fault cases pass.

The maintained C integration test now composes source, configuration and either
ordinary or special target lookup under three mapping identities. It validates
111/107 observed nibbles using 23/22 guards, rejects all 218 changed dependencies,
accepts changes to 742 unobserved cells, and rejects both insufficient-capacity
cases. ASan/UBSan port/reference units, ARMv4T compilation/static analysis, full
tracked-file formatting/Ruff/ShellCheck pass. This is controlled parser/guard
composition, not proven production mappings or complete live-reference coverage.

PR #26 `abf9135c`: ten unfinished workflows, 16 successful / 14 running /
15 queued jobs at observation, no failures. No new #26 push this turn.

PR #21 `969ade88` completed all seven workflows: 319 successful jobs and one
conditional PR evidence-publisher skip. GitHub reported a clean mergeable head;
after marking the accepted draft ready, it merged into its parent #20 as
`1ce018599b622d6762cf02e4f5aa9c62873b188f`. Parent #20 now has that head and still
targets main. New parent CI was not polled in the merge turn. This accepts the
child delivery only; parent acceptance and M8.2/M8.3 completion remain open.

## FlashPtr guards against saved-state ROM tables — 2026-10-29

The maintained bank-switch CI test now invokes `flashptr_guards.py`. Each model
resolves seven logical configurations to physical banks; these bank mappings
are checked independently by original-ROM switch execution in the same run.
The helper visits all six ordinary tables and selector 15's special table,
within their geometry bounds. Each model has 3,976 entries, including one zero
target that must return absent without changing the output. Constructed source
FlashPtrs are decoded and their selector/index passed through the actual C
configuration and target resolvers. Every lookup's exact dependency set and
unchanged validation are checked. First/middle/last-entry mutations reject
2,319 dependencies per model, 4,638 in total. All prior bank/ACPTR checks remain.

Reports, input/source hashes and physical mappings are retained under
`/work/oct29-flashptr-rom-guards/{hp49gplus,hp50g}/bank-switch-maps.json`.
Callback exceptions are captured and fail the test, not silently accepted as
negative controls. Sources include the new helper. This proves fixture-specific
table integration; it does not authenticate arbitrary ROMs or discover every
live FlashPtr, and it does not authorize production Port1 mutation.

Pre-delivery #26 `abf9135c`: one successful/nine unfinished workflows;
40 successful, 18 running, 296 queued jobs, no failures observed. Parent #20
`1ce01859` has four workflows/six jobs queued. Neither branch is accepted.

## Bounded ROM-bank reader — 2026-10-29, local only

The Stock-memory adapter now captures separately proven ROM geometry into a
caller-owned native-coordinate reader. It validates both Flash extent and all
touched identity sections, including a bank crossing the 1 MiB boundary. Reads
are bounded by the declared bank size, not by the 128 KiB Saturn window. The
Stock wrapper selects current launcher identity mappings on HP49g+/HP50g; it
does not switch banks or write Flash. Failed captures preserve both outputs.

ASan/UBSan Stock-memory tests cover every nibble of 96 KiB/128 KiB, cross-section
and end-of-Flash examples, invalid dimensions/permissions/identity, boundary
reads, unchanged outputs and unchanged source bytes. Both-model bank-map and
FlashPtr integration now use this C capture reader for all 7,952 ROM table entries,
with 4,638 stale dependencies rejected and every read compared with independent
ROM indexing. The host explicitly supplies section descriptors; it does not
validate the live machine's L1 table. Reports retain source/input hashes under
`/work/oct29-rom-bank-reader/{hp49gplus,hp50g}/bank-switch-maps.json`.

ARMv4T compilation/static analysis pass; the hardware adapter's fixed-address
checker is excluded, as in its preceding analysis. Firmware identity, live mapping
capture, complete references and production Port1 mutation remain open.
PR #26 `d7db49e8` has ten unfinished workflows, four running/five queued jobs,
none completed and no failures at observation. No push this turn.

## Stock ROM-bank witness — 2026-10-29, validation running

The code-relocation application now gates every native call with `rom-banks.c`:
reviewed fixture geometry, all 14 populated banks and two unused entries, actual
launcher L1 identity sections, 3,520 page-edge nibble comparisons, lower/upper
read rejection and unchanged 64-entry Saturn bank-window mapping. The 96 KiB
first bank keeps its declared bound. No ROM writes or Saturn bank switch occur.

Fresh application/six-launcher build passes (64,184-byte payload). ARM analysis
of witness/Stock adapter passes with fixed hardware-address checker excluded.
Independent temporary rebuild reproduces all 16 delivery artifacts exactly;
the temporary build was removed. Full formatting/Ruff/ShellCheck pass.

Both models have passed the first original `native` call/return. Independent
audit checks source/artifact hashes, pre-rewrite entry observations, the original
post-call marker and unchanged full Flash images. Evidence:
`/work/oct29-rom-bank-stock/native-entry-verified.json`, `reproduction.json`,
and per-model input manifests. The complete 14 cases per model remain running;
these two cases are not acceptance of the full 28-case matrix. The emulator
runner sessions were confirmed live (HP49g+ 41099, HP50g 55869). Do not restart
from missing final result files alone. No new push or milestone acceptance.

PR #26 `d7db49e8`: one successful/nine unfinished workflows, 22 successful,
five running and 110 queued jobs at observation, no failures.

## Bank-switch calling-context probe — 2026-10-29

While both Stock matrices remain live, each model has passed native,
native-new-message, native-missing-message, native-remove, native-detach and
native-reattach: 12 cases observed passing, not full 28-case acceptance. Tested
runtime/application sources and artifacts remain unchanged.

Separate original-ROM traces identify the Flash entries' common path at Saturn
0x22fec, whose ARM interpreter compares P before selecting its branch. Controlled
17-entry x 16-P x 2-carry probes run on both saved contexts (1,088 combinations).
All 68 P=0 combinations return with the previously verified mappings. Every
P=1 combination returns with the same physical bank-2 mapping (first page 0x80000,
last page 0x9f800), regardless of selected entry, and leaves P=0. Other altered
contexts include 120 CPU exceptions, four unmapped reads and 100 instruction-budget
exhaustions. The two incoming carry values did not change the observed outcomes.
These are deliberate context variations, not failures of valid Stock execution
or proof of all handler semantics. They rule out authorizing a target mapping
from a switch-table address alone; the supported calling contract is required.

Evidence: `/work/oct29-bank-switch-trace.json` and
`/work/oct29-bank-switch-p-matrix.json`, with both ROM hashes and mapping hashes.
This corrects the scope of earlier mapping evidence to P=0. It does not add a
requirement to support arbitrary firmware: production qualification concerns
supported firmware and calling contexts. No runtime source change or push.

CI: #26 `d7db49e8` has 54 successful/eleven running/210 queued jobs, one successful
and nine unfinished workflows. #20 `1ce01859` has 63 successful/nine running/
96 queued jobs, one successful and three unfinished workflows. No failures
observed at these snapshots; no branch acceptance.

The maintained `bank_switch_maps.py` now rejects saved contexts with nonzero P
and records incoming P in its report. Both models pass all 17 reviewed switches
and their existing FlashPtr dependency checks; P=1 and P=15 reject without a
success report on both models. Evidence: `/work/oct29-bank-context-gate/`, including
`context-gate-verified.json`; original fixtures remain unchanged. This is a test
precondition, not a production firmware qualification mechanism. The separate
full Stock matrix has reached 22/28 passing cases and remains live; no push.

## Live ROM-bank reader Stock acceptance — local, 2026-10-29

Both original processes finish all 14 cases each. Independent final audit reads
all 28 saved post-call markers, distinguishes 18 original returns from ten
rewritten-return fixtures, and verifies all 36 Flash snapshots unchanged. Current
runtime/witness/build inputs match the test manifests and independent rebuild;
all 16 packaged artifacts still match byte-for-byte. Report:
`/work/oct29-rom-bank-stock/verified.json`. ARM analysis, source quality and strict
docs passed separately. This verifies fixture-specific live mappings and bounded
reads, not complete banked-reference coverage, firmware qualification or M8.0.

Submitted as `f9f50972` in #26 with the bounded reader commit `4b6f328e`.
Prior-head `d7db49e8` CI audit found five successful/five unfinished workflows
and 131 successful/17 running/223 queued jobs, no failures observed.
New-head CI was not polled in the push turn; no merge or milestone acceptance.

## Bank overlay calling-context regression — local, 2026-10-29

510 controlled initial/selected bank pairs all return, but invalidate a generic
"P=1 restores the previous mapping" assumption. The maintained bank-map suite now
checks 16 ERAM-entry/Flash-selection/ERAM-exit/Flash-exit sequences per model,
128 original-ROM calls total. ERAM hides Flash selection until ERAM exit; Flash
exit selects bank 2. Unused banks 14/15 can expose physical 0..0x1ffff after ERAM
exit, despite remaining invalid geometry. All 64 pages are compared at every
step, other Read[] windows and Flash remain unchanged. Reports and source hashes:
`/work/oct29-bank-overlay-maintained/{hp49gplus,hp50g}/bank-switch-maps.json`.
Existing 34 isolated-switch and FlashPtr dependency tests also pass. This is
fixture-specific handler evidence, not public Port1 edit authorization. The
public replace/remove path still awaits complete banked-reference qualification.
CI remains incomplete for #26 `f9f50972` and #20 `1ce01859`; no merge or new push.

Original-ROM access tracing now identifies the selected bank (+0xbd0), ERAM
flag (+0xb9c bit 8) and per-page read ownership (+0x848..+0x887). Maintained
overlay sequences check all three alongside all mapping cells. 128 independent
owner-byte controls (64/model) retain precisely the marked page under its old
mapping with ERAM disabled; all other pages select bank 0 and Flash is unchanged.
These deliberately changed states are dependency controls, not valid Stock-call
failures. Reports: `/work/oct29-bank-overlay-state.json`,
`/work/oct29-bank-owner-controls.json`, and maintained complete suite reports in
`/work/oct29-bank-overlay-state-checks/`. Firmware/handler identity and complete
banked-reference integration remain open; no runtime mutation is enabled.

## Owner-aware bank-window model — local, 2026-10-29

The internal C helper `hpclang_rpl_stock_bank_window_select` predicts only the
reviewed Flash-selection update, retaining nonzero-owner pages. It rejects
invalid geometry/retained pages before publishing output; geometry provenance,
handler/calling-context qualification and the separate payload extent remain
required. ASan/UBSan tests cover each retained page, late failures, null inputs,
in-place output, fully hidden invalid banks and Flash-end bounds. ARMv4T compile
and analysis pass. Both maintained ROM suites pass with C predictions compared
at Flash-selection/exit stages; another 128 independent owner-byte controls agree
with the original ROM page-for-page. Reports and source hashes are under
`/work/oct29-bank-window-model/`, including `owner-controls.json`.
No public Port1 edit path is enabled; complete reference integration is open.
The prior 28 Stock call tests remain evidence for submitted `f9f50972`, not a
claim that this new internal helper has been exercised by a public Stock caller.

## Port1 physical mapping guard composition — local, 2026-10-29

Twelve controlled cases compose the C bank-window predictor, physical overlap
resolver and actual internal Port1 replacement/removal guard. ERAM-covered,
differently addressed and mixed-page aliases reject as BUSY without writes;
invalid late pages and out-of-bounds references reject without writes. Disjoint
Flash targets allow each operation. These tests provide one explicit reference;
they do not implement production root discovery or enable the public API.
The complete rpl_mutation ASan/UBSan suite passes (40.40 s), as do full tracked
clang-format/Ruff/ShellCheck checks. Prior stock-memory sanitizer, ARM analysis
and original-ROM mapping comparisons retain their separate scope.

Mapping/guard batch submitted as `dc6fbd39` in #26. Prior-head CI inspected; new-head CI not polled in the push turn. No merge or milestone acceptance.

## Mapped reference graph scheduler — local, 2026-10-29

`hpclang_rpl_reference_visit_graph` composes immediate structural parsing with
bounded cross-mapping scheduling. Exact mapping/address/bound/prologue tuples
deduplicate queued and visited nodes. Inline/compact descendants stay in their
mapping; other edges require an explicit resolver, with missing policies rejected
instead of silently dropping code or banked obligations. Valid parser reads carry
their mapping to the dependency callback. Success counts publish only after the
scan completes; errors invalidate all partial output. It closes only supplied
roots under the supplied policy, not production root coverage or mutation safety.
ASan/UBSan reference tests pass for cross-mapping cycles, duplicate roots,
different bounds, compact ACPTR payload/switch edges, opaque rejection, exhausted
capacity/budget, failed/invalid reads, callback failures and malformed resolver
outputs. ARMv4T Thumb compilation/static analysis and clang-format checks pass.
Actual Stock root classification and qualified resolution remain integration work.

## Saved HOME graph integration — local, 2026-10-29

`research/oct29-home-graph.py` compiles the current reference/parser sources and
reads the existing HP49g+/HP50g Stock saved states without mutation. Both HOME
structural scans succeed and enumerate 38 inline children, 20 attachments,
81 direct references, four opaque objects and one Flash pointer. These are
structural edges, not a closed reachability graph. The strict graph scheduler
rejects the first attachment (library ID 2, target 259640, field 982345) through
the unresolved-policy callback, preserves the unpublished visited-count sentinel,
and records parser dependencies. No unresolved edge is accepted as a terminal.
The two source SRAM/Flash files are byte-identical afterward. Source/input hashes,
all unresolved structural edges and strict rejection results are retained in
`/work/oct29-home-graph/report.json`. This is saved-state integration evidence,
not fresh Stock execution, complete root coverage or mutation authorization.
Next integration work is qualified attachment/catalog target resolution, followed
by the remaining direct, opaque and Flash-pointer obligations.

## Saved HOME attachment targets — local, 2026-10-29

`research/oct29-home-attachments.py` resolves the 20 attachment targets in each
previously captured HP49g+/HP50g HOME as exact 15-nibble DOACPTR objects. The
graph schedules these objects in the captured mapping; no payload bank is
inferred from a catalog binding. All IDs have unique catalog matches. The three
attachment fields belonging to direct-mapped catalog bindings agree with the
identity-checked hash/message header targets. Banked header checks remain open.
The existing ACPTR resolver recognizes 17 wrapper switches per model, but returns
unknown for two `0x22e83` wrappers and one `0x22e99` wrapper. This is a real
resolution gap, not a malformed-object or missing-library result. The strict
graph rejects the first ACPTR edge (field 259645, target 270562, switch 142979)
and leaves visited_count unpublished. Further execution edges remain obligations.
SRAM/Flash files remain byte-identical. Details are retained in
`/work/oct29-home-graph/attachments.json`; the companion report records source
and input hashes. Reader observations are diagnostic evidence, not a complete
transaction guard or proof of supported-firmware identity. Next: establish the
two additional switch semantics before extending production bank resolution.

## Additional HOME bank-switch execution — local, 2026-10-29

Maintained `bank_overlay.verify_attachment_switches`, invoked by
`bank_switch_maps.py`, compares original-ROM entries 0x22e83/0x22e99 against
the standard bank-2/bank-4 switch entries. Both captured supported ROMs pass
six paired scenarios each: initial saved mapping, selected bank 0 and active
ERAM, followed by entry P=0 and exit P=1. All 256 Read[] pages, 256 owner bytes,
mapping flags and both bank selectors match the canonical entry at each stage.
Normal entry selects bank 2/4; exit selects bank 2. ERAM remains visible while
enabled. All calls return with P=0 and unchanged Flash. The normal 17-switch,
16-overlay, guarded lookup and geometry checks also pass for each model.
Reports/source hashes: `/work/oct29-home-switch-regression/{model}/bank-switch-maps.json`.
The exploratory `/work/oct29-home-switches.json` retains baseline comparisons.
Ruff and strict documentation checks pass locally. These tests establish the
observed mappings in captured calling contexts, not a generic address-based
alias rule, complete handler semantics or production firmware authentication.
The runtime resolver still rejects the additional entries pending qualified
identity integration; safe reference movement and M8.0 remain incomplete.

## Guarded bank-alias lookup primitive — local, 2026-10-29

`hpclang_rpl_bank_alias_lookup_guarded` validates caller-supplied independent
identity ranges covering alias and canonical entry addresses, records every
matching nibble dependency, then resolves the canonical entry through the full
validated table. No ROM address or arithmetic alias rule is built in. Expected
ranges must cover the caller-reviewed equivalence proof, including shared code
and metadata; the primitive cannot establish semantic completeness itself.
ASan/UBSan `rpl_port` passes: every identity-nibble mutation and read failure,
all 105 identity/table dependency interruptions, positive callback rejection,
missing canonical coverage, duplicate/absent canonical entry, invalid expected
or observed nibbles, range overflow and missing callbacks preserve output.
ARMv4T Thumb compilation/static analysis and clang-format pass. This internal
primitive is not yet a production firmware profile or a complete ACPTR graph
resolver. Existing execution obligations, owner-aware mapping, ROM geometry and
transaction guards remain required. No Port1 edit capability is enabled by it.

## Alias selector dependencies — local, 2026-10-29

Original-ROM traces show both extra entries and their standard counterparts join
Saturn 0x22fec after loading different D0 addresses. At that boundary the captured
26 register-state words agree except D0: aliases select RAM 0x86063/0x8606d,
canonical entries select 0x860d6/0x860e0. The current selector contents agree,
explaining the earlier map equivalence; ROM code identity alone is insufficient.
`bank_overlay.verify_attachment_switches` now changes the alias selector's first
nibble in isolated emulator RAM. Both models pass six added counterexamples:
normal/bank-0 initial maps follow the changed bank 2/4, ERAM remains visible when
enabled, and P=1 exits still select bank 2. Canonical and unchanged-alias runs
remain independent. All calls return, Flash is unchanged, and the existing bank
and overlay gates pass. Reports: `/work/oct29-alias-selector/{model}/bank-switch-maps.json`;
execution paths/disassembly: `/work/oct29-bank-alias-trace.json`.
The alias API contract now explicitly requires mutable selector dependencies.
Production profiles must validate these data, shared handler semantics and
calling context; address or code-only matching must not authorize relocation.
Ruff, clang-format and diff checks pass. No production alias is enabled yet.

## Alias selector guard composition — local, 2026-10-29

Maintained `alias_guards.py`, invoked and source-hashed by `bank_switch_maps.py`,
uses the existing guarded FlashPtr selector matcher for the alias D0 fields
established by original-ROM tracing. Both saved models resolve selectors 1/3 to
banks 2/4 through unique configuration matches, retaining 85 observed nibbles
in 17 mapping-qualified guards per case. All 340 individual stale-nibble changes
are rejected. A legitimate alternate-bank configuration rejects the old guard
set, resolves to the alternate bank on recapture, and passes its new guards;
restoring the original configuration rejects the new guards. This composes data
resolution with transaction preflight primitives; it does not itself execute a
transaction or authenticate the switch code. Owner mappings and execution/payload
closure remain separate requirements. Both full maintained bank-switch scripts
pass, including original-ROM overlay/selector counterexamples. Reports are in
`/work/oct29-alias-guards/{model}/bank-switch-maps.json`.

## Saved HOME ACPTR payload composition — local, 2026-10-29

`research/oct29-home-attachments.py` now composes guarded selector matching,
validated native ROM geometry and the C owner-aware window predictor for the
first actual HOME ACPTR on both saved models. It requires the captured baseline
with ERAM disabled and all window owners zero; the calculated mapping retains
all other pages. The target at 270562 resolves to bank 2 (physical base 524288),
with exact object span 7291 nibbles. Parser reads retain mapping-qualified
observations. After scheduling that payload, the graph rejects the separately
emitted EXECUTION edge at field 259650, target 142979. No successful visited
count or mutation authorization is published. All 20 attachment objects were
scheduled first, and original SRAM/Flash files remain byte-identical.
This is controlled saved-profile integration, not a production identity profile:
geometry/owner capture and shared handler qualification still need production
freshness guards. Details/source and library hashes are retained in
`/work/oct29-home-graph/attachments.json` and its companion input/source report.
The local delivery batch also passed full tracked-file clang-format, Ruff,
ShellCheck, sanitizer reference/port tests, ARM compilation/static analysis,
both maintained original-ROM bank regressions and strict documentation checks.
Pre-push audit of dc6fbd39: four successful/six unfinished workflows; 153 successful,
20 running and 198 queued jobs, no new failures. PR20 remains three successful
workflows and one queued publisher (168 successful jobs, one queued).

## Address-register immediate decoding — local, 2026-10-29

`hpclang_rpl_code_literal_inspect` now decodes D0/D1 immediate loads of 2/4/5
nibbles into the existing literal descriptor. Loads replace low address nibbles
independently of P; shorter loads preserve high address bits. This identifies
encoded operands, not pointer provenance or complete machine-code references.
Sanitizer `rpl_code` tests pass all six forms across P=0..15, truncation, unreadable
and invalid input cases with unchanged outputs on failure. Original ARM Saturn
interpreter execution on both captured ROMs passes 192 controlled single-step
cases, comparing all general/address registers and P, with exact next PC.
Evidence: `/work/oct29-address-literals/literal-native.json` (source/driver hashes).
Maintained attachment-switch regression now calls the C decoder to obtain alias
and canonical RAM selector addresses instead of using hardcoded mutation targets;
all normal, alternate-selector and ERAM scenarios pass on both models. Reports:
`/work/oct29-address-literals/{model}/bank-switch-maps.json`. ARMv4T Thumb compile,
static analysis, clang-format and Ruff pass. Shared handler control flow and
production identity/transaction integration remain incomplete.

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

## Shared bank-switch body inventory — local, 2026-10-29

The bounded 0x22fec..0x23077 shared-body inventory decodes 31 instructions on
both captured ROMs. Existing descriptors identify P-dependent branches, setting
P=0, reading five nibbles from DAT0 into C, copying C.A to D0 and writing C.B
through DAT0. The path also contains CONFIG/UNCONFIG and return instructions;
those mapping/device effects must not be treated as ordinary RAM writes or
silently skipped. `/work/oct29-switch-body/report.json` records code bytes,
descriptors and source/ROM hashes. This diagnostic framing does not establish
complete semantics or mutation safety. During the concrete #26 CI wait, PR20
integration was prepared and pushed as 4f2ce605 after its prior full CI acceptance.

## Shared switch configuration effects — local, 2026-10-29

Original-ROM instruction-boundary traces retain C, D0, all read/write map cells,
configuration flags and ARM writes. In the tested shared path, the first CONFIG
uses C.A=0xff000 and sets flag 0x20; the second consumes the selected 0x7f000-page
configuration value and replaces that temporary flag with 0x4. The DAT0 byte
store invokes the bank mapping handler; UNCONFIG restores the original flags.
`verify_attachment_switches` now asserts this sequence for aliases and standard
entries under baseline, bank-0 and ERAM initial states, including changed-selector
counterexamples. It compares all Write[] cells as well as Read[] and rejects any
emulated ARM write outside the reviewed interpreter-state window (cpu..cpu+0xc20)
and controlled ARM stack. Flash remains unchanged. Both maintained model runs
pass; report flags, write-scope checks, maps and source hashes are retained in
`/work/oct29-config-effects/{model}/bank-switch-maps.json`. Expanded exploratory
trace: `/work/oct29-bank-alias-trace.json`. This bounds observed effects, not all
possible configurations, pending interrupts or production handler identity.
The memory-store descriptor alone is insufficient to classify the store as RAM;
configuration routing must be retained in production reference analysis.

## Bounded P=0 bank-entry program inspection — local, 2026-10-29

`hpclang_rpl_code_bank_entry_inspect` validates a caller-bounded code mapping:
full D0 literal, GOTO, ?P=0 branch and the complete 29-nibble shared configuration
sequence. It derives the selector/dispatch/body coordinates without fixed entry
addresses and observes every code read through the dependency callback. Output
changes only after complete success. Nonzero P and different code shapes remain
unsupported; truncation, bad reads and callback errors invalidate partial evidence.
Sanitizer `rpl_code` passes arbitrary-location layout, exact operands, every
observed-read failure, every shared-body nibble mutation, every dependency abort,
unsupported/invalid P and truncated body checks. Both maintained ROM regressions
validate extra and standard entry programs, covering exactly 45 unique code
nibbles per entry; the selector/configuration/ERAM tests also pass. Reports:
`/work/oct29-bank-program/{model}/bank-switch-maps.json`. ARMv4T Thumb compile,
static analysis, clang-format and Ruff pass. This is a bounded syntactic path
proof, not interpreter/device authentication: CONFIG routing, mutable data,
owner/geometry freshness, pending IRQs and return roots remain caller obligations.
It does not enable public Port1 edits or close complete execution coverage.

## Program-to-selector composition and all HOME attachments — local, 2026-10-29

Maintained `alias_guards.py` now derives each selector field by C program inspection
on every capture, then matches its current five-nibble value against all native
bank configurations. Code and data observations enter one mapping-qualified guard
set. Each case covers 45 unique code and 85 data nibbles; both models/two entries
reject all 520 individual stale changes. Legitimate alternate selections invalidate
old guards, resolve afresh and then reject restoration under the new guards.
Both full maintained model runs pass; reports and source hashes are in
`/work/oct29-composed-bank-program/{model}/bank-switch-maps.json`.
The controlled HOME integration now uses this inspected-program selector instead
of a fixed alias-to-selector mapping. All 20 attachment payloads per model resolve
through validated geometry and the owner-aware C window predictor to bounded
objects. Each independent wrapper traversal still rejects its separate EXECUTION
edge and leaves success counts unpublished. Whole HOME traversal likewise rejects
its first such obligation. SRAM/Flash files remain unchanged. Details, all payload
bounds and remaining execution edges are in `/work/oct29-home-graph/attachments.json`.
This covers the captured no-ERAM/no-owner-overlay context; firmware interpreter
identity, configuration routing, capture freshness and full root closure remain
production prerequisites. No public edit capability is enabled by these results.

## Program/selector delivery validation — 2026-10-29

Follow-ups through `efeec80a` pass full tracked-file clang-format, Ruff and
ShellCheck, strict documentation build, and rebuilt ASan/UBSan `rpl_code`,
`rpl_port`, `rpl_reference` and `rpl_stock_memory` suites (four of four).
Pre-push exact-head workflow/job audit of PR26 `d0471894` records five successful
and five queued workflows, 178 successful/19 running/174 queued jobs, no observed
failures. PR20 `4f2ce605` remains at twelve queued workflows/twenty queued jobs.
These are unfinished CI results, not acceptance. Follow-up delivery retains all
public Port1 and complete execution-reference gates. No new-head CI is polled in
the push turn. Full source-quality logs remain at `/work/oct28-pr25-format-fix`,
and documentation log at `/work/oct29-current-docs-check.log`.

## Device-routing dependency — local, 2026-10-29

Original-ROM read-before-write tracing identifies device configuration fields
among the incoming bank-switch dependencies. Inspection of the CONFIG and
UNCONFIG handlers shows that UNCONFIG checks configured RAM before the temporary
bank-control device. Changing only CPU+0xba8 from 0x80 to 0x7f makes the reviewed
P=0 path return with flags 5 instead of 3: RAM is unconfigured and Read mappings
change, despite unchanged entry code, selector and selected bank. This synthetic
state is a dependency counterexample, not a claim that ordinary launches use it.

Maintained `bank_overlay.py` now checks this for both alias and canonical entries
for banks 2 and 4 on both models (eight executions), including actual return,
flags [3, 0x23, 7, 5], changed Read maps, identical selected banks, bounded state/
stack writes and unchanged Flash. Both full maintained bank-map runs pass, with
reports/source hashes at `/work/oct29-bank-routing/{model}/bank-switch-maps.json`.
Ruff check/format pass. Read-before-write traces remain at
`/work/oct29-bank-dependency-trace.json`; independent collision traces are at
`/work/oct29-bank-config-collision.json`. Trace coverage does not establish all
possible dependencies. Runtime integration must qualify device-routing state as
well as code, mutable selectors, owners and geometry; public edit gates remain.

Exact-head CI audit: PR26 ac24c17f has eight queued/two pending workflows and
twelve queued jobs; PR20 4f2ce605 has ten queued/two running workflows, thirteen
queued/seven running jobs. Neither is accepted or merged.

## Bank device-routing check — local, 2026-10-29

`hpclang_rpl_stock_bank_route_check` checks the reviewed incoming CONFIG routing
flags, IO overlap for five-nibble selector reads and two-nibble control writes,
and RAM/ERAM/port priority before bank UNCONFIG. It uses native-unit snapshots;
callers still establish handler identity and guard freshness. It neither runs
a switch nor authorizes mutation. Unit coverage includes all 256 low flag values,
high-bit rejection, each RAM collision, IO boundary crossings, inactive fields
and address bounds. Rebuilt Stock-memory ASan/UBSan suite passes.

Both maintained original-ROM runs pass with the C check on normal P0/P1 paths
and reject all eight alias/canonical RAM-configuration counterexamples. Reports
and source hashes: `/work/oct29-bank-route-check/{model}/bank-switch-maps.json`.
Controlled HOME integration uses the same check for every resolved attachment;
all 20 payloads per model remain resolved and separate execution edges still
reject. The report at `/work/oct29-home-graph/attachments.json` records routing
fields and the library hash. These saved snapshots do not prove live freshness.

ARMv4T Thumb compilation and static analysis pass using the repository's existing
`core.FixedAddressDereference` exclusion for memory-mapped hardware. The first
ad-hoc analysis invocation omitted that exclusion and reported four existing L1
fixed-address accesses; no new routing diagnostic occurred. Changed C/Python
files pass formatting/Ruff. Public Port1 edits remain gated; this is a routing
precondition, not full execution closure or firmware authentication.

## Routing-state capture and guards — local, 2026-10-29

`hpclang_rpl_stock_bank_route_capture` reads all five native 32-bit routing fields
through a caller-qualified CPU-state reader and observes all 40 nibbles. Its
coordinates are state-relative nibble offsets, not Saturn virtual addresses.
Output is published only after complete success. Unit tests exercise 160
per-position read/invalid-nibble/negative-callback/positive-callback failures and
preserve the output sentinel. Rebuilt ASan/UBSan Stock-memory tests and ARMv4T
compilation/static analysis pass (existing fixed hardware-address exclusion).

Maintained alias guards combine the code/selector mapping with a distinct native
state mapping, capture routing before checking it, and validate both mappings
together. Four cases across both models retain 37 guards each and reject all
680 individual stale-nibble changes. Alternate valid selector changes still
invalidate old guards and resolve afresh. Both complete maintained model runs
pass; reports and source hashes are in
`/work/oct29-bank-route-guards/{model}/bank-switch-maps.json`. Changed source
format/Ruff and diff checks pass. Capture is not concurrency exclusion or firmware
identity; callers still need exclusive lifetime and complete mapping/geometry/
owner/root coverage. No production edit gate is removed.

## Upper bank-window state guards — local, 2026-10-29

`hpclang_rpl_stock_bank_window_capture` captures Read[0x40..0x7f] and its
64 owner bytes through the qualified CPU-state reader, observing all 640
nibbles and publishing only on complete success. It shares checked word reading
with route capture. The unit suite covers 2,560 per-position read/invalid-data/
callback failures with unchanged output, plus exact page/owner capture.
Rebuilt Stock-memory ASan/UBSan, ARMv4T compile/static analysis and changed-file
format/Ruff pass.

Maintained alias composition now predicts pages from the captured window and
retains the window dependencies alongside routing, code and selector guards.
Each case has 166 guards covering 810 nibbles; four cases across both models
reject all 3,240 individual stale changes. Both complete maintained model runs
pass; reports/source hashes: `/work/oct29-bank-window-guards/{model}/bank-switch-maps.json`.
Controlled HOME resolution also consumes both C captures, records native-state
dependencies under a separate mapping ID, and still resolves all 20 payloads per
model while rejecting separate execution edges. Its saved-profile report remains
`/work/oct29-home-graph/attachments.json`. This does not cover lower mappings,
write maps, geometry freshness, handler identity or complete execution roots.
Exclusive lifetime remains a caller obligation; no public edit gate is removed.

## Full bank read-map prediction — local, 2026-10-29

Review of the original ARM handler at 0x248dc reveals an unchanged-selector early
return, four owner-sensitive read-map quadrants, and a final optional two-page
overlay driven by CPU+0xc48/0xc5c. The earlier upper-window helper never modeled
those whole-handler effects. `hpclang_rpl_stock_bank_map_select` now predicts
Read[0..255], preserving old maps on unchanged selectors and applying overlay
restoration after other updates. It rejects absent banks and unsupported page/
overlay geometry without publishing output, and supports in-place output.

Maintained `bank_map_model.py` runs the original handler for four lower banks by
fourteen populated upper banks under baseline, owner-modified and crossing-
overlay contexts, on both models: 336 executions return and agree on all 256
read pages and bank selectors with unchanged Flash. Another 48 cases reject
absent upper banks without running them. Reports and source hashes are in
`/work/oct29-full-bank-maps/{model}/bank-switch-maps.json`. The full maintained
suite passes on both models. Unit tests cover all 256 malformed-page positions,
invalid selectors/overlays/banks, no-op overlay behavior and in-place results;
ASan/UBSan, ARMv4T compile/analysis and changed-source format/Ruff pass.

Controlled HOME now uses this complete read-map predictor and distinct mapping
IDs for lower/upper pairs; all 20 payloads per model still resolve, while separate
execution edges remain rejected. The saved snapshot is not a production state
guard: full-map/overlay capture, geometry provenance and handler identity remain
open. The predictor does not model write-map/sentinel/cache side effects or
authorize mutation.

## Complete read-map state capture — local, 2026-10-29

`hpclang_rpl_stock_bank_map_capture` observes all 256 read pages, 256 owner bytes,
prior lower/upper banks and two-page overlay fields: 2,592 native-state nibbles.
It publishes output only after complete success. Unit tests cover 10,368
per-position read/invalid-nibble/negative-callback/positive-callback failures
with unchanged output, plus exact full-width field capture. Rebuilt ASan/UBSan
Stock-memory tests, ARMv4T compile/static analysis and changed-source format/Ruff
pass.

Maintained alias composition now feeds this capture into the full map predictor.
Together with routing/code/selector observations, each case retains 558 guards
covering 2,762 nibbles; both models/two entries reject all 11,048 stale changes.
Both full maintained model runs pass, including the 336 original-handler map
comparisons and 48 absent-bank rejections. Reports/source hashes are in
`/work/oct29-full-map-guards/{model}/bank-switch-maps.json`.
Controlled HOME also uses the full C capture instead of direct field assembly;
all 20 payloads per model resolve and execution edges remain rejected. Report:
`/work/oct29-home-graph/attachments.json`. This captures read-map prerequisites,
not all interpreter execution state: geometry, handler identity, write/cache
effects, complete roots and the production exclusive-lifetime adapter remain.
No public edit capability is enabled.

## Bank geometry capture and delivery — 2026-10-29

`hpclang_rpl_rom_bank_capture_guarded` reads the independently qualified ROM
operand pair, count and KiB directory plus all sixteen native bases, then validates
the complete bank layout before publishing sixteen entries. Unused banks are
zeroed. Physical-ROM and native-state observations keep separate mappings.
Unit coverage includes 600 per-position read/invalid-data/callback failures,
unchanged output, unused-base inconsistency and bounds. Rebuilt port ASan/UBSan,
ARMv4T compile/static analysis and changed-source format/Ruff pass.

Maintained composition now captures geometry under a third mapping and feeds it
into the full predictor: 594 guards/2,936 nibbles per case; both models/two entries
reject all 11,744 stale changes. Both full maintained model runs pass, including
336 actual original-handler read-map comparisons. Reports/source hashes:
`/work/oct29-bank-geometry-guards/{model}/bank-switch-maps.json`. Controlled HOME
also uses guarded geometry capture; all 20 payloads/model resolve and execution
edges still reject (`/work/oct29-home-graph/attachments.json`). The caller still
qualifies operand locations, Flash size and handler identity; metadata plausibility
is not authentication.

This batch includes device routing, native-state/geometry guards and complete
read-map prediction. Full tracked-file format/Ruff/ShellCheck and strict docs pass
locally. Pre-push PR26 ac24c17f audit: three successful/seven queued workflows,
27 successful/152 queued jobs, no failures observed. PR20 4f2ce605: five successful,
one running/six queued workflows; 149 successful/21 running/218 queued jobs.
Follow-up submission is not acceptance; no new-head CI is polled in the push turn.
No public edit gate or retained M8 requirement is removed.

## Reviewed ROM content profiles — local, 2026-10-29

No reusable launcher content identity existed; its established Read[] checks
qualify mapping shape, not firmware bytes. Internal `rpl-rom-profile.c` now
identifies the complete immutable physical prefix [0,0x140000) with SHA-256 and
returns model-specific layout locations only on a match. User Flash is excluded.
Reviewed prefix digests:

- HP49g+: `894e146c3252191ad3700b17d24bc0669c6f3f0923e9307c1ed4f3d9857477d8`
- HP50g: `be21f48a44feb9a899a1e9c31799a55e8d1cbdd7e3774e868d64062e41f5105c`

The reader uses blocks of at most 64 bytes and caller-qualified physical access.
No Flash commands, allocation or partial output. SHA-256 unit vectors were
independently generated with Python hashlib for 13 lengths including padding
boundaries and the full prefix. ASan/UBSan tests, ARMv4T compile/static analysis
and full tracked-file format/Ruff/ShellCheck pass. The source is included in the
SDK runtime source list.

Both maintained model runs identify their profile, reject ten changed firmware
bytes/model and the wrong model, accept changes at both user-area boundaries,
and reject four early/late read failures/model without publishing a profile.
Geometry capture now takes its operand/base addresses and Flash size from the
matched profile. Full combination reports/source hashes:
`/work/oct29-rom-profile-composed/{model}/bank-switch-maps.json`. Controlled HOME
also identifies the profile before resolving all 20 payloads/model; execution
edges still reject. Its report records the matched profile and library digest.

This identifies these exact contents, not an authenticated signature or all
possible ROM versions. Physical-reader permission/identity, exclusion while
using the result, CPU-state identity and complete handler/root coverage remain
production obligations. Firmware matching alone does not enable public edits;
performance on calculator hardware has not yet been measured.

## 2026-10-29 — Physical ROM-profile backend, local validation in progress

The read-only Stock ROM backend captures both identity section descriptors and
checks them before each bounded block read. Assembly halfword loads support
physical address zero; the caller must mask IRQ/FIQ and prohibit Stock calls or
mapping/Flash changes throughout capture/read/profile use. Host sanitizer checks
cover odd/even boundaries, invalid ranges and every descriptor bit becoming stale.
ARM analysis passes for the backend, identity, memory reader and Stock witness.
No Flash programming is performed.

The mandatory Stock ROM-bank witness identifies the immutable image before using
its geometry locations. `/work/oct29-stock-rom-profile/reproduction.json` records
16 independently rebuilt, byte-identical artifacts and source hashes;
`native-entry-verified.json` checks both original first-case returns and unchanged
Flash. Both models passed their first three call/catalog cases; the complete
14-case-per-model campaign remains running and unaccepted. These local results
do not establish CI acceptance, production reference safety or M8.0 completion.

Independent saved-memory follow-up: `profile-memory-verified.json` translates
ELF evidence symbols through the original launcher pool table and its build
metadata, then checks both first-case post-return SRAM images. Model IDs 5/6,
immutable extent 0x140000, geometry locations 0x248a0/0xbe0, 14 populated banks,
3,520 samples and both identity-section descriptors match. ELF, metadata and
saved-SRAM hashes accompany the result. This confirms witness execution only;
it does not qualify remaining execution roots or the still-running full matrix.
Exact-head CI audit: #26 b5891517 has ten queued workflow records while its jobs
report two success, one running, thirteen queued. #20 4f2ce605 has six successful,
one running, five queued workflows and 278 successful/19 running/91 queued jobs.
Neither head has a failed job in this audit; neither is fully accepted.

## 2026-10-29 — Bank-control SRAM effects

The maintained bank-map regression now compares the entire saved SRAM image
against an independently expressed model of the reviewed original ARM handler,
and rejects any write outside predicted interpreter fields or its nine-register
stack save. Besides all 256 read pages, it checks Read[256], bank selectors,
conditional overlay write entries and saved read/write cache fields. Unchanged
selectors require no interpreter writes. Four overlay contexts distinguish both,
neither and either individual page already matching the overlay backing page.

Both full maintained runs pass in `/work/oct29-bank-handler-effects/{model}`:
336 original-handler executions and 48 absent-bank rejections per model, along
with existing ROM identity, 17 switch, 16 ERAM and six attachment-sequence checks.
Source and input hashes are in each `bank-switch-maps.json`. Ruff and formatting
pass. This qualifies the tested populated-bank handler's SRAM effects under six
controlled contexts, not all register/interrupt/configuration behavior or a
production execution-edge resolver. The separate live Stock profile matrix
continues against its unchanged application/source manifest.

## 2026-10-29 — Runtime bank-effects predictor

`rpl-bank-effects.c` adds a pure runtime prediction API for the reviewed populated
bank handler. It composes existing read-map validation with selector/sentinel,
conditional write-map and saved overlay-cache updates. Equal selectors preserve
all captured state. Write/cache words remain opaque values, never dereferenced
or certified as writable. Error returns preserve output; exact input/output
aliasing is supported. A caller still needs independently qualified snapshots,
guarded capture, exclusive lifetime and complete execution-reference semantics.

The maintained tests compile this runtime implementation and compare every
predicted field with original ARM execution under all six contexts on both
models: 672 matches, including in-place prediction, and 96 absent-bank failures
with untouched output. Full reports and source hashes are retained under
`/work/oct29-runtime-bank-effects/{hp49gplus,hp50g}/bank-switch-maps.json`.
ASan/UBSan unit test 167 passes null/invalid-page rejection, no-op preservation,
last legal overlay pair (254/255), cache/write values and in-place operation.
ARMv4T Thumb compilation/static analysis pass. The first build exposed a missing
complete geometry-type include; adding `rpl-port.h` fixed it before all passes.
The separate live Stock matrix uses its previously reproduced application;
this predictor is not yet part of that application or a production resolver.

## 2026-10-29 — Guarded bank-effects state capture

The runtime effects capture composes the existing read-map/owner/selector/overlay
capture with the sentinel, 256 write-map words and four saved cache words.
It reports 4,680 unique native-state-relative nibble dependencies and publishes
only a complete snapshot. Readers and callbacks require the same exclusive
mapping lifetime as prediction; partial dependency collections must be discarded
on failure. Raw captured words still require separate semantic qualification.

ASan/UBSan test 167 passes 18,720 fault cases across every read/dependency position
(unavailable read, invalid nibble, negative callback, positive callback), plus
exact field/coordinate coverage and null inputs. Both maintained runs under
`/work/oct29-bank-effects-capture/{model}/bank-switch-maps.json` pass: all 672
original-handler predictions now use C-captured state, independently compared
with raw fixture fields, each recording 4,680 unique dependencies. The 96 absent
bank rejections and all existing suites remain passing. ARM compilation/analysis,
full tracked-source formatting/Ruff/ShellCheck pass. Production execution-edge
integration and full execution-state qualification remain open.

The preceding remote heads remain pending with no failed job in this audit:
#26 b5891517 has seven queued/three running workflows and 12 successful/three
running/104 queued jobs; #20 4f2ce605 has seven successful/five queued workflow
records and 297 successful/17 running/75 queued/one skipped jobs. Workflow/job
snapshots are reported independently; neither head is fully accepted.

## 2026-10-29 — Complete physical ROM-profile Stock matrix

Both live runs under `/work/oct29-stock-rom-profile` finish all 14 cases and
validate unchanged application/source input hashes. Independent `verified.json`
checks 28 post-call markers, 18 original returns, ten fixture-rewritten returns,
36 unchanged Flash snapshots and 16 reproduced artifacts. The expanded
`profile-memory-all-verified.json` independently translates each ELF evidence
symbol through its saved original pool table: all 28 cases report the expected
ROM model/layout, 14 populated banks and 3,520 samples. Saved SRAM and artifact
hashes accompany the evidence.

A fresh independent rebuild from the current tracked source (including the new
bank-effects module) matches all 16 original HPC1/stage/metadata/launcher artifacts
byte-for-byte; unused bank-effects code is not exercised by this Stock witness.
The updated reproduction manifest records current source hashes, and its temporary
build is removed. The effects capture/predictor remains separately covered by
sanitizers, ARM analysis and the 672 original-handler comparisons. This closes
this local Stock campaign, not M8.0 or production execution-reference acceptance.

Pre-push audit: #26 b5891517 has one successful/nine queued workflows and 16
successful/two running/217 queued jobs, with no failed jobs. #20 4f2ce605 has
seven successful/five queued workflow records and 300 successful/18 running/71
queued/one skipped jobs. Neither head is accepted. Historical unexplained
pre-entry Kermit/PWM failures remain recorded separately.

## 2026-10-29 — Full effects in alias and controlled HOME resolution

The maintained alias composition now uses `hpclang_rpl_bank_effects_capture`
and `hpclang_rpl_bank_effects_select`, recording write-map/sentinel/cache
observations alongside program, selector, routing, geometry and read-map state.
Each alias records 5,024 unique nibble dependencies in 1,011 bounded guards.
Across two aliases and two models, all 20,096 individual stale-nibble changes
invalidate the old result. Legitimate alternate selectors invalidate old guards,
resolve a fresh bank, then invalidate again when restored. Both full maintained
runs pass at `/work/oct29-bank-effects-alias/{model}/bank-switch-maps.json`,
including the 672 original-handler effects comparisons and 96 absent-bank cases.

The research HOME integration consumes the same full effects captures/predictor.
`/work/oct29-home-graph/attachments-effects.json` retains source/library hashes:
all 20 attachment payloads per model resolve and their separate EXECUTION edges
still reject without publishing a completed graph. Saved SRAM/Flash remain
unchanged. This is controlled integration, not the production public Port1 gate.
Formatting and Ruff pass for the changed maintained test.

Exact-head audit: #26 bdeb3227 has eight queued/two running/one pending workflows,
16 queued/two running jobs; #20 4f2ce605 has seven successful/five queued workflow
records and 303 successful/18 running/68 queued/one skipped jobs. No failed jobs
appear in either audit; both deliveries remain unaccepted.

## 2026-10-29 — Unified runtime bank planning

`rpl-bank-plan.c` composes the formerly test-orchestrated sequence: bounded P=0
program inspection, unique configuration-table lookup, guarded selector read,
routing checks, profile-derived geometry capture and full effects prediction.
The three coordinate spaces carry explicit distinct mapping IDs. The selected
table index must also agree with reviewed control-bit semantics. Caller-owned
scratch storage keeps intermediate plans private; only complete success changes
output. The caller still supplies an identified ROM profile, qualified code,
native state/configuration table and exclusive lifetime. This API does not
execute the switch or classify its EXECUTION edge as terminal.

Both maintained runs pass under `/work/oct29-bank-runtime-plan/{model}`. For two
aliases and their alternate configurations, the new C plan matches the prior
staged implementation's program, selected bank, configuration, geometry and full
effects. All 144 failure controls (callback errors at eight stages, mapping-ID
collision and swapped configuration-table rows) preserve output. The existing
20,096 stale-nibble controls, 672 original-handler effects comparisons, ROM
identity and attachment suites continue to pass. ARMv4T compilation/static
analysis and full tracked-source formatting/Ruff/ShellCheck pass; the final
Python counterexample additions also pass repository formatting/Ruff.
Production/HOME single-call integration and complete execution roots remain open.

CI remains pending: #26 bdeb3227 has eight queued/two running/one pending
workflows and 13 queued/five running jobs; #20 4f2ce605 has seven successful/five
queued workflow records and 310 successful/16 running/63 queued/one skipped jobs.
No failed job appeared in this exact-head audit.

## 2026-10-29 — Unified planning across HOME and standard bank entries

Controlled HOME attachment resolution now calls `hpclang_rpl_bank_plan_prepare`
once per banked payload, replacing its manually sequenced inspection/capture/
prediction calls. `attachments-plan.json` and `attachments-plan-verified.json`
under `/work/oct29-home-graph` record script/library/report hashes. Independent
comparison with the previous full-effects path confirms all 40 payload banks,
addresses, spans, mappings and code locations agree, while all 40 separate
EXECUTION obligations still reject and no completed graph is published. The
saved SRAM/Flash remain unchanged; this remains saved-state integration.

The maintained alias suite additionally calls the planner on all 16 standard
bank-table entries per model. All fourteen populated banks produce the expected
selector, configuration and geometry; both absent banks reject without changing
output. Both full suites pass under `/work/oct29-bank-plan-entries/{model}`,
retaining the existing 20,096 stale guards, 144 preparation failure controls and
672 original-handler effect comparisons. Changed Python formatting and Ruff pass.

Exact-head CI: #26 bdeb3227 has nine queued/two running workflows with 15
successful/219 queued/five running jobs; #20 4f2ce605 has eight successful/one
running/three queued workflows with 328 successful/15 running/46 queued/one
skipped jobs. Neither audit shows failures, and neither head is fully accepted.

## 2026-10-29 — Live Stock planning readers and new campaign

`rpl-stock-view.c` provides bounded native-state-relative, physical-ROM and mapped
Saturn nibble readers. Capture validates the complete 0xc60-byte SRAM state span
and user-readable identity sections. Reads reject stale descriptors, unaligned
or out-of-range physical pages, user Flash and MMIO without publishing a value.
Immutable-ROM reads reuse assembly halfword access, including physical zero.
The caller must preserve exclusive state/mapping lifetime and separately prove
ROM/state identity. No native writes or bank switching occur.

ASan/UBSan test 168 passes complete native-state traversal, all 256 map pages'
first/last nibble pairs, SRAM/ROM boundaries, invalid extents/pages and individual
descriptor-bit changes. ARM compilation/analysis and full source quality checks
pass. The Stock witness now runs the unified planner against fourteen populated
standard entries and two aliases, rejects two absent entries, and compares all
0xc60 native bytes before/after. Static workspace avoids putting its snapshots
on the application stack. The application has 11,808 static bytes and requires
six RW pages; generated metadata includes the increase.

`/work/oct29-stock-bank-plan` contains the newly built application/six launchers.
Independent rebuilding matches 16 artifacts; seven relevant ARM units pass
analysis. The HP49g+/HP50g 14-case matrices are running against fixed input
manifests; the first HP50g original Stock case has passed. These new results are
not the preceding completed 28-case ROM-profile campaign and are not complete
matrix or production-reference acceptance.

Exact-head CI remains pending with no failed jobs in this audit: #26 bdeb3227
has two successful/nine queued workflow records and 27 successful/216 queued/
three running jobs; #20 4f2ce605 has eight successful/two running/two queued
workflows and 328 successful/17 running/44 queued/one skipped jobs.


## 2026-10-30 — Stock planner and execution-map follow-up

The completed `/work/oct29-stock-bank-plan` campaign passes the independent
`verified.json` audit: 28 return markers (18 original, ten fixture-rewritten),
36 unchanged Flash snapshots and 16 reproduced artifacts. The additional
`plan-memory-all-verified.json` audit reads all 28 saved SRAM states through the
recorded pool tables and ELF symbols: ROM identity, bank witnesses and planner
counts (16 accepted, two absent, observed dependencies, native extent 0xc60)
match in every case. This closes the local campaign, not M8.0.

The new execution-map predicate compares physical locations page by page without
requiring physically contiguous runs. Unit 169 passes under ASan/UBSan; ARMv4T
compilation and static analysis pass. Maintained original-handler reports under
`/work/oct29-execution-map/{hp49gplus,hp50g}` have matching source hashes. Across
both models, 142 valid switch cases preserve the shared body's physical mapping,
530 remap it, and 96 absent-bank cases reject. The comparison also covers a
range crossing a page boundary. These synthetic contexts demonstrate a missing
continuation condition; they do not establish full Saturn execution safety.
The predicate is not yet part of production planner acceptance.

PR #26 exact head `bdeb322708ebae3ce974a6ac41dfeee46cf140b4` has eleven successful
workflows, 379 successful jobs and two conditional skips. Local follow-ups are
not covered by that CI result; no milestone completion is claimed.


## 2026-10-30 — Planner continuation gate and resource snapshot acceptance

The planner now checks physical mapping preservation for all 29 nibbles of the
inspected shared body before publishing a plan. Both maintained reports under
`/work/oct30-plan-execution` pass the original-handler/alias/standard-entry
regressions. Each populated canonical entry additionally forces a different
incoming code page and a real selector transition: all 28 cases reject and
preserve the output sentinel; restoring the input restores success. This checks
a necessary continuation condition, not all intermediate CONFIG semantics or
execution roots. ARM compilation/static analysis pass, and the application plus
six launchers build under `/work/oct30-stock-plan-execution`. A new 28-case Stock
campaign is running; earlier Stock acceptance remains scoped to its old build.

PR #20 head `4f2ce60534d366a7e84ff06338286d07766edd09` passed all twelve workflows
(390 successful jobs, two conditional skips). Scope review confirmed resource
capture/verifier/UI evidence changes and no production runtime changes relative
to main. It was marked ready and merged as `3b77453a70d0850a42736ba728eb29081fd7c6b1`.
M8.2/M8.3 remain open; snapshot delivery alone is not whole-application acceptance.


## 2026-10-30 — Main integration and HOME continuation regression

PR #26 needs fresh CI after integrating accepted main `3b77453a`: the workflow
conflicts retain both code-relocation stage1/incoming-carry artifacts and resource
memory metadata/snapshots. Historical evidence from both parents is retained.
Eighteen resource snapshot/verifier/UI tests, full format/Ruff/ShellCheck and
strict docs pass in the isolated integration worktree. The previous exact head
`bdeb3227` passed eleven workflows (379 successful jobs, two conditional skips).
This integration does not include the separate local bank-planner follow-ups.

The locally integrated continuation check also passes the controlled HOME
regression under `/work/oct30-home-plan`: forty payload resolutions match the
previous effects path and forty separate execution obligations remain rejected.
The fresh Stock campaign is still running; both initial native cases pass.


## 2026-10-30 — Continuation-build reproducibility

`/work/oct30-stock-plan-execution/reproduction.json` confirms sixteen byte-identical
artifacts from an independent clean rebuild of the current continuation-check
source: HPC1, result address, stage1, launcher memory metadata and twelve model
launcher files. Source hashes were stable during the rebuild; the temporary
build was removed. Eight actual-application ARM analyses pass, including the
new execution-map predicate and its planner integration. The first native
saved-memory witness per model independently confirms profile/bank identity and
16 accepted/two rejected plans; the full live campaign remains in progress.
These results do not reuse earlier-build Stock acceptance.

The exact-head CI audit of #26 `d0795413fad89345a52ad20dc89b7ea018f34f61` found
12 queued workflow records and 20 queued jobs, with no completed jobs yet.
Current status text was shortened; M0–M7 summaries, all M8 stage requirements,
acceptance boundaries and detailed historical evidence remain retained.


## 2026-10-30 — Port2 startup configuration mapping inventory

During the live Stock wait, the new bank planner was applied read-only to the
retained reservation baselines for both models. `/work/oct30-startup-config/inventory.json`
records source script/shared-library hashes, unchanged SRAM/Flash hashes, native
catalog bindings, configuration entry framing and physical locations. Both
inventories match: 59 catalog IDs below 769, of which fourteen have configurations:
0, 1, 2, 12, 13, 171, 221, 222, 223, 226, 227, 229, 255 and 256. All fourteen
configuration roots frame as programs; this does not classify their transitive
execution references or establish startup behavior.

The first exploratory assertion incorrectly equated all lower-ID catalog entries
with configured libraries. The final inventory resolves each library header
through its predicted bank mapping and distinguishes absent configuration fields.
IDs 226, 227 and 229 have configuration-object bytes in mutable user Flash beyond
0x140000; all other configuration roots are below that physical boundary. Thus
immutable-ROM identification alone cannot qualify all earlier startup effects.
This supplies concrete next inputs for execution-reference qualification and
trusted startup ordering, not permission to erase Flash or publish recovery.


## 2026-10-30 — Maintained startup configuration regression

`startup_configs.py` is now part of the maintained bank-switch suite and source
hash manifest. The two reports under `/work/oct30-startup-maintained` pass the
full existing bank/alias/effects matrix plus configuration-root classification.
The helper also passes both retained recovery baselines. Initial fixed-count
assertions exposed a real fixture difference: the launch states additionally
contain ARMToolbox 275, whose configuration object is in mutable user Flash.
Recovery baseline: 59 lower-ID catalog entries, fourteen configuration roots,
mutable IDs 226/227/229. Launch fixture: 60 entries, fifteen roots, additionally
mutable ID 275. The test checks absent configuration output preservation,
program framing, the entire object's physical extent and explicit unqualified
execution status. This is mapping/root coverage, not startup execution proof.

Independent source-hash readback confirms both completed reports match current
sources. Exact-head #26 CI has eight queued/four running workflows and three
successful/six running/fourteen queued jobs, with no failure at inspection.
The separate live Stock continuation campaign remains in progress.


## 2026-10-30 — Complete Stock continuation-check campaign

Both `/work/oct30-stock-plan-execution/{hp49gplus,hp50g}/result.json` reports
completed successfully with fourteen cases each. The independent `verified.json`
audit confirms all 28 post-call markers, 18 original and ten fixture-rewritten
returns, 36 byte-identical Flash snapshots and sixteen reproduced artifacts.
`plan-memory-all-verified.json` independently reads all 28 saved SRAM witnesses
through ELF symbols and original pool descriptors: ROM/profile identity, fourteen
banks/3,520 sampled edges and sixteen accepted/two absent planning entries match.
Source manifests include the execution-map predicate; the planner's full native
state comparison remains mandatory. This completes the local campaign only.

The resource-snapshot main integration `d0795413` was merged into the local
planner branch, resolving only roadmap/history conflicts and retaining both
workflow artifact sets. Eighteen resource/verifier/UI regression tests pass.
The tested application sources and eight-module analysis inputs remain unchanged.
Remote CI has not yet tested the combined planner follow-up.


## 2026-10-30 — Planner follow-up submitted

Head `6a582958` was pushed to #26 after independently checking every preceding
`d0795413` workflow/job: one successful, two running and nine queued workflows;
24 successful, four running and 340 queued jobs, no failures. That preceding
integration was not accepted as complete. The submitted batch includes unified
planning, live Stock readers, continuation mapping checks, the latest completed
28-case Stock witness and maintained startup configuration regressions. Artifact
and source manifests were rechecked before delivery. No new-head CI was polled
in the push turn. Local passing evidence remains distinct from remote acceptance
and from completion of M8.0. The PR description now reflects this full batch.

## 2026-10-30 — Guarded configuration and independent migration tooling

Local M8.0 commit `9e220d9b` records all eight configuration-header nibble
dependencies, including absent configuration. The focused sanitizer test covers
64 observer/read/malformed-input faults; ARM compilation/analysis and maintained
HP49g+/HP50g regressions pass. Saved reports under `/work/oct30-config-guard`
record 480 dependencies per model across 60 catalog entries. This does not prove
configuration execution safety and was not included in submitted `6a582958`.

M8.5 `tools/check-migration.py` adds six lexical review categories, original
source locations after line splicing, text/JSON output and explicit check/error
exit codes. Five unit tests cover comments, ordinary/raw literals, line splices,
identifier boundaries, discovery/deduplication, read-only behavior and input
errors. Actual CMake SDK installation followed by relocation runs the installed
tool on external source and checks diagnostics, source preservation and installed
file identity; `/work/oct30-migration-install.json` retains the result. Temporary
SDK trees were removed. The maintained SDK validator now checks this tool too;
the full existing SDK compiler/application matrix was not rerun for this slice.
Full tracked formatting, Ruff, ShellCheck and strict documentation checks pass.
These are local results, not M8.5 acceptance or a source-compatibility certificate.

The preceding #26 head `6a582958` was audited before further delivery: four
successful/seven queued workflows, with 118 successful, twelve running and
248 queued jobs; no failures in this snapshot. Historical failures remain above.

Migration delivery: `b59fb56d` pushed in draft PR #27; new-head CI was not
polled in the submission turn. M8.5 remains incomplete.

## 2026-10-30 — Full migration SDK client validation

Submitted migration head `b59fb56d` passes the complete maintained relocated SDK
validator with prepared HP39 assets: external C/C++ applications, ARMv4T return
checks, generated fonts/HPC1, six-model launchers, all six model-specific
Notebook builds, incremental-output restoration, invalid-model rejection and
HP39g+/HP39gs/HP40gs Aplet packaging. Two clean builds produce 517 byte-identical
release files. Log: `/work/oct30-migration-sdk-full-prepared.log`, exit 0.
The installed migration checker runs as part of this gate. Temporary SDK/build
and freshly compiled host-tool directories were removed by their context managers.
This supersedes the earlier note that the complete SDK matrix was not rerun.

The first invocation failed during configuration because the selected raw
HPGCC39 asset directory lacked HPCRESET.000; it is not passing evidence.
The corrected run used the prepared SDK assets, checked reset bytes against the
maintained generator and compiled the packaging tool afresh. Its complete run
passed. The original failure log remains `/work/oct30-migration-sdk-full.log`.

Preceding remote audit: #26 `6a582958` has 125 successful, fourteen running and
239 queued jobs, no failures; #27 `b59fb56d` has ten queued workflows and sixteen
materialized queued jobs. Neither branch is CI-accepted. M8 remains incomplete.

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

## 2026-10-30 — PR26 HP39g+ runtime transfer failure and retry pacing

PR26 head `6a582958`, run 41024016583, job 123960548578 fails before application
entry: host-gate-failed, no PASS/return, zero payload keyboard frames, PC 0x44c4.
Directory transfer completed; payload send-window 57 stalled for 90 seconds,
with three NAKs and repeated identical retransmissions. All 1,668 audited data
and short ACK/NAK packets have valid length, sequence and CRC at socat. The
final device Timeout packet has a valid CRC but an invalid sequence field.
Evidence: `research/oct30-pr26-runtime-packet-audit.json` and downloaded artifacts
under `research/oct30-pr26-hp39-runtime-failure`. This does not locate the fault
inside the device/emulator or prove a native runtime regression.

The current mainline omitted `ckermit-resend-pacing.patch`, previously tested
on the still-unaccepted pacing branch. An isolated follow-up now applies it
and its PTY regression: retransmissions honor configured SET SEND PAUSE before
updating the RTT timestamp and sending the unchanged packet. Both the older
local server and the exact downloaded failing CI server fail the turnaround
negative test; a freshly compiled patched server passes the entire PTY suite.
Logs, binary and input hashes: `/work/oct30-kermit-retry`. Ruff and ShellCheck
pass. Existing x50ng cache keys already include the build script and all Kermit
patches, so this change invalidates the stale server bundle.
A replay under `/work/oct30-pr26-runtime-replay` uses exact CI application and
assets/emulator with only the Kermit executable replaced; it is still running.
Neither this diagnosis nor local protocol success waives remote CI failures.

The replay completed with exit 0: application PASS, all 52 keyboard frames
checked by the maintained harness, normal return and four settled-return
observations. Original CI asset files compare byte-for-byte; the final status,
input hashes and retained keyboard montage hash are recorded in
`/work/oct30-pr26-runtime-replay/verified.json`. Individual keyboard frames are
removed by the maintained harness after its count check and montage creation.
This local result supports delivery of the pacing correction, not acceptance
of #26 or proof that it explains every original packet-loss event.

Retry-pacing delivery: `fce14d73` pushed in draft PR #29 after auditing and
investigating preceding CI failures. New-head CI was not polled this turn.

## 2026-10-30 — Installed public API conventions

The M8.5 guide `docs/sdk/api-conventions.md` documents service-specific results,
errno handling, versioned output initialization, failure-safe output replacement,
C++ owner/view/lexical-guard distinctions, close errors and exit behavior. Public
object-store iterator declarations now state 1/0/-1 results and output/name rules,
matching the existing implementation. The guide is linked from Getting started,
migration and navigation and installed with the SDK. It does not freeze pending
UI names or imply cross-version object/layout compatibility. The obsolete note
claiming opt-in formatting still awaited remote acceptance was removed.

Strict docs pass. Actual CMake installation and prefix relocation preserve the
new guide byte-for-byte; installed navigation links resolve, and its complete
C11 example compiles for ARMv4T with Wall/Wextra/Werror. Temporary installation
and build directories are removed. Evidence: `/work/oct30-api-contracts-installed.json`.
This documentation/header-comment change adds no runtime behavior; the prior
517-output full SDK result remains separately scoped to `b59fb56d`.

CI audit: #26 `6a582958` has 228 successful/twenty running/129 queued/one skipped
and one failed job (the previously investigated HP39g+ runtime transfer).
#27 and #29 each have ten queued workflows/sixteen materialized queued jobs;
#28 has eight queued workflows/twelve materialized queued jobs. No new failure
was found and no branch was accepted.

M8.5 delivery: guide follow-up `5d5195dc` pushed to existing PR #27 after the
preceding exact-head audit; new-head CI was not polled in this submission turn.


## 2026-10-30 — Port C++ integration and second PR26 failure

The local PR14 merge preserves the native-allocation parent PR13 and accepted
main resource snapshots. Combining `--artifact` with `--native-release` had
skipped the latter through an `elif`; independent dispatch and a combined-CLI
regression retain both gates. Eight UI verifier tests and Ruff pass. GCC
ASan/UBSan tests `port` (including the real C++ owner fixture), `native_allocation`
and `pool_checkpoint` pass. The first Clang configuration failed because the
container lacks its host ASan runtime; that environment failure is retained in
`/work/oct30-port-cpp-integration/clang-environment-failure.log`. GCC reused the
build directory with a fresh configuration. ARMv4T Object Basics links and strict
docs pass. Logs: `/work/oct30-port-cpp-integration/{unit,arm}.log` and
`/work/oct30-port-cpp-docs.log`. No merged emulator or full SDK result is claimed.

PR26 exact head `6a582958` audit now has two failed jobs. Job 123960552830 passed
Stock execution and original-interpreter verification, then failed in the
`native-missing-message/message-run` transfer before USB server readiness.
The saved emulator log reports `key=6 down=1 ready=1 sampled=0 pressed=1` and
`phase=input-not-sampled`. This is distinct from the HP39g+ packet-57 failure;
its root cause is not established. Downloaded evidence is retained at
`research/oct30-pr26-relocation-failure`, with the job log beside it. Neither
failure is waived. The original 88 native reference/control cases and preceding
native/new-message checks passed, but do not accept the remaining cases.


## 2026-10-30 — Reproduced first-run relocation abort

Audit of all six saved code-relocation CPU states rejects both missing-message
states (first execution and second launch); the other four return-state checks
pass. First-run PC is 0x10, CPSR 0x1d7, data fault status 0x15, fault address
0x53000000. The earlier key failure diagnosis described a secondary symptom.
`research/oct30-pr26-relocation-cpu-audit.json` retains each state result.

`research/oct30-pr26-relocation-replay.py` uses the original CI emulator/server,
application artifacts and archived missing-message package. A fresh Stock
baseline passes; first missing-message execution reproduces ARM abort locally,
before any second launch. Logs and states remain in
`/work/oct30-pr26-relocation-replay`; input hashes and CPU values are recorded in
`research/oct30-pr26-relocation-reproduction.json`. The recorded fault address
may be secondary to an earlier exception; the initial fault is not yet known.

The local code-relocation verifier now applies the existing Stock CPU-return
check after each producing run, before message/cache relaunch and at the initial
branch probe. An orchestration regression covers abort and undefined modes and
requires that no second transfer occurs; it and 47 recovery-verifier tests pass.
Ruff and formatting pass. This is a diagnostic/acceptance correction, not a
runtime fix, and does not accept PR26 or complete M8.0.


## 2026-10-30 — Relocation fixture stack collision and local correction

Original CI emulator/ELF payload identity was checked against retained
`/work/oct30-stock-plan-execution/app/e2e.hpc` (SHA256
326692cc6a7a25175129c14db47d6df92f5695d512712bfb0032829096418968).
A debugger breakpoint at the data-abort branch (`do_interrupt+622` in this exact
emulator) captures the first data abort at guest PC 0x28 in user mode, before the
ROM handler enters abort mode. Guest breakpoints then show a correct sbrk return
at 0x0122bf52 followed by a zero return address. The allocation starts at
0x0112e2f4, requests 0x28f8 bytes and ends at 0x01130bec, beyond the active
SP 0x011308f0. Clearing it overwrites live stack/return data. Logs remain in
`/work/oct30-relocation-abort-pc/run/emulator.log` and
`/work/oct30-relocation-return/guest.log`. Two earlier instrumentation attempts
were deliberately stopped: the common interrupt breakpoint perturbed USB timing,
and a source-line breakpoint resolved to the wrong optimized location. Neither
is validation evidence; their logs remain separately retained.

The mutually exclusive branch path was inlined into main, reserving 7408 bytes
before calling the 2096-byte native probe, already exceeding the 8192-byte ABI
stack reservation before further callees. Branch edit/guard/address tables now
live in a heap workspace released on every run result; allocation failure has an
explicit test failure code. Main's compiled frame is now 1064 bytes. The source
compile gate `-Wframe-larger-than=2048 -Werror=frame-larger-than` rejects the old
source and accepts the fix (`old-frame.log`, `new-frame.log`). This is a fixture
correction, not a claim that sbrk dynamically prevents arbitrary stack overflow.

ARM application and all six launchers build. The HP49g+ CI-derived baseline and
retained HP50g baseline both pass missing-message removal, catalog/reference
checks, normal return, second-launch message query and unchanged Flash. Both
also pass all 88 branch/control cases with the heap workspace. Results are under
`/work/oct30-relocation-workspace/{hp49gplus,hp50g,branches}`. Full fourteen-case
matrices continue under `full-hp49gplus` and `full-hp50g`; do not infer their
completion from these targeted results. Formatting and the compiler regression
pass. No new remote CI acceptance or milestone completion is claimed.


## 2026-10-30 — Port C++ full SDK gate and additional PR26 failures

Port C++ local integration `3ab04c07` passes the full maintained SDK gate:
installed/relocated external C/C++ (including the owner example), public ARMv4T
assembly checks, six model-specific Notebook builds, three HP39 Aplet variants,
incremental missing-output regeneration and rejected model configurations.
Two builds produce 517 byte-identical outputs. Log:
`/work/oct30-port-cpp-sdk-full.log`; runner:
`research/oct30-port-cpp-sdk-full.py`. Prepared reset assets were byte-checked
against the generator and a fresh hp2aplet host tool compiled; temporary SDK/tool
workspaces are removed by the test. This is local SDK validation, not CI or M8.5
acceptance. New Object Basics application/all launchers also build.

Six-model Direct/Stock owner verification is running under
`/work/oct30-port-cpp-integration/runner`, with application/emulator/server hashes
in `runtime-inputs.json`. The first local wrapper incorrectly enabled keyboard
progression for Object Basics, while runtime-suites.json specifies false. All
three HP39 payloads passed but that wrapper run failed; its log is retained as
`runtime-keyboard-config-failure.log`. The corrected wrapper reads the maintained
suite flag and reruns. Do not count the failed wrapper run as an accepted gate.
The runner uses the separately tested PR29 retry-pacing server; this dependency
is explicit in the input hash and is not remote CI acceptance.

PR26 exact-head audit: 341 successful, one skipped, four running, thirty queued
and four failed jobs. HP50g job 123960552981 in run 41024016583 has the same first
missing-message ARM-abort snapshot as HP49g+: PC 0x10, CPSR 0x1d7, fault address
0x53000000. Its artifact is `research/oct30-pr26-hp50-relocation-failure`.

Console job 123962113880 belongs to run 41024024226 (not 41024016583). The latter
run's passing console artifact was initially downloaded, identified by its
returned/PASS evidence, and retained separately as `hp39-console-pass-control`.
The correctly matched failure artifact is `research/oct30-pr26-hp39-console-failure`.
It stops before payload entry at packet 36: 90 seconds without acknowledged
progress, three NAKs and four resends. All 1218 checked captured data/ACK/NAK
frames pass; the terminal Timeout frame also has valid length/sequence/CRC.
The passing control has 2505 checked frames without failures. These host capture
results do not establish the source of packet loss or accept a failed gate.
Audits are `research/oct30-pr26-console-{packet-audit,pass-packet-audit}.json`.


## 2026-10-30 — Complete workspace-fix matrices and delivery preparation

Both `oct30-relocation-workspace-full.py` processes complete successfully: all
fourteen call/return cases per model. Independent audit checks all 28 post-call
markers, eighteen original and ten rewritten returns, all 36 saved Flash images
and normal CPU state after every producing/query phase. A fresh temporary build
reproduces sixteen release artifacts byte-for-byte, including HPC1, stage1,
metadata and six launcher pairs. Source/artifact manifests and final audit are
`/work/oct30-relocation-workspace/{reproduction,verified}.json`. Both baseline
paths are explicit: HP49g+ uses the CI-derived baseline; HP50g uses the retained
maintained baseline. No full startup or hardware qualification is inferred.

Full tracked-file format/Ruff/ShellCheck pass in a temporary LF-normalized tree;
logs are `/work/oct30-relocation-quality`. ARM static analysis of the changed
branch workspace allocation/use/release also passes. Strict docs pass. The
previous exact-head CI was inspected before delivery: 370 successful, one
skipped, five failed, three running and one queued job. Job 123962117215 in the
PR workflow repeats the HP50g missing-message second-launch failure; its log is
`research/oct30-pr26-hp50-relocation-pr-failure.log`. It is not a new accepted
result. All old failure evidence remains retained; HP39g+ transfer failures are
not waived by the relocation correction.

### 2026-10-30 — M8.5 merged UI resource gate execution

The Port C++ integration source remains `3ab04c07`. All six Direct Object
Basics runs and Stock HP39g+/HP39gs/HP40gs runs pass. The same live process
continues to HP48gII and then HP49g+/HP50g; no restart or six-Stock acceptance
is inferred. Inputs and runtime evidence remain in
`/work/oct30-port-cpp-integration`.

The merged UI application and freshly patched pinned x50ng build pass, using
`research/oct30-port-cpp-ui-build.py`; source hashes and the build log are in
`/work/oct30-port-cpp-ui`. The fresh emulator contains both native-allocation
and resource-snapshot capture support. `oct30-port-cpp-ui-run.py` exercises the
actual combined CI verifier arguments (`--pool --allocation --native-release
--artifact`) and separately checks lifecycle snapshots. The first HP48gII run
is active, not accepted. Verified existing firmware assets and the separately
tested PR #29 Kermit server are reused; this local dependency does not imply
remote acceptance of PR #29 or parent PR #13. Application ownership, whole-UI
acceptance and M8.5 completion remain open.

### 2026-10-30 — Port C++ full runtime matrix and first integrated UI pass

`oct30-port-cpp-runtime-audit.py` passes after the six Direct/six Stock runner
finishes. `/work/oct30-port-cpp-integration/runtime-verified.json` records twelve
Stock reports, settled returns, all target completion markers and unchanged
application/emulator/Kermit hashes. Direct JSON files were replaced by Stock
reports; Direct acceptance retains the per-target log and successful family
completion, not independent retained Direct JSON. The actual owner fixture
exercises move assignment, release/re-adoption, independent snapshot digests
and errno-preserving destruction on the three classic Stock models. Other
contexts explicitly require ENOSYS.

Fresh merged UI integration passes on HP48gII, including both artifact-budget
and native-release checks plus 14 lifecycle snapshots. Native release identifies
6164 released block bytes, including 20 metadata bytes. The snapshot report confirms observed capacity
returns to 12540 bytes and largest free block to 11516 bytes. This is sampled
allocator evidence, not complete physical peak admission. Reports are under
`/work/oct30-port-cpp-ui/{runner/build,snapshots/hp48gii}`.

Two local-wrapper setup errors remain archived: `capture-disabled-*` lacks
CI's pool/allocation capture flags; `missing-firmware` is an explicitly stopped
HP49g+ attempt that lacked the nested firmware directory. The wrapper now sets
both capture flags and checks the shared verified firmware file before launch.
It retains the HP48gII pass and resumes HP49g+ with identical application and
emulator hashes. Remaining UI models and parent #13 integration remain open.

### 2026-10-30 — Native allocation parent reconciled with accepted main

Local parent #13 merge `19e19700` integrates accepted `3b77453a`, retaining
the later Aplet identity and Kermit retry-pacing fixes absent from the current
Port C++ branch. Conflict resolutions match the tested combined UI verifier,
workflow arguments and emulator capture-header set; the CLI regression requires
both image-budget and native-release checks. Eighteen host tests, Ruff and strict
docs pass. A fresh UI build reproduces fifteen delivery files exactly, including
the application, result address, memory layout and six raw/wrapped launchers.
Core runtime, platform, UI and emulator harness source identity also matches the
currently tested C++ integration. The Kermit patch/build/regression files match
PR #29. Six actual HP39 Aplet identities and six mixed-application negatives pass.
Evidence: `/work/oct30-allocation-main-integration/{verified,aplet-verified}.json`.

The first integration audit was stopped before checks to correct its artifact
list and avoid re-reading unrelated generated font assets. Subsequent attempts
identified inherited Python formatting and an incorrect audit-only stage1 path;
those logs are retained as `format-failure.log` and `audit-path-failure.log`.
The final audit passes; temporary builds are removed. No failed attempt counts
as acceptance. Parent #13 remains local/unpushed, and C++ still needs its ancestry
integrated. Fresh combined UI gates pass HP48gII, HP49g+ and HP50g; HP39 runs
remain active. Neither the parent nor M8.5 is remotely accepted.

### 2026-10-30 — C++ parent ancestry integrated

Local C++ merge `ed93a9bd` includes parent `19e19700`. Git confirms the parent
is an ancestor and no changes from tested `3ab04c07` in runtime, platform,
public headers, CMake, examples, Object Basics/UI sources or their build and
emulator preparation tools. The only non-document additions are the parent's
Aplet preflight and Kermit patch/regression/build files. Their sources match
the validated parent. Eighteen host tests, LF-normalized Ruff checks and strict
docs pass (`/work/oct30-cpp-parent-merge.log`); Windows checkout CRLF caused an
initial format-only diagnostic, with no source formatting change required.

Fresh combined UI verification now also passes HP39g+. HP39gs remains live;
HP40gs is pending. `oct30-cpp-ui-final-audit.py` is prepared to independently
recompute all six budgets, native-release checks and lifecycle reports once
the actual runner finishes; it has not yet passed. Parent #13 and C++ #14
remain unpushed. Previous #14 CI retains 319 successes, one skip and fourteen
cancellations; cancelled gates are not accepted. #26 remains queued.

### 2026-10-30 — Preserve pending evidence publishers

GitHub check annotation for job 121289451878 (run 40208342087, parent head
`4a4b36bd`) states that a higher-priority waiting request for
`ci-evidence-publisher` cancelled it. The job had no runner or executed steps.
The retained API evidence is `research/oct30-publisher-cancellation.json`.

Parent commit `700be00e`, merged into C++ as `80fdda5f`, adds `queue: max` to
both Runtime and Font Gallery publisher concurrency groups, retaining
`cancel-in-progress: false` and single-writer serialization. Parsed YAML checks
both groups exactly. [GitHub's concurrency documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency)
specifies up to 100 pending jobs instead of the default replacement of the
single pending job. Queue overflow can still cancel additional jobs; this
does not repair old cancellations or prove remote behavior before CI. No
application code changes. Fresh combined UI gates now pass HP39gs as well;
HP40gs remains active. Both delivery heads remain local and unpushed.

### 2026-10-30 — Complete combined UI matrix and delivery audit

The resumed UI runner finishes all six models. Independent audit
`oct30-cpp-ui-final-audit.py` recomputes 79 lifecycle snapshots, six linked
image budgets and native-release outcomes, including actual SRAM/Flash hashes
when an optional native block is present. Application and emulator hashes still
match the original run manifest. `/work/oct30-port-cpp-ui/independent-verified.json`
contains the final model reports. The first three-model reports survive the
local-wrapper corrections; neither invalid setup attempt is counted as a pass.

The Port C++ application/runtime/launcher/owner implementation remains identical
to tested `3ab04c07`; later ancestry and queue commits change transport checks,
publisher policy and documentation. Object Basics six Direct/six Stock, the
517-output installed SDK gate, sanitizer tests and ARM analysis retain their
recorded scope. Independent parent UI reproduction covers fifteen delivery files.
Before delivery, exact remote heads were inspected: #13 `4a4b36bd` has 371
successful, two skipped and one cancelled publisher job; #14 `492926e1` has
319 successful, one skipped and fourteen cancelled jobs. No cancelled gate is
accepted. New-head remote validation remains required, and M8 is incomplete.

### Saved startup configuration shape audit (2026-10-30)

The exploratory shape reader previously stopped at library 223 because its
configuration crosses a logical page. It now reads banked nibbles using the
retained per-page bank-switch maps, checks each entry's first physical address
against the inventory and verifies the unchanged input Flash digest. Both models
complete all fourteen configured roots. Libraries 226, 227 and 229 each contain
DOCOL, inline binary integer equal to the library ID, `0x7709`, and SEMI.
This focuses subsequent execution analysis on a shared entry; it does not prove
its effects, qualify startup or authorize Flash writes. Library 226's shape was
already known; this confirms 227 and 229 in both retained fixtures as well.
Evidence: `research/oct30-config-program-shapes.py` and container artifact
`/work/oct30-startup-config/program-shapes.json`. No product runtime change.

The follow-up runs the actual saved configuration bodies of libraries 226, 227
and 229 on both models in ordinary Stock. Each of six independent runs adds
exactly the expected existing HOME record again: `(226, 550234, 0)`,
`(227, 550264, 550249)` or `(229, 550294, 550279)`. Catalog and Flash are unchanged;
the child header, HOME sentinel and empty stack are checked. Six wrong-record
controls are rejected. Independent snapshot replay passes all six reports, and
the current C traversal recognizes 130 attachment edges;
130 unreadable null-field-nibble controls fail as required.
These tests establish ordinary attachment/reference behavior, not startup
safety, idempotence or complete execution-reference coverage. The copies run
from uploaded RAM; they do not prove execution from damaged Flash during boot.
Read-only ROM listings locate catalog lookup `0x843a`, indirect bank-switch call
`0x21c9e`, wrapper allocation `0x21840` and HOME insertion under `0x7719`.
Listings are exploratory and do not constitute a control-flow proof.
Evidence: `research/oct30-mutable-config-run.py`,
`research/oct30-mutable-config-audit.py`, container
`/work/oct30-mutable-config/independent-verified.json` and its six saved states.
No product runtime change or new CI submission is claimed.

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

Delivery preparation: functional commit `84fa1a70` passes full tracked-file
format/Ruff/ShellCheck and strict docs. Pre-push audit of `b8d38497` finds
ten queued workflows/16 queued jobs; no preceding CI acceptance is claimed.

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

## 2026-10-30 — C data-target device matrix complete locally

Both HP49g+ and HP50g pass all fourteen Stock call/return cases with mandatory
HOME data-target parsing, bounded-read and execution-edge rejection checks.
Independent `/work/oct30-bank-target/verified.json` validates 28 post-call
markers, eighteen original/ten rewritten returns, 36 unchanged Flash states
and sixteen byte-identical rebuilt artifacts. The audit explicitly checks the
new resolver, Flash data reader, code decoder and profile against reproduced
source identities. Ten ARM analyses, main-frame budget (1064 < 2048) and full
source format/Ruff/ShellCheck pass. This local result does not enable public
Port1 edits or close graph/execution qualification, startup safety or M8.0.

## 2026-10-30 — Bank-program CPU and return-slot prediction

Original-ROM experiments with distinct return cells showed that RTN clears
the consumed slot, correcting the initial assumption that all eight cells
survive. Added `hpclang_rpl_bank_cpu_predict`: configuration replaces C's low
five nibbles and D0; P/carry reset; RTN returns the popped address, clears its
cell and decrements the cursor. Other captured bytes survive. Invalid inputs
preserve output, including in-place-use and domain-rejection unit coverage.

Both maintained `bank_switch_maps.py` regressions pass with the new `bank_cpu.py`
comparison: fourteen populated switches, two P contexts, eight cursors and two
register patterns per model (896 cases total). All 144 CPU bytes and continuation
match actual original-ROM execution; Flash and seven unconsumed cells survive.
Unit ASan/UBSan, ARM compilation/analysis, formatting and Ruff pass. Evidence:
`/work/oct30-bank-cpu-effects/{hp49gplus,hp50g}/bank-switch-maps.json`; the earlier
independent observation/model comparison is in `verified.json`. The experiment
clears pending Saturn interrupts; no interrupt or execution-target qualification
is claimed. This follow-up is local, unsubmitted, and does not enable mutation.

## 2026-10-30 — Guarded native CPU capture

`hpclang_rpl_bank_cpu_capture_predict` captures the pending word and CPU slice
through native-relative reads, observing all 296 valid nibbles, checks actual P
against the prepared context and publishes only on success. Original dispatch
at 0x28898 loads native word 0xc24 before instruction execution; nonzero values
are rejected rather than treated as normal-return execution. Caller still
qualifies identity, configuration dependencies and exclusive lifetime.
All 296 read/callback failure positions, positive callback normalization,
32 pending-word bits and P mismatch preserve output under ASan/UBSan.
Both-model original-ROM comparison passes 896 captured/predicted CPU slices
with exact ordered dependency coverage. ARM compilation/analysis and Ruff pass.
Evidence: `/work/oct30-bank-cpu-effects/guarded-verified.json`. This local path
does not model interrupt execution or authorize movement. UI #28 `a181a6d2`
CI audit finds ten queued/two pending workflows and twenty queued jobs.

## 2026-10-30 — Combined bank mapping and CPU transition

Added `hpclang_rpl_bank_transition_prepare` with caller-owned planning storage.
It uses the same P and source snapshot for the existing map planner and guarded
CPU capture, preserving mapping IDs in dependency callbacks. Output publishes
only after both checks succeed; the continuation remains a separate unresolved
mapping/ownership/code obligation. No native writes or allocation are added.
Both maintained ROM regressions pass for P=0 populated entries and all reviewed
P=1 exits: sixty combined map/144-byte CPU/continuation matches, plus 240
pending-word, P-mismatch and signed callback-failure controls preserving the
complete output. The 896 varied-CPU comparisons also remain passing.
Evidence/source identities: `/work/oct30-bank-transition/both-p-hp49gplus/`
and `both-p-hp50g/bank-switch-maps.json`. ASan/UBSan unit, Ruff, ARM compile/
analysis pass; the combined ARM frame is 208 bytes. Changes remain local;
interrupt execution and public mutation authorization are not established.

## 2026-10-30 — Live Stock transition probe integration (in progress)

rom-banks.c now invokes the combined planner for every successful P=0/P=1
mapping probe (32 per application). The expected CPU result uses the original
native snapshot; the live source is neither seeded nor repaired. Pending
interrupts/P mismatch or invalid CPU state must reject, preserving the entire
sentinel-filled transition. Successful results must match the separate mapping
plan and CPU predictor. Original-ROM CPU comparisons remain the separate 896-case
gate; comparing two C paths here is not a replacement for that evidence.
Native state is checked byte-for-byte after the probes, under the existing IRQ
guard. The probe does not install mappings or authorize execution/mutation.

ARM application/six launchers, ten ARM analyses, full source quality and an
independent rebuild pass. All sixteen delivery artifacts reproduce identically;
main frame remains 1064 bytes. Evidence: /work/oct30-bank-transition-device/
reproduction.json and quality/. Both 14-case Stock matrices are running; first
native cases produced entry.json and normal post-call markers on both models.
No full-matrix result or remote acceptance is claimed. Changes remain local.

## 2026-10-30 — Combined-transition Stock completion and delivery

Both original Stock matrices completed normally: fourteen cases/model, sessions
14273 and 72742 exit 0. Independent current-source audit confirms 28 markers,
eighteen original/ten rewritten returns, 36 identical Flash snapshots and
sixteen independently reproduced delivery artifacts. Evidence is
/work/oct30-bank-transition-device/verified.json and reproduction.json.
The current live probe tests the real unmodified suspended CPU and preserves
its output on qualification failures; this does not mean all sampled launch
contexts qualify for execution. Both earlier maintained ROM results still match
all 26 recorded source identities, retaining 60 combined comparisons, 240 failure
controls and 896 varied-CPU comparisons. Ten ARM analyses, main-frame 1064-byte
budget, full source quality and strict docs pass.

Submitted nine implementation/test files as 2e7885a4 in PR #26. Immediately prior
21e0822e audit: two successful/nine queued workflows; 20 successful/224 queued jobs.
The new head was not polled in this submission turn. No milestone acceptance,
public Port1 mutation authority or safe startup claim is made.

## 2026-10-30 — Current status consolidation

The following is the previous roadmap status, preserved verbatim. Its queue states and active-process statements are historical, not current acceptance.

## Current work: M8.0 mainline and independent UI/C++ acceptance

The mainline remains safe movement and execution-reference coverage for native
library management and Port1 edits, followed by Port2 trusted recovery
installation, authority lifecycle, safe startup and reclamation. Sound
compatibility is independent.

M8.5 Port C++ `3b06b8c0` is submitted in PR #14 on allocation parent
`97d5ba74` (PR #13). Both await new-head CI acceptance: the latest #13 audit has two successful,
two running and eight queued workflows (70 jobs pass, fourteen run, 304 queue).
#14 retains ten queued workflows at its last audit. Neither branch is accepted.
All six Direct/six Stock Object Basics runs pass; live Port owners are exercised
on classic Stock models and other contexts require ENOSYS. All six Stock UI
budget/allocation/release/lifecycle gates also pass. Independent replay checks
79 lifecycle snapshots, six image budgets and six release outcomes against
unchanged application/emulator hashes. The installed SDK gate reproduces 517
outputs; the parent independently reproduces fifteen UI delivery files.
Parent integration retains Aplet identity and retry-pacing checks; eighteen
host tests, lint and strict docs pass. The publisher queue fix is locally checked.
Previous #13 CI retains one cancelled publisher; #14 retains cancelled gates.
Remote acceptance, whole-application peak admission and M8.5 completion remain open.

M8.5 migration tools, API guide and explicit-format hint `583f38e6` are
submitted in draft PR #27. The preceding `5d5195dc` audit found seven successful
and three queued workflows: 295 successful, fourteen running, 58 queued and one
cancelled job. HP39gs Stock Object Basics was cancelled during execution;
verification/upload did not complete and its log endpoint returned BlobNotFound.
The new head has not been polled this submission turn and is not accepted.
HP007 explains explicit legacy-format opt-in; six checker tests, full source
quality and strict docs pass. A fresh SDK installation relocated to a path with
spaces passes the installed-checker gate without changing its input. Earlier
six-model builds and 517-output reproduction remain evidence for the earlier
source. A local SDK documentation correction removes obsolete pending-CI
claims for accepted decimal formatting and KOS sound adapters; strict docs pass.
Ownership/application migration acceptance and M8.5 completion remain open.

M8.2/M8.3 stack, return-capacity and update-cost gates `4bcd3fb6` are submitted
in draft PR #28. The preceding `a181a6d2` audit found two successful and ten
queued workflows; 23 jobs passed, two ran and 133 queued. New-head CI was not
polled in the submission turn. The branch checks application SP, native capacity
recovery and fixed-workload processing ceilings, with optional ROM/launcher SP
diagnostics. Local evidence does not replace CI or whole-application acceptance.

The UI SP follow-up `79a5fe36` is submitted in PR #28; CI acceptance is pending.
All six Direct and six Stock UI/SP/return cases pass; independent replay
checks all 79 lifecycle snapshots, pool/artifact budgets and unchanged
application/emulator identities. Shared User/System stack use is
2424–2432 bytes of 8192. Nine host tests and full source-quality/strict
docs checks pass. The untouched 4 KiB control detects 4144 bytes while
the watermark stays 2424. CI, native/exception stacks and whole-system
peak admission remain open.

The optional all-PC SP observer in `a181a6d2` separates CPU modes and 1 MiB stack
address regions, including ROM/launcher assignments. Thirteen host tests, Ruff,
strict docs and a fresh emulator build pass. HP39g+ Direct UI/return and the
existing application SP gate pass on the unchanged application; additional
stage-region observations span `0x0131d3b0–0x0131dff8`. Independent
image/pool reconciliation measures 3152/4096 bytes for this Direct stage
stack, deepest in image decompression. HP39g+ Stock UI/return also passes;
reconciliation against its observed 34-page stage pool measures the same
3152/4096-byte stage stack. Full source quality checks pass. Native-stack
bounds and full peak admission remain open.

The M8.2/M8.3 update-cost gate submitted in `4bcd3fb6` caps the UI workload at
100 ms/frame and 5 ms/input edge. All twelve retained six-model Direct/Stock
timing records pass (maxima 50.416/1.708 ms); six verifier tests include exact
limits, over-limit failures and stale-pass removal. Ruff, Actionlint and strict
docs pass. This is saved-evidence revalidation, not new device execution or a
hardware latency guarantee. Whole-application resource acceptance remains open.


CI follow-up `fce14d73` is submitted in draft PR #29. Nine workflows now
pass; one is queued. The audit has 375 successful jobs, two skipped and one
queued job. The prior HP39gs Stock object-composite job failed in runner setup
when three Action v7 tags could not be resolved. All three tags now resolve;
the failed job was rerun and attempt 2 remains queued, not accepted. The local
retransmission-pause regression/replay passes; initial packet-loss origin remains
unproven and no gate is waived.

M8.0 `2e7885a4` is submitted in PR #26. It combines bank-map and guarded
CPU-return prediction from one P context and exclusive snapshot, publishing
neither output on failure. Pending interrupts or P mismatch reject. Both
maintained ROM regressions pass: 60 combined P=0/P=1 comparisons, 240 failure
controls and 896 varied-CPU comparisons; all 26 source identities per model
still match. The combined entry uses a 208-byte ARM frame.

The actual Stock probe leaves the suspended CPU unchanged and checks successful
predictions or complete output preservation on rejection. Both fourteen-case
Stock matrices pass locally. Independent audits confirm 28 return markers,
eighteen original/ten rewritten returns, 36 unchanged Flash snapshots and
sixteen reproduced artifacts. Ten ARM analyses, full source quality and strict
docs pass; main remains 1064 bytes. The pre-push audit of `21e0822e` found two
successful/nine queued workflows and 20 successful/224 queued jobs. New-head
CI was not polled in the submission turn. Interrupt execution, continuation
qualification and production mutation integration remain open.

M8.0 follow-up `9e220d9b` captures library-configuration read dependencies;
sanitisers, ARM analysis and both-model maintained regressions pass. It is included
in this delivery, but does not qualify startup execution.

PR #26 now includes the accepted resource-snapshot mainline, unified bank
planning, bounded live Stock readers, continuation mapping checks and startup
configuration regressions. Previous head `6a582958` failed CI; this delivery
adds the verified workspace fix and return-state checks, and awaits CI acceptance.

Local planner tests cover ROM identity, code/selector/routing/geometry guards,
full bank effects and unchanged switch-body physical mappings. Original-handler
comparisons, failure-output preservation and forty controlled HOME payload
resolutions pass; forty separate execution obligations remain open. The previous
live-reader build passed 28 Stock cases. The latest continuation-check build
now also passes all 28 cases locally: independent audits confirm every saved
planning witness, 18 original/ten fixture-rewritten returns, 36 unchanged Flash
snapshots and sixteen reproduced artifacts. Eight ARM analyses pass. This is
local validation; the planner follow-up still needs remote CI acceptance.

Complete execution roots and CONFIG semantics, production exclusive integration,
public library operations and Port1 edits remain open. ROM identity and a valid
predicted data mapping alone do not authorize mutation. Detailed tests, counts,
artifact identities and historical limitations are retained in the validation
history below.

| Work | Verified state | Remaining gate |
| --- | --- | --- |
| Native references and libraries | #25 accepted; #26 follow-up submitted. Guarded TEMP lookup and masked roots pass 304 controlled return cases/480 rejections, sanitizer/API checks and ARM builds. The corrected register-root build passes all 28 HP49g+/HP50g Stock call/catalog cases, with source/artifact hashes and unchanged Flash independently checked. This exercises default RPL3; explicit RPL4 masks retain host/controlled-fixture coverage only. | Complete launcher/opaque/banked provenance, full handler semantics, RAM continuations across exhausted-stack collection, library configuration/public management and a production banked-reference guard for Port1 replace/remove |
| Port0 growth / HOME snapshots | #24 fixes pass 54 six-model lifecycle phases and ten HP49g+/HP50g native library cases locally; sanitizers, ARM analysis, package checks and strict docs pass | Accepted in #24; retain lifecycle regressions |
| Port2 supported binding 769 | Both HP49g+ and HP50g pass guarded/repeated/absent recovery and all 16 rejection cases per model; independent saved-state checks pass | Complete trusted installation, authority lifecycle, safe startup and reclamation |
| Explicit recovery ID | Local primary `4c0f6044`, delivery `2dcb4aa5`; 73 recovery tests and Ruff pass on both; generated witnesses match retained native packages | Accepted in #25. HP49g+ used the preceding harness, HP50g continuation uses the explicit-ID helper |

Guarded ID769 tests retain 23,900 user elements. Earlier complete two-model
matrices used ID1701. Fourteen configured Stock libraries precede supported
ID769 among 59 lower-ID catalog entries in both saved fixtures. Their mapped
configuration objects are now resolved; IDs 226, 227 and 229 lie in mutable user
Flash, outside immutable-ROM identity coverage. Their configuration bodies invoke `0x7709`. All six ordinary-Stock executions
add the expected HOME record and preserve catalog/Flash/child/sentinel/stack;
current C traversal recognizes the resulting references. Startup execution,
bank-switch calls and allocation effects still require qualification. Maintained tests cover this baseline and the Stock
launcher fixture, which adds ARMToolbox 275: 60 entries, 15 configuration roots
and a fourth mutable-Flash configuration. Passing these checks does not prove startup safety.
All prior artifact-specific evidence and limitations are retained in
[M8 validation history](m8-validation-history.md).

Submitted M8.0 planning `84fa1a70` accepts explicit P=0/P=1 and retains
the existing P=0 entry point. Both-model original-interpreter comparisons
pass 32 P=1 page-map predictions and 96 unsupported-P rejection controls.
ASan/UBSan, ARM compile/analysis and current-source artifact checks pass.
This is mapping prediction, not arbitrary-bank restoration, startup
qualification or authorization for public storage mutation.
The mapped reader submitted in `21e0822e` consumes caller-qualified predicted
pages without installing them. Unit sanitizers, ARM checks and both-model
saved-state physical-read comparisons pass. The device probe now checks
32 P=0/P=1 plans per launch with unchanged native state; both 14-case Stock
matrices pass. Independent audits confirm 28 return markers, 36 unchanged
Flash snapshots and 16 reproduced artifacts.
The C data-target resolver submitted in `21e0822e` joins planning, bounded reads
and object spans, including observed mutable-Flash data. All 40 saved HOME
targets resolve and their execution edges remain rejected. Unit/ARM checks
pass. The device build passes ten ARM analyses and reproduces sixteen
delivery files byte-for-byte; its main frame remains 1064 bytes. Both
models pass all fourteen Stock cases. Independent replay verifies 28
return markers, 36 unchanged Flash states and sixteen reproduced
artifacts, binding the new resolver/reader sources. Full source quality
passes. CI acceptance and complete graph/execution policy remain open.

## Pending delivery

| PR / head | State |
| --- | --- |
| #26 / `2e7885a4` | Submitted; new-head CI not polled this turn. Combined map/CPU prediction and both complete Stock matrices pass locally. Remote acceptance remains open. |
| #26 previous head / `6a582958` | **Five failed jobs retained:** HP39g+ runtime/console pre-entry transfer, plus three HP49g+/HP50g relocation jobs across push/PR runs. Final pre-push audit: 370 successful, one skipped, five failed, three running and one queued job; nine successful, one running and one queued workflow. The workspace fix passes all 28 local call/return cases. HP39g+ packet-loss origin remains unproven; no failing gate is waived. |
| #25 / `752b8999` | **Accepted and merged as `3c484673`.** All eight workflows pass: 355 successful jobs and one intentionally skipped PR publisher. Push evidence publication succeeded. |
| #24 / `6ff8a1e9` | **Accepted and merged as `fee4e4b9`.** All 12 workflows pass: 390 successful jobs; two PR evidence publishers intentionally skipped. Follow-ups are tracked below. |
| #21 / `969ade88` | **CI accepted and merged into #20 as `1ce01859`.** Seven workflows pass: 319 successful jobs and one conditional PR evidence-publisher skip. This does not complete M8.2/M8.3 or accept the parent branch. |
| #18 / `439339b1` | **Three failed jobs remain unresolved.** HP49g+ Direct system-services fails (recorded PWM IRQ `0x46417b03`). HP40gs UI and HP39g+ notebook stall in pre-entry payload transfer. All 3,539 audited data/ACK/NAK frames pass length/sequence/CRC checks at the socat capture point; final device Timeout frames have invalid sequence fields but valid CRCs. Root causes remain unproven. |
| #20 / `4f2ce605` | **Accepted and merged as `3b77453a`.** All twelve workflows pass: 390 successful jobs and two conditional publisher skips. Resource snapshot delivery does not complete M8.2/M8.3. |

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

## 2026-10-30 — Guarded instruction fetch continuity (local)

Added hpclang_rpl_execution_instruction_preserved: framing at a caller-proven code boundary uses the real decoder, checks each nibble mapping before the source read and observes every valid consumed nibble. A cross-page instruction cannot pass by checking only its first address. Errors preserve length and invalidate partial dependencies; ownership, instruction semantics, interrupts and mutation authority remain separate obligations.

The rpl_code and rpl_execution_map CTest targets pass GCC ASan/UBSan in /work/oct30-continuation-fetch-host. Tests exercise a four-nibble instruction crossing two discontiguous physical pages, changed second-page mapping without reading that page, read failures and positive/negative observer failures at each position, truncation and malformed nibble rejection. ARM compile/static analysis pass using the established native-probe compile command. System Clang lacked host ASan libraries, so the installed GCC sanitizer runtime was used. No native execution or push of this follow-up is claimed.

## 2026-10-30 — Kermit retry pacing accepted

Exact PR #29 head fce14d7385878542a52fffce8a79280e9011d328 passed all ten workflows: 376 successful jobs, two intentional pull-request publisher skips. Full audit: research/oct30-pr29-final-ci.json. The formerly setup-failed HP39gs Stock object-composite retry passed. Marked ready and merged with exact-head protection; readback confirms merge 1c0d1e3544f2afa0e4c95518daa483383270fc22 at 15:25:05 UTC. No milestone completion or initial packet-loss root-cause claim.

## 2026-10-30 — Controlled ROM continuation fetch integration (local)

The maintained alias-selector gate retains its original thirty returns per model and adds thirty returns per model to the inspected bank-body first instruction. Original ROM execution must pop the return stack before the observer stops, preventing confusion with initial entry at the same address. The first attempt exposed that observer bug and is retained at /work/oct30-continuation-fetch-rom/hp49gplus.log; it did not pass.

Corrected runs in /work/oct30-continuation-fetch-rom-return pass on HP49g+ and HP50g. Independent verified.json confirms sixty original plus sixty known-boundary returns, 480 complete-output failure controls and 896 varied-CPU comparisons; all 26 source hashes per model and original SRAM/Flash identities match. Each added continuation frames P=0 and rejects a changed physical page before any instruction read. Ruff passes. These are controlled saved-state ROM regressions, not live arbitrary-return qualification or storage-mutation authorization.
