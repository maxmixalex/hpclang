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

## 2026-10-30 — Explicit legacy-format migration hint

Local M8.5 follow-up adds HP007 for set_decimal_digits/set_decimal_separator.
It explains the user's explicit opt-in choice and the named-formatter alternative,
without claiming to prove preprocessor state or format-string compatibility.
Six checker tests pass, including comments/literals, identifier boundaries,
physical positions after line splicing and locally defined opt-in. A fresh CMake
SDK install, relocated to a path containing spaces, passes the updated standalone
checker gate (HP006/HP002/HP007, exit 1, unchanged source). The temporary install
was removed. Full tracked-file formatting/Ruff/ShellCheck and strict docs pass.
This follow-up is local, not submitted or accepted; the earlier 517-output full
SDK reproduction has not been rerun for it.

PR #27 exact-head audit 5d5195dc: seven successful/three queued workflows;
286 successful, 70 queued, eleven running and one cancelled job. Job 124020549105
(HP39gs Stock Object Basics) was cancelled in execution before verification and
artifact upload. Its log endpoint returns 404 BlobNotFound. No cause is inferred
and the incomplete gate is not waived. Audit: research/oct27-reserve-pr27-ci.json.

## 2026-10-30 — Migration hint delivery and CI setup retry

Submitted HP007 as 583f38e6 to existing PR #27 after auditing all preceding
5d5195dc workflows/jobs: seven successful/three queued workflows, 295 successful,
fourteen running, 58 queued and one cancelled job. New-head CI was not polled.
The local checker/installation/quality evidence above applies to this delivery;
full 517-output SDK reproduction remains tied to the preceding implementation.

PR #29 head fce14d73 had 374 successful/two skipped/one failed/one running jobs.
Job 124001018937 failed solely in runner setup: checkout, download-artifact and
upload-artifact v7 could not be resolved. All three refs resolve now via the
GitHub API. Retried that job; run 41031458583 attempt 2 is queued. No project test
executed in the failed job, no passing result is inferred, and no gate is waived.

## 2026-10-30 — Correct stale SDK compatibility status

GitHub confirms PR #17 merged as 88aae022 and PR #25 as 3c484673.
The migration branch's hpgcc-format.c/hpgcc-format.h and hpgcc-sound.c/kos.h
match their corresponding accepted implementations. Getting started and
migration still claimed their acceptance was pending. Removed those obsolete
statements, retaining API behavior and documenting Hz/milliseconds and the
unambiguous sound-status query. Pending UI/resource requirements are unchanged.
Strict documentation passes; this documentation correction remains local.

## 2026-10-30 — Current status consolidation

Historical submitted roadmap from migration head 583f38e6; superseded status, not current acceptance. All requirements are retained.

# hpclang roadmap

Current checkpoint: 2026-10-30. **M8 is incomplete; only M8.1 is accepted complete.**
Docker/WSL is restored. Earlier ID1701 two-model reservation retests pass guarded
recovery, repeat restart, absent recovery and 16 rejection cases per model.
Supported ID769 reservation matrices now pass on both models; full startup
safety and M8.0 remain incomplete.
Accepted main: `3b77453a` (resource snapshots #20, guarded dispatch/origin foundations #25, native catalog/HOME and Port0 fixes #24, physical spans #23, return roots and M0–M7 overview #22, SD Reset #15 and opt-in formatting #17 accepted). Local passing tests, submitted CI, CI acceptance and
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

## Current work: M8.0 mainline and independent M8.5 tooling

The mainline remains safe movement and execution-reference coverage for native
library management and Port1 edits, followed by Port2 trusted recovery
installation, authority lifecycle, safe startup and reclamation. Sound
compatibility is independent.

M8.5 migration checker `b59fb56d` is submitted in draft PR #27; ten workflows
and sixteen currently materialized jobs are queued. Local full relocated-SDK
validation now passes external C/C++ builds, all six model-specific Notebook
builds, three HP39 Aplet variants and 517 byte-identical release outputs.
Five focused tests and full formatting/static/documentation checks also pass.
A local installed API-contract guide now documents results, versioned outputs,
owners/views and exit behavior; relocated links and its ARMv4T C11 example pass.
This is not CI acceptance or completion of M8.5; remaining ownership/application
migration acceptance is still required.

M8.2/M8.3 UI return-capacity gate `8849b85f` is submitted in draft PR #28;
eight workflows and twelve materialized jobs are queued. It checks native free capacity and largest-block
recovery after settled return. Fifteen tests, all 79 retained snapshots across
six models, the actual CI commands and strict docs pass locally. This is not a
new emulator run or complete physical-memory admission; whole-application
resource acceptance remains open.

CI follow-up `fce14d73` is submitted in draft PR #29; ten workflows and sixteen
materialized jobs are queued. The failing #26 server lacks the previously tested retransmission pause. The exact downloaded CI binary fails the NAK-turnaround regression;
a fresh build with the isolated patch passes. Original packet CRC checks pass
at the host capture point; loss inside the emulator/device remains unproven.
The local replay with original CI payload/assets/emulator and only the server
replaced passes application execution, 52 keyboard frames and settled Stock
return. No failing remote gate is waived; the initial packet-loss cause remains
unproven.

M8.0 local follow-up `9e220d9b` captures library-configuration read dependencies;
sanitisers, ARM analysis and both-model maintained regressions pass. It is not
part of submitted head `6a582958` and does not qualify startup execution.

PR #26 now includes the accepted resource-snapshot mainline, unified bank
planning, bounded live Stock readers, continuation mapping checks and startup
configuration regressions. Head `6a582958` is submitted for CI and is not yet
accepted; its local Stock and maintained regression evidence is recorded below.

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
Flash, outside immutable-ROM identity coverage. Their execution dependencies
still require qualification. Maintained tests cover this baseline and the Stock
launcher fixture, which adds ARMToolbox 275: 60 entries, 15 configuration roots
and a fourth mutable-Flash configuration. Passing these checks does not prove startup safety.
All prior artifact-specific evidence and limitations are retained in
[M8 validation history](m8-validation-history.md).

## Pending delivery

| PR / head | State |
| --- | --- |
| #26 / `6a582958` | **CI failure, not accepted:** HP39g+ Stock runtime failed before payload entry while fetching packet 57. Audit: 228 successful, twenty running, 129 queued, one skipped and one failed job; seven successful/three queued/one running workflows. Earlier `bdeb3227` passed eleven workflows (379 successful jobs, two conditional skips). Historical HP39g+ pre-entry Kermit failure remains unexplained. |
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

## 2026-10-30 — SDK migration documentation delivery

PR #27 `f960876e` removes obsolete pending-acceptance claims for formatting and
KOS sound, whose implementation files match accepted commits 88aae022 and
3c484673. Strict documentation passes after preserving the branch roadmap
snapshot and resolving its history anchor. This is documentation delivery, not
new runtime validation. Prior exact head 583f38e6 has two successful/five queued
workflows and ten successful/78 queued jobs, none failed or cancelled. New-head
CI was not polled in the submission turn; M8.5 remains incomplete.

## 2026-10-30 — UI allocation failure matrix

Local commit 9a95726b expands renderer allocation failure injection to 131x64
and 131x80, each with 2/4/16 levels. Every failed construction preserves an
existing nonempty accepted image and allocation count; immediate successful
recreation/destruction restores the baseline. Normal declaration/layout/render/
copy checks zero allocation attempts, including rejected attempts. GCC ASan/UBSan
passes ui, ui_surface and ui_surface_alloc; changed C formatting passes.
This is host regression evidence, not whole-device peak memory or native-stack
acceptance. The commit is local and unpushed. Exact-head CI audit records are
research/oct30-independent-ci-26.json and research/oct30-independent-ci-28.json;
both still have unfinished gates and no failed/cancelled jobs.

## 2026-10-30 — Maintained launcher-stack verifier

Local 8c473f80 adds optional --stage-stack verification to the Stock UI gate.
It binds observed shared-bank launcher SP assignments to the actual native
pool, layout and payload; missing/malformed/duplicate traces, adjacent-region
crossings and exhausted reservations reject without leaving a stale report.
Six memory-verifier tests, Ruff/format and strict docs pass. Existing HP39g+
Stock trace replays at 3152/4096 bytes after verifying original runtime input
hashes and normal settled return; this is not a new device execution. Evidence:
/work/oct30-stack-regions/maintained-stage-gate-replay.json. Native/exception
stack bounds, system allocation peaks and six-model stage-trace coverage remain
open. Neither this commit nor 9a95726b has been pushed.

## 2026-10-30 — Stock stage-stack matrix started

Started five remaining models serially with the existing UI artifact and
all-PC observer; HP39g+ retains its independently rechecked prior trace. Runner:
research/oct30-stage-stack-matrix.py; evidence /work/oct30-stage-stack-matrix.
The active HP39gs emulator/process was observed live; no new pass is claimed.
The matrix requires original UI/SP/pool gates, maintained stage-stack gate and
normal settled return, with artifact/emulator/Kermit/assets hashes retained.

PR #13 publisher retry 124134000902 is terminal cancelled with no steps. GitHub
annotation confirms a higher-priority waiting ci-evidence-publisher request.
It is not a project-test failure or acceptance; repeated immediate retry is
deferred. Exact-head audits remain in research/oct30-independent-ci-*.json.

## 2026-10-30 — Deferred verifier cleanup during active matrix

A tested CLI stale-report cleanup patch is preserved in `research/oct30-stage-cli-cleanup.patch` for application after the active matrix; the verifier source remains identical to the run-start hash.
The added CLI regression proves a failed earlier UI gate removes an old stage
pass report; all seven memory tests passed with that patch. Both edited files
were then restored to their committed versions, and every active-run input hash
was independently rechecked unchanged. This patch is not committed or submitted.
The existing matrix process remains live; do not restart it.

## 2026-10-30 — UI allocation and stage-gate delivery

Pushed c59528cc to PR #28 with 9a95726b allocation rollback tests and 8c473f80
optional launcher-stack gate. Prior exact head 4bcd3fb6 has two successful/eight
queued workflows and sixteen successful/126 queued jobs, no failure/cancellation.
Ruff, strict docs and whitespace checks pass. New-head CI was not polled.
The live five-model Stock matrix still uses the original immutable verifier
source; the separate tested CLI cleanup patch remains deferred. No stage or
new device pass is inferred from submission.

## 2026-10-31 — Stock HP39gs stage acceptance and publisher queue repair

The existing serial matrix completed HP39gs: original UI/pool/application-SP
gates and normal settled Stock return pass, with launcher SP 3152/4096 bytes.
Inputs match their recorded hashes; report: /work/oct30-stage-stack-matrix/verified.json.
The same process continues HP40gs; remaining models are not claimed complete.

Draft PR #30, bfddd250, adds queue: max to both ci-evidence-publisher jobs.
GitHub documentation supports up to 100 waiting jobs with serial execution and
cancel-in-progress false. Both YAML structures pass. Actionlint 1.7.12 lacks
this new keyword; only that exact diagnostic was excluded to check the rest.
No test gates changed. Prior remote PR #28 c59528cc audit has four queued
workflows/six queued jobs, none failed/cancelled. New PR #30 CI not polled.
Existing cancelled runs retain old definitions and remain unaccepted.

## 2026-10-31 — Independent HP39gs/HP40gs stage audit

Both fresh Stock runs pass independent replay: UI pixels/themes/fonts, low-memory
reconstruction, application SP 2424/8192, launcher SP 3152/4096, observed pool
35 pages and normal settled return. Frame/input maxima are 14398/610 us and
19036/854 us respectively. Runtime inputs and retained evidence hashes match.
Auditor: research/oct31-stage-stack-matrix-audit.py; partial report in
/work/oct30-stage-stack-matrix/partial-independent-audit.json. Its default full
mode rejects the incomplete five-model matrix without producing acceptance.
Same session continues HP48gII, then HP49g+/HP50g. This is partial local
validation, not CI acceptance or whole-system memory/native-stack completion.

## 2026-10-31 — Completed Stock stage matrix and missing migration CI

The five-model matrix terminated successfully. Independent full audit passes
HP39gs/HP40gs/HP48gII/HP49g+/HP50g with original UI, pool, application-SP,
launch-stack and normal settled-return gates. All stage observations are
3152/4096 bytes; application SP is 2424 or 2432/8192. Previous HP39g+ evidence
is retained separately. Full audit: /work/oct30-stage-stack-matrix/independent-audit.json.
After process completion, applied deferred CLI stale-pass cleanup as local
b15ad2ff: seven regression tests, Ruff and normalized source formatting pass.
No new hardware/native-exception-stack or whole-system peak claim is made.

PR #27 f960876e had zero automatic check runs due docs-only path filters.
Manually dispatched quality/runtime/storage/objects/gallery/FatFs; all six
confirmed queued (41087137996, 41087144988, 41087152004, 41087159812,
41087167665, 41087174489). Core runtime has no workflow_dispatch, so that
current-head coverage remains open. No documentation deployment was requested.

## 2026-10-31 — Manual core validation entry submitted

PR #30 head 97d9eb46 adds workflow_dispatch to Core runtime. The unchanged job
has no push/PR-only event dependencies; Actionlint passes without exclusions.
This follows the docs-only PR #27 head having no automatic core check or manual
entry. The PR also retains publisher queue: max from bfddd250. Prior exact head
had four queued workflows/eight queued jobs, none failed/cancelled. New-head CI
was not polled after push. Mainline integration and PR #27 core coverage remain
open; no existing test, threshold, permissions or publishing policy was removed.

## 2026-10-31 — Migration Quality timeout and targeted retry

PR #27 exact head f960876ec9a57d2de8ef75dce30aa62f57438500 was audited
with all job pages. Before retry: 25 successful, three running, 148 queued
and one cancelled job. Quality job 124171152266 (run 41087137996) is
terminal cancelled. Its annotation reports the 20-minute execution limit;
recorded lint, format, analysis and cleanup steps report success. This does
not establish a successful workflow or explain the timing discrepancy.
No newer Quality run exists on the branch. A targeted job rerun was accepted
(HTTP 201); acceptance remains pending. PR #30 exact-head audit reports
16 successful, eight running and 157 queued jobs, with no failed/cancelled
jobs. Full audit records: research/oct30-independent-ci-27.json and -30.json.
No push or merge was performed.

## 2026-10-31 — C++ migration digit separators

Local migration commit 6d312ad5 fixes a reproduced false negative: two numeric
constants such as `1'000` and `2'000` caused intervening calls to be hidden as
a character literal. Preprocessing-number recognition now preserves those
diagnostics. All seven checker tests pass in the independent container venv,
including numeric bases, fractions/exponents, suffixes, literals, comments and
diagnostic positions. Ruff lint/format pass with cache disabled for the read-only
mount; git whitespace checks pass. Installed-SDK/runtime matrices were not rerun.
The fix is not pushed. Quality retry 124251710102 was confirmed queued.

## 2026-10-31 — Installed migration regression delivery

Commit a785c7e4 extends the maintained installed-SDK checker gate with C++
digit separators. Fresh CMake installation, relocation to a path containing
spaces, expected diagnostic categories, check-mode exit status and unchanged
external source all pass. Temporary installation was removed. Ruff and strict
documentation pass. This targeted gate does not rerun the complete ARM matrix.
Before submission, exact remote f960876e audit reports one successful/five
queued workflows; jobs: 29 successful, three running and 145 queued. No current
failed/cancelled jobs; earlier Quality timeout remains recorded above.
