# hpclang roadmap

Current checkpoint: 2026-11-21. **M8.1 through M8.5 are accepted complete. M8.0 and M8.6 remain open.**

Accepted main: `67a84462` ([#52](https://github.com/maxmixalex/hpclang/pull/52)).
All three workflows passed on `93afbede` (21 successful jobs, one configured
skip). [#51](https://github.com/maxmixalex/hpclang/pull/51) added the HP49g+
independent Notebook save and relaunch regression.
[#50](https://github.com/maxmixalex/hpclang/pull/50) records production Notebook
release acceptance.

M8.6 local verification on HP49g+ and HP50g covers independent Notebook
launch from original ROM, saving `20` in HOME, a low-memory application exit
and normal relaunch from that state. Notebook exports the edit to a FAT16 card
and backs it up to Port 0. Both contain `202` while HOME retains `20`.
All 20 code pages map directly from Port 1, reducing aligned KOS usage from
21 to nine pages on normal launch. When a 35-page request is only partly
satisfied, the loader releases the blocks and returns to Stock. Both models
check reacquisition, GC and relaunch from that state. On HP48gII the independent launcher now runs the installed SDK's
64,264-byte Notebook in 29 pages owned by the executing program (one table,
16 code, 12 writable). Two Stock processes ran one USB-installed program. On recall the editor
read `314` from a Stock-written HOME note, displayed `31420` after keyboard
input and returned with status zero. Both return snapshots verified the code and
page mappings. After each exit, Stock GC moved a tagged temporary and preserved
its payload. The resident Notebook relaunched after the first collection. This
passed locally at `7b29e9f2` and is submitted for CI in #54. Application-originated
HOME save and GC during active execution remain open. A local HP48gII Save-button replay confirmed that both create and replace report ENOSYS. The independent loader supplies the Saturn state for reads but no RPL mutation bounds. Both launches returned normally; the existing HOME note retained 314. HP39 Aplet entry and wider native services remain open.

The [development board](https://github.com/maxmixalex/hpclang/issues/1) tracks
current work. Earlier accepted deliveries remain in the validation history.

Container housekeeping (2026-11-14): removed 109 obsolete CMake intermediate directories, 80 superseded host-test build directories and 104 untracked emulator object files, reclaiming 855.7 MiB in total from inside the container. The additional 1.6 MiB object-file cleanup retains sibling C sources, shared emulator binaries and directory aliases; report: /work/nov14-old-emulator-intermediates.json. Current 180-test ASan/UBSan validation, source trees and concise reports remain. No live-process or external-symlink dependencies were found; no archive was created. Old binaries require rebuilding. Reports: /work/nov14-cmake-cleanup-result.json, /work/nov14-old-host-cleanup-result.json and /work/nov14-old-oct-host-cleanup-result.json. Retain shared dependencies /work/oct7-next-probe/cpu/dist/runtime-lib and /work/oct25-watchdog-src/.cache/hp39-aplet. Earlier Windows download deletion remains blocked by execution policy.

Container cleanup (2026-11-13): removed /work/oct30-stage-stack-matrix/runner/build from inside the container after confirming its completed audit, current six-model replacement coverage and absence of active process/external symlink dependencies. Reclaimed 48,915,552 bytes without an archive; source scripts, concise reports, original inputs and shared emulators remain.

## Scope and retained requirements

Reach practical HPGCC application capability parity, then exceed it with one
LLVM-native SDK/ABI, owned services, modern C++, declarative C UI and cross-family
validation. M0–M7 provide the accepted foundation; they do not complete M8.

The previous roadmap and board snapshot are preserved in the
[frozen requirements/evidence archive](roadmap-history.md). **Requirements remain in force except for the user-approved deferral below.** This page shortens status reporting;
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
| M8.2 Declarative C UI core | **Accepted complete** under the approved resource scope | Preserve declarative identity/layout/input/effects and supported-flow resource regressions |
| M8.3 Components/renderers | **Accepted complete** under the approved resource scope | Preserve six-model rendering, low-memory recovery, budgets and clean return |
| M8.4 HPGCC source adapters | **Accepted complete** | Preserve original-source adapters, explicit migrations, tone/RTC ownership and supported-model regressions |
| M8.5 Modern C++/migration | **Accepted complete** | Preserve ownership/error/lifetime, migration tooling, installed C/C++ examples and deterministic resources |
| M8.6 Applications/release | Production Notebook release checks accepted | Complete the independent-launcher evaluation across three families |

Detailed stage contracts are preserved under [M8 stages](roadmap-history.md#m8-stages).
UI must retain the user's declarative C design: nested ordinary C declarations,
stable keyed identity, controlled inputs, explicit asynchronous state and
revision-driven canvas. A superficially similar imperative API is not completion.

## Approved resource scope — 2026-11-14

The user approved deferring comprehensive Stock exception-stack capacity and
whole-system worst-case transient-memory research. These are follow-up research,
not blockers for M8.2/M8.3 or application/release acceptance. Acceptance still
requires budgets and isolation for supported hpclang launch/run/return flows,
allocation-failure handling, low-memory recovery and correction of observed
faults. Measurements remain bounded-workload evidence, not universal bounds.
Avoid expanding marginal cases while ordinary supported flows need work.
This decision does not defer Port2 recovery/reclamation or change other M8 goals.
The subsequent acceptance audit confirms M8.2/M8.3 complete on main 2a65079f:
29 UI source/header/example/test paths match accepted 236e792b; all six Direct
and six Stock UI cases pass in Runtime 43130132986. Identity/layout/input/effects,
renderer/theme/font/canvas behavior, 36 KiB RW admission, allocation pressure,
reconstruction, stack/heap and update budgets, and settled return are covered.
This uses existing accepted execution plus source-identity reconciliation; no
new hardware or universal-memory-bound claim. #43 follow-ups are accepted on main 38e9472a.

## Current work: balanced M8 delivery

Rotate bounded delivery batches through M8.0, M8.2/M8.3, M8.4, M8.5 and
M8.6; retain M8.1 regressions. Finish or record a blocker before switching,
without expanding edge-case investigations. Other stages do not wait for
complete Port2 qualification. Apply the approved resource deferral below; other acceptance conditions remain
unchanged. Sound compatibility is independent of Port2.

CI (2026-11-19): #46 at `16369f3b` passed all five triggered workflows (184 successes; Pages deploy and runtime evidence publication skipped by design). Merged as `e45f8461`; remote main verified. Earlier HP39gs transfer stalls remain unexplained. No post-merge run is claimed.

M8.0 local: on HP49g+ and HP50g, the catalog relocation planner matched
Stock's complete library catalog after native deletion of a backup preceding
a registered Port 1 library. The [Port 1 observations](object-service.md)
cover the header movement and a separate attached-library run: both native
HOME cache pointers matched the C relocation plan on both models. The focused
host regression passes. General root collection and live execution remain open.

M8.0 / #47: merged as `516231c5`, whose tree matches accepted head `1cebf08d`.
CI: 184 successful jobs, including both HP49g+ and HP50g Stock code-relocation
jobs, and two configured publication skips. Local original-ROM runs at
`310e8200` rejected an injected 15-nibble gap before writes, then registered
library 123 using the real mapping. Native replay verified the new binding,
unchanged existing bindings, Stock return and unchanged Flash on both models
(`/work/nov19-registration-stock/independent-audit.json`). This accepts the
boundary fix only; public registration and production reference collection
remain open.

M8.6 local (HP50g Stock, 2026-11-19): the full instrumented Notebook editor
started and returned to settled Stock. Boot mapped 62 RW pages; heap reservation
peaked at 13,660 bytes and touched stack at 1,880 of 8,192 bytes. For this run,
20 code + 7 launcher/workspace + 8 RW pages cover the observed heap with the
reserved stack (35 pages; the packaging minimum before heap is 32). Entry RPL
bounds 560552..880707 leave 37 aligned virtual slots after the probe reserves.
Fresh emulator-side observation with the same instrumented application resolved
all 37 slots. For `i=0..36`, Saturn nibble address `0x8c000+i*0x2000`
maps to SRAM byte address `0x08046000+i*0x1000`. Read and Write agree on both
halves. No duplicate frames or other Saturn slots alias them. **All 37 already
back the application's live ARM RW mapping**. None overlap code, launcher-only
pages, the checked low SRAM/exception stack page, Saturn state page or the
three stage/application coarse tables. Additional capacity from these slots
on this production launch: zero. Do not combine them with the separate KOS
or paused RPL probe capacities. This does not establish a new launcher's layout
or a lease across Stock native calls. Local per-page audit:
`/work/nov19-notebook-map/audit.json`; editor, ON exit and settled return passed.
Evidence: `/work/nov19-notebook-occupancy/runner/build/hp50g-frame-memory-{1,2}.json`.

Independent HP50g admission test (local original ROM without library 275):
transferred the complete 79,168-byte plain Notebook HPC1 as `NBIMAGE` in HOME;
the returned snapshot reproduces the installed-SDK image byte-for-byte.
With that image resident, a fragmented KOS allocation trial held 8+8+4+1
distinct aligned pages simultaneously (21/35). At peak ownership, RSKTOP
555966 and DSKTOP 643021 permit only 9 aligned RPL candidates after reserves.
Even treating all nine as disjoint and usable would leave a five-page deficit
for this layout; no lease across Stock GC is inferred. Rejected admission
released all four blocks in reverse order; a fresh eight-page allocation reused
the first block. The probe returned to RPL with original control/SWI and Flash.
Per-page owners and acquire/release order: `/work/nov19-resident-kos-gap/proof.json`.
No Notebook code was entered: save, GC, application exit and reload are still
open. Repeating with the production 59,210-byte HPCZ encoding (verified to
expand to identical HPC1 bytes) keeps 21 KOS pages and exposes 14 distinct RPL
frames at peak, disjoint from the KOS frames in the live Read/Write maps.
This reaches 35 only numerically: no RPL frame is leased across a native
service. Next qualify reservation and restoration of this RPL region under GC
before entering the loader. Report: `/work/nov19-resident-kos-packed-map/proof.json`.

HP50g GC gate (local original ROM, no library 275): a stack-rooted TEMP
copy of the complete packed `NBIMAGE` does **not** pin physical frames.
From copies of the post-KOS-release snapshot, the no-GC control retained
its original address and payload at all 14 candidate frames. With Stock
GARBAGECOL, the retained object moved from nibble `0x87f60` to `0x87871`;
the updated root still read the complete original image, but all 13 whole
payload pages changed at their former physical addresses. On the partial
14th frame, 2,214 of 3,582 originally owned nibbles changed. DROP followed
by GC restored the original data-stack depth (2) and top value; the HOME
image and Flash matched the baseline. This tests release of the temporary
root, not combined cleanup with KOS blocks still held. Per-frame evidence:
`/work/nov19-root-lease-{control,gc}/pages.json`; release snapshot:
`/work/nov19-root-lease-release/run/hp50g/state`. Reject TEMP-root pinning;
full Notebook execution requires a GC-aware fence or a different page owner.

A separate local no-275 HP50g run held four KOS blocks (8+8+4+1 aligned
pages) while Stock GARBAGECOL moved a rooted witness from `0x87f8d` to
`0x87871`. The second ARM entry validated every block's allocated header,
footer and reciprocal links, the identity mapping and all 86,016 bytes of
page-specific data before release. An extra 1 MiB request returned NULL;
all four releases succeeded in reverse order. All four blocks were then
reacquired simultaneously at the same addresses and released again. Returned
RPL result, witness contents, packed HOME image and unchanged Flash were
independently checked. Ledger: `/work/nov19-kos-gc-cycle-v2/audit.json`.
KOS therefore covers 21 of the 35 editor-budget pages across this GC; the
other 14 still lack a qualified owner. The initial transfer trial did not
yield a verifiable completion marker and is excluded.

HP50g Port 1 plus KOS (local no-275 original ROM): Stock `STO` appended a
named `:1:NBLSE` backup containing a 60 KiB test string after two existing
records. Its payload contained 14 whole aligned ERAM frames `[0x08021000,
0x0802f000)`. The same 14 frame addresses and SHA-256 hashes were found
before and after a GC that moved the TEMP witness `0x87f8f` to `0x87871`.
All 21 concurrently held KOS frames were disjoint and passed the live owner,
identity mapping and 86,016-byte content checks. Thus 35 distinct frames
were accounted for during this GC. After the rejected 1 MiB allocation, all
KOS blocks were released and reacquired; a subsequent Stock `PURGE` removed
the test backup and restored the prior Port 1 chain and terminator. HOME
Notebook and Flash remained intact. Evidence: `/work/nov19-port1-lease/`
(`setup-pages.json`, `trial-pages.json`, `kos-audit.json` and release snapshot).
The ERAM payload stayed unchanged: using it as writable application memory
still requires preserving native backup integrity, preventing movement by Port
edits, and safe teardown on failure. No general Port 1 lease API or independent
Notebook run is accepted by this test.

HP50g split-code entry (local original ROM, no 275): independently entered
the plain installed-SDK Notebook (`_start`, VA `0x01220008`). Its 79,152-byte
code image used 14 user-read-only Port 1 frames plus six user-read-only KOS
frames, and eight KOS RW frames. This fixture stages the whole padded image
in an enlarged Port 1 backup; only its first 14 frames are mapped from there.
The first entry faulted on missing bootstrap table-view address `0x0121f010`.
With the read-only table view restored, ON was accepted and Notebook returned
zero. After restoring Stock mappings, GC moved a witness from `0x87f91` to
`0x87871`; the second entry remapped and checked the entire code image
(`0xd1729905`, FNV-1a). Snapshot verification passed the RPL return, unchanged
backup, rejected allocation, four KOS release/reallocation checks and Port 1
purge preserving earlier records. Report: `/work/nov19-split-enter-v2/audit.json`.
This run used the captured Saturn state but no native mutation cookie. GC
occurred between entries, not during an application save.

HP50g independent HOME save/relaunch (local, no library 275): the fixture
now passes the live Saturn map, main-stack fences, saved post-ARMSYS PC and
classified execution roots, and denies Port 1 edits. A direct HOME-program
trial wrote the note but failed on restart. Using a Stock TEMP copy of the
entry program (not as a backing store for application pages), physical keys
entered `20` and activated Save. A fresh emulator process resumed the saved
state and recalled the same program without retransferring the application.
The initially empty editor displayed `20` on relaunch; both native HOME
records matched `hpclang-notebook/1\n20` byte-for-byte. Both ON exits
returned the RPL witness; the two Stock frame captures one second apart
matched within each run. Both GC witnesses moved, the full code mapping
checksum and four KOS release/reuse checks passed, and the Port 1 backup
and Flash remained unchanged. GC was exercised **after** each application
exit with the frame leases still held. Evidence and per-launch frame hashes:
`/work/nov19-save-context-run/audit.json`. Local fixture evidence only;
production migration, other family qualification and release review remain open.

The repository regression in `tests/e2e/independent-notebook` now reproduces
this save and relaunch from a fresh HP50g firmware boot. In an offline clean
container the installed SDK built the Notebook; both processes returned to
Stock ROM, the native HOME bytes matched `hpclang-notebook/1\n20`, and the
second editor showed `20` without retransferring the program. The GC witness,
unchanged Port 1 region and stable Stock frames passed. The Core CI job runs
the same firmware test and publishes the phase report. Local validation
passed. The first [#48 CI attempt](https://github.com/maxmixalex/hpclang/actions/runs/44549769224/job/135650047342)
at `0c0fb778` stopped before the emulator: SDK install rejected the Git LFS
pointer for the font bundle. The job now requests an LFS checkout; remote
acceptance of that correction is pending.

HP48gII minimum-memory check (local original-ROM emulator, 2026-11-19):
the installed-SDK Notebook editor used a 35-frame production pool: 16 code,
12 RW and 7 stage/stack/MMU frames (4 KiB each). The 34 aligned RPL slots
between the observed bounds 554930..845814 after reserves all alias that pool:
16 code, 12 RW, 6 launcher, including one coarse-table frame. The separate
6,144-byte native allocation `[0x080053ac,0x08006bac)` contains no whole
aligned page. Neither source yields an additional exclusive frame. No other
SRAM is assumed free. The pre-heap packaging minimum is 27 pages; the editor
workload budget is 29 with 9,924 heap bytes and the full 8 KiB stack reserved.
The editor and settled return passed; a different allocator/entry layout
requires its own ownership evidence. Local capture: `/work/nov19-hp48-map/raw.txt`;
per-frame reconciliation: `/work/nov19-hp48-map/audit.json`.

Independent HP48gII admission (local, no library 275): the HP50g address
conversion copied the probe but jumped outside 256 KiB SRAM. Using the
HP48gII Stock main RAM map, the same bounded ARM probe executed and
returned `124275` to RPL, with caller control and the SWI vector restored.
With the complete 47,417-byte compressed Notebook in HOME, the tested
fragmented KOS requests acquired **one** aligned 4 KiB frame against the
29-frame editor budget. The probe checked allocation ownership and direct
address overlap; the later inventory also checks SRAM mirrors. The
contiguous 29-frame request was refused; the probe verified
the held data, released the block, reacquired it at the same address and
released it again. The HOME image decompresses to the exact SDK build and
Flash is unchanged. Report: `/work/nov19-hp48-entry-admission-mainmap/audit.json`.
This is a bounded clean rejection, not a general KOS capacity limit. No
Notebook entry or HOME mutation was attempted in this layout.

HP48gII image-absent control: the same ROM, probe binary and key sequence,
without transferring `NBIMAGE`, also yielded **one of 29 pages** (28 short).
The sole frame (`0x08005000`) had no direct address match in the Saturn maps;
the later physical audit found a mirror outside the RPL gap. The
122,879-byte contiguous allocation request was refused; all
4,096 bytes of the held page passed the probe's check. Stock released the
only block, reacquired it at the same pointer and released it again before
returning `124275` to RPL. The final HOME snapshot had no `NBIMAGE`; Flash and
the SWI vector were unchanged. In this paired test, removing the HOME image
did not improve KOS capacity. An application-sized page source remains open.
Evidence: `/work/nov19-hp48-entry-admission-control/audit.json`.

HP48gII read-only frame inventory: all 64 external SRAM frames are covered
by the Stock Saturn map in both saved return states. Eight frames appear
through the emulator's SRAM mirror, not a separate bank. In particular,
`0x08045000` aliases the already-counted KOS frame `0x08005000`. The model
has no Port 1 ERAM source. The observed TTBR and LCD buffer are in separate
on-chip SRAM, which is not counted as application memory.

After excluding observed native state, stacks, probe storage, allocator
metadata and mappings outside the reserved gap, the RPL gap contains 36
whole frames with `NBIMAGE` resident and 47 without it. Neither snapshot
proves a lease across Stock allocation or GC. The inspected KOS free lists
are also not a non-KOS owner. This read-only audit qualifies **zero additional
exclusive frames**, so the 28-page deficit remains. Inventory and input
hashes: `/work/nov19-hp48-page-ownership.json`. No emulator rerun, guest
memory writes or new container were needed.

HP48gII HOME-backed reservation trial (local original ROM, no library 275):
with `NBIMAGE` resident, Stock stored four test strings with 28 distinct
whole physical pages, of which only 16 overlap the 36 original gap
candidates. Creating one small HOME variable moved all four strings by
79 Saturn nibbles: **0 of 28** pages retained their contents at the original
physical addresses, although every logical string remained byte-identical.
A separate GC-only replay moved the rooted TEMP witness (`0x87f4e` to
`0x87828`) but kept the HOME object addresses and all 28 page hashes.
Reject this reservation mechanism for mixed Stock allocation and GC: no
additional frames are qualified. After the rejection, dropping the GC
witness and purging only test variables restored the original HOME/Port 0
bytes, read/write map and both stack slots. `NBIMAGE` and Flash matched
throughout; observed `AVMEM` recovered from 8,539 to 61,277 (baseline
61,138). This is logical cleanup, not byte-for-byte restoration of unused
RAM. Evidence: `/work/nov19-hp48-home-lease/comparison.json` and saved
setup, allocation, GC-only and cleanup snapshots.

HP48gII allocator-quota trial (local original ROM, no library 275):
with `NBIMAGE` in HOME, a disposable snapshot marked all 28 validated gap
frames and capped Stock `AVMEM` from 61,050 to 6,817 cells, below either
side's margin to the reservation. No HOME object owned the marked pages.
Stock did not preserve the cap: the USB-only control, with no HOME creation
or explicit GC, already restored credit to 61,050, and only 26 of 28 page
hashes matched. HOME creation and the separate GC replay also retained only
26 hashes; all phases changed `0x08010000` and `0x08011000`. GC moved the
TEMP witness from `0x87f1b` to `0x87828`. The USB-only control rules out
attributing those changes specifically to HOME creation or GC. Reject the
quota as a physical fence. Stock cleanup restored HOME/Port 0 bytes, stack
values and the Saturn map, and recovered `AVMEM` to 61,277 (baseline 61,138).
The harness then restored and checked all 28 original page contents in the
disposable snapshot; original inputs, `NBIMAGE` and Flash were unchanged.
No additional page is qualified. Per-phase hashes and cleanup evidence:
`/work/nov19-hp48-boundary-lease/`.

HP39g+ Aplet boundary (local D1.55 original-ROM emulator, 2026-11-19):
the installed-SDK Notebook reached its editor and returned to settled Stock.
The 37 physical frames comprise 12 code, 16 RW, 7 launcher/stack/MMU and
2 native Aplet workspace pages. After reserves, all 36 aligned RPL slots
alias these frames (12 code, 16 RW, 6 launcher including one coarse-table
frame, 2 native workspace). The separate 6,144-byte native block contains
no whole aligned page. Additional exclusive pages from those sources: zero.
Packaging requires at least 25 pages before heap; this editor workload budgets
27 including a 9,460-byte heap reservation and the full 8 KiB stack (1,384
bytes touched). A different Aplet entry/allocation layout requires separate
qualification. Per-slot evidence: `/work/nov19-hp39plus-map/audit.json`.

M8.6 local release check (2026-11-19, SDK from `e45f8461`): an offline
consumer container with no repository mount built plain and resource-observed
Notebook packages using the installed SDK. The observer links the unchanged
application sources. Each observed package passed original-ROM editor pixels,
ON exit and settled return. SHA-256 manifests checked before and after launch:
`/work/nov19-clean-stock/outputs/checks.json`. Table values are bytes.

| Family representative | Plain `.hp` bytes | Touched stack / reserved | Peak heap reservation / available |
| --- | ---: | ---: | ---: |
| HP 39gs | 48,404 | 1,384 / 8,192 | 9,460 / 48,024 |
| HP 48gII | 62,828 | 1,744 / 8,192 | 9,924 / 35,736 |
| HP 50g | 74,620 | 1,840 / 8,192 | 13,700 / 237,264 |

This covers clean SDK consumption, model packaging and the measured entry/exit
workload across three families. Resource measurements include observer overhead.
Backup/export, failure recovery, independent-launcher evaluation and release
documentation have separate acceptance records below.

**Production Notebook release checks accepted on main `b3d86f65` (2026-11-21).**
The audit rehashed six model packages and 15 outputs from two installation
prefixes, checked the three HP39 Aplet transfer sets, and reran the maintained
editor pixel, resource and Stock return verifier against captures for HP39gs,
HP48gII and HP50g. All nine save-failure, retry and relaunch reports on these
models recorded the expected phase and settled return. The installed transfer
instructions shipped in [#48](https://github.com/maxmixalex/hpclang/pull/48).
The evidence covers original-ROM emulation and the supported application flows.

The independent HP50g Notebook save/relaunch prototype and the documented
decision to retain the current launchers satisfy two launcher checklist items.
Cross-family independent page ownership, native service coexistence and exit
recovery remain open, including HP39 Aplet entry and the HP48gII memory budget.

**M8.6 release reconciliation (2026-11-19).** Against the original
[application and release checklist](roadmap-history.md#m86-application-and-release-acceptance):

| Requirement | Evidence and remaining boundary |
| --- | --- |
| Integrated application and native backup | Six-model Stock primary persistence and three classic-model Port 0 backup/restore/replace/remove have historical CI evidence. The local backup audit covers nine launches; restoring an edit preserves the primary save. See `/work/nov14-final-notebook-backup-native/independent-audit.json`. |
| Supported file export | HP49g+/HP50g original-ROM runs retain UTF-8 export across relaunch and refuse overwrite. Independent FAT image reads check the file and sentinel (`/work/nov14-open-stack-native/independent-audit.json`). Physical card testing remains a separate follow-up. |
| Installation, assets and user instructions | From a second prefix and new client/build paths, the **installed** Notebook README's `CMAKE_PREFIX_PATH` configuration and default build succeeded offline for all three representatives. Generated font C, HPC1, raw and wrapped launchers and memory JSON matched the first plain build byte-for-byte (15/15, `/release/documented-rebuild/manifest.json` in the release container). HP39 Aplet envelopes are covered by the earlier SDK reproducibility gate. |
| Resources, failures and restoration | Three-family entry/exit above; host sanitizer store/UI fault tests pass. Local HP39gs, HP48gII and HP50g Stock tests now also pass an injected replacement-string allocation failure: the Save action reports insufficient memory, retains the edit and rereads the prior native save. Retry persists the new note across exit/relaunch. All nine launch reports show settled Stock return. Reports: `/work/nov19-save-fault-<model>/runner/build/`. This covers pre-write allocation failure, not interrupted writes or power loss. |
| Public documentation | The [installed guide](../sdk/getting-started.md) and Notebook README now distinguish the HP39 Aplet bundle, HP48gII Port 0 setup and HP49g+/HP50g Port 2 setup, with transport and storage limits. Local review rebuilt all six fixed-model launchers and all three HP39 Aplets from the unpacked SDK ZIP, checked its manifest and 15 installed links, and matched three representative families' saved Stock transfer artifacts and settled-return reports. Plain package and instrumented run identities were checked separately. Evidence: `/work/nov19-release-docs-audit/audit.json`. No new ROM or device runs; this is the public packaging/transfer review, not release-wide acceptance. |
| Independent launcher and migration decision | [Three-family migration assessment](architecture.md#migration-decision-after-the-full-notebook-measurements-2026-11-19): retain production launchers. HP39g+, HP48gII and HP50g editor budgets are 27, 29 and 35 pages respectively; the measured RPL candidates add no frames to those production layouts. First qualify independently owned pages and native calls together on HP50g; model-specific entry, failure cleanup and six-model parity remain gates for retirement. |

| Stage / delivery | Current evidence | Next gate |
| --- | --- | --- |
| M8.0 / #31 | Internal Port0 registration and Port1 owned-tail edits pass local host/native gates. Accepted and merged as 2a65079f: exact head c662b236 passes all nine workflows, 219 jobs and three skips. This accepts the bounded implementation, not all M8.0 requirements. | Production reference collection, public library management and Port1 edits remain open. Test-owned-tail guards do not authorize existing-object edits; execution collection in call-roots.c also depends on fixture-specific ROM addresses and trace counts, so it cannot be promoted directly. This investigation is bounded here while other stages advance. Earlier HP39gs transfer failure passes exact replay, original cause unresolved. Evidence: /work/nov14-port1-owned-edit-native-v2/independent-audit.json. |
| M8.0 / Port2 | Both supported ID769 recovery/rejection matrices pass locally; explicit recovery-ID support is accepted in #25. | Trusted installation, authority lifecycle, startup and reclamation remain. Mutable-Flash configuration execution is unqualified; forty data targets retain separate execution obligations. |
| M8.2/M8.3 / UI | Core/component milestones remain accepted complete. #43 merged the saved-user-SP observer, low-SRAM pool exclusion and report-schema repair; all six Stock UI cases pass at 712e60c6. | Preserve supported-flow budgets, isolation and failure handling. Comprehensive Stock stack capacities and whole-system worst-case memory proof remain deferred. #44 delivery is included in #43. |
| M8.4 / compatibility #41 | **M8.4 accepted complete** on main 38e9472a: all five original requirements reconcile with the maintained adapter ledger, recorded original-source edits, KOS/RTC behavior and window/migration boundaries. #43 passes Storage and configured HPGCC/normal/exit/immediate-exit CI gates. | Retain source, signature and native regressions. Physical SD remains an approved nonblocking follow-up; six-model packaging does not imply SD availability. Final acceptance: docs/development/hpgcc-compat.md and docs/development/m8-validation-history.md. |
| M8.5 / ownership #14 | **M8.5 accepted complete** on main 38e9472a: the original three requirements are reconciled against implementation, host lifecycle/destructor tests, native exit checks and accepted installed-SDK CI. | Retain ownership/error/lifetime, no-exceptions/no-RTTI, C/C++ example and deterministic-resource regressions. Requirement map: docs/development/m8-validation-history.md (M8.5 final acceptance); local reconciliation: research/nov14-m85-final-acceptance.json. |
| M8.5 / migration #27 | Accepted within M8.5: installed migration guidance and read-only HP001–HP015 checker cover historical APIs, precision/ownership, target capabilities, text/fonts and raw hardware retirement. | Retain installed/relocated CLI, unchanged-input, encoding and incomplete-scan regressions. Evidence: /work/nov14-migration-window-lifetime.json and accepted Core run 43589087998. |
| M8.6 / applications #14 | #43 merged; all six Stock Notebook cases and configured backup/export/entry gates pass at 712e60c6. File-open buffer reduction and Port2 README correction are included. Local backup checks cover nine launches across three classic models. | Final release audit and independent-launcher evaluation remain. Earlier export stack is 6656/8192 bytes, relaunch 3712/8192; these are bounded-workload measurements. Evidence: /work/nov14-open-stack-native/independent-audit.json and /work/nov14-final-notebook-backup-native/independent-audit.json. |
| M8.6 / reproducibility #40 | Local KOS pool probe passes HP50g Stock. Follow-up now borrows/restores 28 main-RPL pages (112 KiB) with Stock/interrupts paused; saved-map, return and ten corruption checks pass. Default entry package remains unchanged. | Qualify combined ownership/fences and actual Notebook occupancy before claiming native-service coexistence. HP48/HP39 and release evaluation remain open. #46 merged; native snapshots remain local. |
| Shared Stock harness / #39 | 1db47ca3 fixes premature host readiness timeout while retaining the outer emulator timeout; four tests, old-behavior negative, Bash syntax and Ruff pass. The same fix is included in #14. | Accepted and merged as 7d1a5bb9 on 2026-11-14: exact-head 1db47ca3 passes all ten workflows, with 376 successful jobs and two skips. Preceding #14 failures remain recorded below. |

Current owner audit reports are
/work/nov06-sum/ui-owner-native-runs/independent-audit.json and
/work/nov06-sum/ui-owner-stock/independent-audit.json.
Completed timer validation includes six-model Direct/Stock integrated and
installed-example runs with independent artifact/pixel/return checks; its
branch acceptance passed in #14; integration into main is accepted through #43. Emulator results do not claim physical
hardware acceptance. Detailed historical results belong in the validation
history and PRs, not repeated per-turn narratives here.

## Pending delivery and unresolved failures

- #43 is accepted and merged for compatibility, ownership, migration, SDK
  distribution and applications. #40/#41/#44/#45 are included deliveries;
  acceptance comes from #43, not cancelled duplicate runs.
- #14 passed its allocation-branch CI (396 successful jobs, three skips);
  its main integration is now accepted through #43. This does not close all M8 stages.
- Earlier HP39gs/HP40gs USB transfer failures and HP49g+ system-services PWM IRQ
  failure remain historical unresolved observations. Passing exact replay or
  a later head does not establish the original cause.

Detailed accepted commits, earlier failures and validation scope remain in
[M8 validation history](m8-validation-history.md). All original completion
conditions remain in the [requirements archive](roadmap-history.md#m8-stages).

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
