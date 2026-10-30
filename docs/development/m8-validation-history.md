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

## 2026-10-30 — Cooperative C++ timer ownership (local)

Added timer.hpp/cooperative_timer on the M8.5 Port C++ branch. It is move-only,
allocation-free and no-exceptions/no-RTTI C++17. Moving disarms the source;
replacement discards the old destination schedule. Chrono arguments reject
negative durations before sampling time; C clock/overflow failures retain the
schedule and poll output. Polling/armed/cancel keep the C cooperative semantics.
No background callback or hardware lease is introduced. The global sound API
is not wrapped as an owner: it lacks an identity token to prevent stale owners
from stopping replacement requests.

Two CTest targets (real C timer plus new C++ tests) pass ASan/UBSan under
/work/oct30-timer-cpp-host. Tests cover move construction/assignment/self-move,
failed replacement, overflow, missed periodic ticks, inactive no-clock polls,
one-shot consumption and cleanup errno. Ruff and strict docs pass. The new
installed examples/timer-cpp is included in relocated SDK builds, all six
launchers, optional three-model Aplet packaging and output reproduction. That
full SDK run is active in session 87805, log /work/oct30-timer-cpp-sdk.log; no
result or device acceptance is claimed. No push was made.

## 2026-10-30 — Timer SDK and Direct validation follow-up (local)

The full installed SDK run completed: 549 byte-identical outputs, six-model
Notebook and launcher builds, relocated C/C++ builds and three HP39 Aplet
variants pass. Log and source identities are recorded in
/work/oct30-timer-cpp-device/sdk-verified.json. The system-services application
now exercises actual-clock C++ timer move, one-shot expiry, periodic replacement,
cancel and destructor errno preservation alongside its existing gates. All six
Direct emulator cases pass; the original Stock matrix remains live in session
68475. No restart or reduced gate was used. Standalone timer-cpp example runtime
is not yet covered by this integration result.

The maintained analyzer passes five host/ARM source configurations (seven
compile entries before deduplication). cplusplus.Move is disabled only for the
two timer fixtures that deliberately query documented moved-from states;
production sources and other fixtures retain the checker. Eight analysis-tool
tests pass, including exception boundaries. The container requires explicit
GIT_DIR/GIT_WORK_TREE for the Windows worktree; no repository metadata changed.
M8.0 head 2e7885a4 was checked this turn: all ten listed workflows remain queued.
No new commit or push; no new milestone acceptance.

Timer independent replay: six Direct cases pass source/artifact SHA-256 checks, ARM call-site inspection, original input-pixel checks and settled-return fields. Evidence: /work/oct30-timer-cpp-device/runtime-audited.json. Stock matrix remains live; HP39g+ runtime/return passes in its log, with the HP39 family gate still pending. PR #29 run 41031458583 still has HP39gs object-composite job 124102178326 queued; no current failed jobs in that run.

Timer example runtime preparation: research/oct30-timer-example-build.py installs an SDK into a temporary directory, copies the unchanged public main.cpp and adds an external observer entry point. It checks stdout progression (including coalesced ticks), at least five seconds elapsed, final progress five and normal return. Build session 24467 is live; no execution claimed. research/oct30-timer-example-run.py prepares the ordinary six-model Direct gates. System-services Stock session 68475 remains live. M8.0 exact-head CI still lists ten queued workflows.

Installed example observer build completed (session 24467 exit 0): 55,576-byte payload, 1,912-byte static RW, six launchers; temporary SDK removed. Execution awaits existing Stock matrix to avoid concurrent emulator timing load. HP39gs now passes runtime and settled return; HP40gs is running. PR #13 Gallery run 41045600435 had only publisher 124108968421 cancelled before any steps; GitHub annotation says a higher priority waiting ci-evidence-publisher exists. Reran only that job; new job 124134000902 is pending. No gate waived or merge performed.

Example execution is now queued in session 57664 behind OS pidfd for confirmed Stock runner PID 85996 (session 68475). It checks command identity, waits for that process to terminate, requires six completed family/mode groups, runs the independent runtime audit, and only then starts six Direct example cases. A failed/incomplete predecessor stops the chain. No concurrent second emulator or restart.

## 2026-10-30 — Current status consolidation

The following is the previous roadmap status, preserved verbatim. Its queue states and active-process statements are historical, not current acceptance.

## Current work: M8.0 mainline and independent UI/C++ acceptance

The mainline remains safe movement and execution-reference coverage for native
library management and Port1 edits, followed by Port2 trusted recovery
installation, authority lifecycle, safe startup and reclamation. Sound
compatibility is independent.

M8.5 Port C++ `3b06b8c0` is submitted in PR #14 on allocation parent
`97d5ba74` (PR #13). Both await CI acceptance. The latest #14 audit has two successful, two running
and six queued workflows. #13 had five successful, one running, five queued and
one cancelled workflow: only its Gallery evidence publisher was cancelled by
a higher-priority waiting publisher. That job was rerun; attempt 2 is pending.
Neither branch is accepted.
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

Local M8.5 `cooperative_timer` owns a movable, noncopyable polling schedule
with chrono durations and no allocation/IRQ. Real C/C++ timer tests pass
ASan/UBSan. Installed/relocated SDK validation passes, reproducing 549 outputs
and all six model launchers plus three HP39 Aplet variants. System-services
integration passes all six Direct emulator cases; independent replay confirms
source/artifact identities, timer calls, input pixels and settled returns.
Stock execution remains in progress. Five host/ARM source configurations pass the maintained analyzer;
eight analyzer-tool tests pass. The Move-checker exception is confined to two
fixtures deliberately testing the documented empty moved-from state. The
standalone example runtime fixture now builds with the installed SDK, preserving
its main and checking output progression and elapsed time. Execution is queued
behind the live Stock process and requires its complete matrix plus independent
audit to pass first; HP39g+ and HP39gs runtime/return checks pass so far.
Changes remain local and M8.5 open.

M8.5 migration tools and API guide `5d5195dc` are submitted in draft PR #27;
CI has four successful, five queued and one running workflow; 159 jobs pass, 205 are queued and four run. Checker head `b59fb56d` passed the full relocated
SDK gate, six model-specific Notebook builds, three HP39 Aplet variants and 517
byte-identical release outputs. The later guide/header-comment follow-up passes
installation/relocation, links, strict ARMv4T C11 example compilation and docs.
Remaining ownership/application migration acceptance is still required;
this submission does not complete M8.5.

M8.2/M8.3 UI stack/return-capacity gate `a181a6d2` is submitted in draft
PR #28; ten workflows are queued/two pending and twenty jobs are queued.
The pre-push audit of preceding `79a5fe36`
found twelve queued workflows and twenty queued materialized jobs. The branch
checks application SP and native free-capacity recovery after settled return,
and adds optional native/launcher SP diagnostics. Local evidence below does
not replace CI acceptance or whole-application resource admission.

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


CI follow-up `fce14d73` is submitted in draft PR #29; four workflows pass and six are queued; 126 jobs pass, one is skipped, five run and 243 are queued. The failing #26 server lacks the previously tested retransmission pause. The exact downloaded CI binary fails the NAK-turnaround regression;
a fresh build with the isolated patch passes. Original packet CRC checks pass
at the host capture point; loss inside the emulator/device remains unproven.
The local replay with original CI payload/assets/emulator and only the server
replaced passes application execution, 52 keyboard frames and settled Stock
return. No failing remote gate is waived; the initial packet-loss cause remains
unproven.

M8.0 head `21e0822e` is submitted in PR #26, including bounded mapped
reads and C banked data-target resolution. CI has eight queued/three pending
workflows and eighteen queued jobs. The exact
preceding head `af7d7e8b` had eight queued/two pending workflows and sixteen
queued jobs at the pre-push audit. Both fourteen-case Stock matrices pass
locally, with 28 return markers, 36 unchanged Flash states and sixteen
independently reproduced artifacts. Ten ARM analyses, source quality and
strict docs pass; the main frame remains 1064 bytes. Complete graph/execution
policy and public mutation integration remain open.

A local transition planner now composes page-map and guarded CPU-return
predictions from one P context and exclusive snapshot, publishing only after
both succeed. Both maintained ROM regressions pass: 60 combined P=0/P=1
comparisons and 240 whole-output failure controls, alongside 896 varied-CPU
comparisons. Capture retains 296 nibble dependencies and rejects pending
interrupts or P mismatch. Unit sanitizers and ARM checks pass; the combined
entry uses a 208-byte frame. Interrupt execution, continuation qualification
and production integration remain open.


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
| #26 / `21e0822e` | Submitted; eight workflows queued/three pending and eighteen jobs queued. Bounded data-target resolution passes both-model Stock matrices and local quality gates. Remote acceptance remains open. |
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


Timer Stock HP39 family completed: HP39g+/39gs/40gs all pass original runtime, settled-return and physical-input gates. Independent runtime-audited.json verifies nine Direct/Stock cases with unchanged source/artifact hashes and the ARM timer call present. HP48gII is now running. Status consolidation preserves M0–M7 and stage contracts unchanged; all three maintained documentation copies pass strict Zensical build.

Local timer implementation committed as e0f167de (13 files); no push. Exact preceding remote head 3b06b8c0175e8528ed568f0b9abb325ce2e7f21c audit: two successful/three running/five queued workflows; 106 successful/eleven running/254 queued jobs, no failed or cancelled jobs. Complete audit retained in research/oct30-timer-prepush-ci.json. Local code and SDK/test documentation are committed separately from current-status/history edits. Stock and queued example sessions remain active.

Timer system-services matrix completed with exit 0: all six Direct and six Stock cases pass. Independent audit reports complete=true, twelve cases, unchanged source/artifact identities, ARM timer call present, original input-pixel gates and settled returns. Evidence: /work/oct30-timer-cpp-device/runtime-audited.json. Session 57664 then started the installed example Direct matrix; HP39g+ passed and remaining cases are running. This is local emulator evidence, not CI or hardware acceptance.

Installed example session 57664 completed with exit 0. All six Direct cases pass the unchanged example main under an external observer: strictly increasing stdout tick progress ending at five, at least five seconds elapsed, success return and settled emulator return. Independent replay validates every report, original source/ELF/HPC hashes and the real example call in disassembly. Evidence: /work/oct30-timer-example/runtime-audited.json. Standalone Stock and physical hardware coverage are not claimed. Final preceding-head audit: three successful/two running/five queued workflows, 122 successful/ten running/239 queued jobs, no failed/cancelled jobs.

Timer delivery pushed to PR #14 as d3e27922, including implementation e0f167de and consolidated status/evidence. PR title/body and remote head read back. No new-head CI poll or merge in the submission turn. The prior-head audit and all local gates above remain distinct from remote acceptance.

## 2026-10-30 — Continuation delivery and independent branch audit

PR #26 head `51b4566e` submits guarded instruction fetch and accepted main
`1c0d1e35`. The complete ARM application and six launchers build successfully.
The current-source ROM audit confirms, per model, 30 retained returns, 30 known
boundary returns, 240 failure controls, 448 CPU cases and 26 source hashes.
The prior-head CI audit found ten queued workflows and sixteen queued jobs;
new-head CI was not polled after pushing. Controlled returns do not qualify
arbitrary continuation provenance or complete M8.0.

Independent exact-head audits of PRs #13/#14/#27/#28 found no failed or cancelled
jobs, but each still has unfinished jobs. No branch was accepted or merged.
Detailed audit records are retained locally in research/oct30-independent-ci-*.json.

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

## 2026-10-31 — PR13 HP39gs format transport failure

Job 124103093154 in run 41045608034 fails before payload execution.
The downloaded Stock report has saw_pass=false and host-gate-failed; the
transfer gate records no acknowledged progress for 90s at send-window 54.
Independent wire audit checks 407 data/ACK/NAK frames with valid length and
CRC3. Retries already include 50ms pacing; build-ckermit.sh and its resend
patch match accepted main. This is not proof of the initial loss/rejection
cause or a format-runtime failure. Evidence: research/oct31-pr13-format-evidence
and research/oct31-pr13-format-packet-audit.json. Acceptance stays open.

## 2026-10-31 — Installed timer example HP39g+ Stock pass

The original HP39g+ Stock run completed successfully; only the driver then
failed by assuming the Direct-only result field. After terminal exit, retained
the original report and continued with the Stock saw_pass/returned/return_settled
checks. Independent audit passes all recorded source/artifact/firmware/runner
hashes, real example call and settled return. Evidence: /work/oct31-timer-example-stock/partial-audit.json.
Remaining five models continue in session 7681; no full matrix or hardware claim.
PR13 failed-job rerun request was rejected with HTTP403 because its workflow
is still running; original failure evidence remains unresolved.

## 2026-10-31 — Public SDK API usage conventions

Local c19618a8 adds a concise contract summary to the existing getting-started
page: result conventions, errno, versioned records, ownership, C++ cleanup and
capability-query boundaries. Existing UI provisional status is retained.
Strict documentation builds and a real temporary SDK installation pass; the
installed page is byte-identical and its migration link target is present.
The temporary install is removed. This is local documentation delivery, not
remote acceptance or completion of M8.5. Stock timer session 7681 remains live.

## 2026-10-31 — Second Stock timer result and obsolete CI queue cleanup

HP39gs completes the installed timer example and settled Stock return.
Independent partial audit validates both HP39g+ and HP39gs reports and all
recorded input hashes; HP40gs continues in the original session 7681.
Cancelled PR30 old-head bfddd250 runs 41085995365 and 41085995559 only after
checking every job was queued, runner_id=0, with no steps and a corresponding
current-head workflow present. Readback confirms cancellation. Other old runs
with execution evidence or missing current-head replacements were retained.
Evidence: research/oct31-obsolete-ci-cancelled.json. No acceptance is inferred.

## 2026-10-31 — Shipped timer example joins maintained E2E

Local c8ac990e links examples/timer-cpp/main.cpp with a renamed entry into the
existing system-services suite. Its observer checks monotonic progress to five,
at least five seconds elapsed and stdio cleanup; the existing runtime/return
and pixel gates remain. Workflow filters include example changes. ARM build
and all six launchers pass; timer-example.cpp and checks.c pass ARM analysis.
Actionlint passes excluding only its known unsupported queue keyword diagnostic.
Artifacts: /work/oct31-timer-example-integrated. Integrated runtime execution
and remote submission remain pending; the original standalone Stock matrix
continues unchanged in session 7681. Its results do not validate this new image.

## 2026-10-31 — Four Stock example passes and serial integrated validation

HP48gII completes the standalone installed example; independent audit now
passes HP39g+, HP39gs, HP40gs and HP48gII. HP49g+/HP50g continue in session 7681.
Session 21119 waits on the confirmed original driver PID 93996 and requires
its full six-model audit before starting the integrated system-services matrix.
Source/artifact identities are checked across the wait. No second emulator
starts during the standalone run. The linked ARM image contains calls to both
the observer and actual example; /work/oct31-timer-example-integrated/linked-example-audit.json
records the ELF hash. Integrated runtime and remote acceptance remain pending.

## 2026-10-31 — Complete standalone Stock timer matrix; integrated console isolation

All six standalone installed-example Stock cases and the full independent audit
pass. Session 7681 terminates successfully. Evidence: /work/oct31-timer-example-stock/independent-audit.json.
The subsequent integrated run terminates with HP39-family Direct timeouts: the
example hpconio constructor selects blocking legacy stdin, conflicting with
the existing unbound getchar/EOF checks. Retain failed evidence in
/work/oct31-timer-example-integrated. Local c6e801cd defines
HPCLANG_HPCONIO_NO_AUTOINIT only for the example source inside the explicit-stdio
observer fixture; shipped source and original EOF gates remain unchanged.
The v2 image and six launchers build, both changed translation units pass ARM
analysis, and HP39g+ Direct runtime/settled return passes. Session 65071 continues
remaining Direct/Stock checks under /work/oct31-timer-example-integrated-v2.
Full integrated and remote acceptance remain open; no hardware claim is made.

## 2026-10-31 — Integrated timer example six-model Direct acceptance

The corrected c6e801cd image passes all six Direct system-services cases.
Independent audit checks unchanged source/artifact/runner identities, original
physical-input pixel gates, settled return and linked calls to the observer
and actual example. /work/oct31-timer-example-integrated-v2/runtime-audited.json
records six cases with complete=false; the same session 65071 now runs Stock.
Full tracked/new-source formatting, Python/shell lint and strict docs pass.
Temporary quality sources are removed. Stock/remote acceptance remains pending.

## 2026-10-31 — Targeted retry after PR13 Runtime termination

Runtime 41045608034 reached completed/failure. GitHub accepted a single-job
retry of failed HP39gs hpgcc-format Stock job 124103093154 with HTTP 201;
readback is queued. Original transfer/packet evidence remains retained.
This is a diagnostic retry, not a claimed fix or milestone acceptance.
PR14 full-job audit hit HTTP 502 twice; its prior audit is retained and no
new acceptance or push follows this incomplete observation.

## 2026-10-31 — Integrated timer HP39-family Stock acceptance

The original v2 session 65071 completes HP39g+/HP39gs/HP40gs Stock.
Independent runtime-audited.json now records nine cases: all six Direct plus
three Stock, unchanged source/artifact/runner identities, original physical-input
pixels and settled return. Full matrix remains incomplete; the same process
continues HP48gII/HP49g+/HP50g. SDK documentation correction 2bc3ee69 passes
strict docs and actual installed getting-started byte comparison (9539, exit 0).

## 2026-10-31 — Complete integrated timer example matrix

Session 65071 exits 0 after all six Direct and six Stock cases. The independent
audit /work/oct31-timer-example-integrated-v2/runtime-audited.json records
12 cases and complete=true, validates unchanged source/artifact/runner hashes,
original input pixel gates and settled Stock return, and checks ARM calls to
the C++ timer fixture, example observer and actual shipped example. Original
failed console-selection evidence remains preserved. This completes the local
integrated-example matrix, not remote acceptance or the broader M8.5 stage.
