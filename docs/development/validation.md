# Validation

## M7 final acceptance

M7 is complete within its individually approved scope. Functional head
`2988e1de` passes [Core](https://github.com/maxmixalex/hpclang/actions/runs/38184717670),
[Quality](https://github.com/maxmixalex/hpclang/actions/runs/38184717687),
[Runtime](https://github.com/maxmixalex/hpclang/actions/runs/38184717682),
[Objects](https://github.com/maxmixalex/hpclang/actions/runs/38184717711) and
[Storage](https://github.com/maxmixalex/hpclang/actions/runs/38184717704).
This accepts ordinary application termination, owned CPU power policy, the
historical timed client and the copied-Aplet memory repair, in addition to
the previously accepted services and capability ledger.

[Gallery](https://github.com/maxmixalex/hpclang/actions/runs/38160477819)
passed on `216bb5d5`; its path filter did not trigger on the final optional
cleanup-dispatch repair. This is retained prior-head evidence, not a Gallery
run on `2988e1de`. The earlier Runtime transfer failure also passed its rerun.
Final local acceptance includes 72 host tests, exit/stream sanitizers,
six-model Direct Runtime/System Services, six-model Stock System Services
and both object lifecycle suites, plus HP49g+/HP50g Direct/Stock exit-time
file persistence. Historical pending statements below are superseded by
this acceptance for the implemented scope.

Native Port 0/1/2 services, whole-calculator reset and reliable hardware
serial-number discovery retain their individually approved future goals.
The prepared HP49g+ physical SD test remains nonblocking and unexecuted;
emulator success does not establish physical hot-plug safety. Optional USB
stability measurements and transport reliability investigation remain open.
M8 retains broad source adapters, SDK delivery and higher-level UI work.

## M7.2 metadata / M7.3h–j / M7.5 integrated acceptance

Accepted on 2026-10-07 at functional head `6a7f9a28`. This integrated tree
contains the complete current M7.3 and M7.5 branches plus the M7.2 metadata
extension. All six workflows passed on that exact head:

- [Core](https://github.com/zyf722/hpclang/actions/runs/37607519230)
- [Quality](https://github.com/zyf722/hpclang/actions/runs/37607519197)
- [Runtime, System services and Display](https://github.com/zyf722/hpclang/actions/runs/37607519337)
- [Objects](https://github.com/zyf722/hpclang/actions/runs/37607519269)
- [Storage](https://github.com/zyf722/hpclang/actions/runs/37607519294)
- [Font Gallery](https://github.com/zyf722/hpclang/actions/runs/37607519262)

This closes formal acceptance of M7.2's metadata extension, M7.3h–j's
documented operations and approved port limitations, and M7.5's clean display
services and HPG feasibility witnesses. The integrated host suite has 54 tests.
Earlier pending-status statements below describe historical validation stages.
Native Port 0/1/2 services, broad M8 adapters and the remaining M7 parity ledger
and application-flow goals remain open. Physical-panel/audio claims remain
limited to their documented evidence. HP39 USB transfer throughput remains a
reliability follow-up: another branch's run exhausted the unchanged transfer
ceiling. A green integrated run does not establish complete determinism.

## M7.2 file metadata extension

Local acceptance on 2026-10-07 covers masked read-only/hidden/system/archive
mutation, creation and stored access-date queries, and the HPGCC metadata adapters.
Formal CI acceptance is complete on `6a7f9a28` (see above). The original 53 host tests pass. A real FatFs namespace
on an independently constructed FAT12 image checks distinct timestamps, leap-day
validation, absent/malformed dates, remount persistence, write protection, invalid
masks, missing paths and unsupported models. Access dates are stored metadata;
the runtime does not maintain atime on reads.

Storage passes Direct on all six models and Stock on HP49g+/HP50g. The payload
checks attributed creation, read-only write rejection, close/reopen persistence,
timestamp getters, mutation through an entry after its parent iterator closes,
masked preservation and cleanup. The vendor check reproduces the official FatFs
baseline plus the declared access-date extension without fuzzy patch application.

## M7.5 display runtime local acceptance

The `m75-display-runtime` implementation is locally validated on 2026-10-07;
formal CI acceptance is complete on `6a7f9a28`. The dedicated Display suite reuses Runtime's
Direct/Stock matrix and original launch/return gates.

| Model | Direct | Stock | Pixel/register checkpoints |
| --- | --- | --- | --- |
| HP39g+ D1.55 | Pass | Pass | 2/4/16 levels, both pages, six status segments |
| HP39gs | Pass | Pass | same independent checks |
| HP40gs | Pass | Pass | same independent checks |
| HP48gII | Pass | Pass | same independent checks |
| HP49g+ | Pass | Pass | same independent checks |
| HP50g | Pass | Pass | same independent checks |

Each payload exhausts and recovers its real heap before mode acquisition,
rejects invalid/busy calls, draws forward/reversed bars, exchanges distinct
physical pages, restores explicitly, then leaves a final gray16 session active
for automatic runtime cleanup. The host independently checks every captured
pixel, status segment and DMA layout. Direct also checks restored writable
LCD configuration; Stock requires the actual launch boundary and settled return.
The last PASS image is rendered in the owned mode, so cleanup is exercised after
PASS rather than hidden by a manual close in the test.

The 52 host tests include DMA mapping/allocator failures, LCD ordering, session
rollback, stalled clocks and the HPG clip/style/pattern/XPM feasibility witness.
The final transparent-pattern/origin-offset witness also passes host sanitizers
and the HP50g Stock payload. Existing Runtime's six-model Direct regression
passes with all 52 keyboard progression captures per target.
The large original Runtime payload also passes HP39g+ Stock with all 52 keyboard
captures and settled return, rechecking the earlier launcher memory-budget risk.
The common bootstrap provides a read-only L2-table alias without allocating an
extra physical page. Existing Runtime payload size remains within the same
15-page code allocation as the HP39 memory-budget fix (60,632-byte container).

Source review covers all four historical HPGCC distributions and newRPL's
HP50g LCD code, with S3C2410A register semantics governing DMA bank bounds,
read-only status, address prefetch and restore ordering. Accepted evidence here
is emulator/application correctness: it does not measure real LCD flicker,
luminance or electrical timing. There is no claim of recovery from arbitrary
CPU faults or power loss, nor of an installed broad HPG source adapter.

hpclang separates common-runtime correctness from calculator UI/package automation so a menu, transfer, launcher, or final repaint failure cannot be mistaken for an ABI/runtime regression.

## Continuous gates

| Gate | Purpose |
| --- | --- |
| `CI / Core runtime` | host/link/runtime invariants for the LLVM-native core |
| `CI / Font Assets` | deterministic generated-font catalog, source parity, and repertoire assertions |
| `CI / HP39 package` | build HP39g+/HP39gs/HP40gs hardware-validation packages |
| `E2E / Runtime` Direct families | boot target-appropriate firmware, execute canonical HPC1 through stage0/stage1/common bootstrap, observe PASS, and validate machine-state restoration |
| `E2E / Runtime` Stock families | drive original transfer/UI/ARMToolbox launch paths and require the same canonical runtime PASS artifact |
| `E2E / Font Gallery` | exhaustively traverse generated/native Gallery entries in Direct plus compact live-native Stock controls on HP50g and HP48gII |
| `Maintenance / CI` | trim Actions history and retained CI evidence |

Direct runtime has established green evidence on HP39g+, HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. HP39g+ model 1 boots the verified real-machine D1.55 full-NOR fixture, passes the strict 51-key Direct transition gate, and returns cleanly; no sibling firmware result is substituted. Its original-Stock USB Connectivity Kit install/launch is automated. All six profiles select USB for canonical Stock Runtime. Stock Gallery retains UART on HP39 and uses the shared USB paths on HP48gII/HP50g; Storage retains SD; see [Stock USB transport](stock-usb-transport.md) for the precise scope.

Historical fully-green canonical runtime regression evidence is **35832611353 / #357** on the final cleanup functional head `d33b3127`: Build, all three Direct families, Stock hp39, Stock hp48, Stock hp4950, and evidence publication all pass. This predates the USB migration. The M7.3a-e semantic closure gate remains **35812005032 / #338** on `ba2c0398`; #357 revalidates that same cross-family runtime/return contract after the subsequent cleanup, report-renderer consolidation, and workflow-dependency fixes. Runtime #122 remains the first run that closed the strict HP48 Stock return contract, while later superseded intermediate runs are retained only as historical diagnostics.

## Main follow-up

[Main Runtime](https://github.com/zyf722/hpclang/actions/runs/36322508885) at
`53856bc7` exposed another Stock HP50g launch failure after the integration
acceptance below. Its early captures already show ALG mode: the UI sequence
assumed a MODE toggle had succeeded, so subsequent RPN installation actions
could not work. The follow-up sets RPN explicitly through Stock Kermit's remote
command, clears documented system flag -95, and queries its cleared state before
releasing the host gate. The calculator executes the command; no RAM/ROM state
is patched. See HP's [system flag reference](https://hpcc.org/calculators/hp50g/hp50g_aur.pdf).

Local follow-up also exposed a separate keyboard capture race: reset published
KBRD before the caller drew its initial frame. KBRD now publishes on the first
poll after drawing, matching the existing post-draw KACK contract. A host test
checks both publication boundaries. Local verification passes 32 unit tests,
Clang analysis across the three compilation databases, full formatting/lint,
HP49g+/HP50g Stock USB with 52 frames each, HP39-family Direct with 52 frames
each, and HP50g Gallery shard 14.

Formal follow-up acceptance is complete on `fdcd38b78bac143aaff4ac76ed8845787cc18848`:

- [Runtime](https://github.com/zyf722/hpclang/actions/runs/36367636757), [Storage](https://github.com/zyf722/hpclang/actions/runs/36367636735), and [Font Gallery](https://github.com/zyf722/hpclang/actions/runs/36367636734) all pass.
- [Core](https://github.com/zyf722/hpclang/actions/runs/36367636756) passes.
- [Quality](https://github.com/zyf722/hpclang/actions/runs/36367636714) passes with lint, actual source formatting, and host/ARM Clang analysis all executed successfully.

This supersedes the main Runtime failure above. Branch retirement follows
validation of the fast-forwarded main head.

## Quality and workflow responsibilities

Quality runs three real gates: Ruff/ShellCheck, full owned-source formatting,
and Clang static analysis. Formatting uses the pinned ATfE clang-format plus
pinned Ruff. It never rewrites sources in CI. Vendor and generated font sources
are excluded because their upstream/generation tools own their formatting.

Clang analysis reads CMake compilation databases, rather than inventing include
paths, defines, or an ARM target. Host target detection comes from the compiler
recorded by CMake. The toolchain's own `clang-check --analyze` supports ATfE's
multilib layout; a generic clang-tidy package did not understand that layout even
with a matching major version. Analysis findings fail the job. The fixed-address
heuristic alone is disabled because this SDK intentionally accesses MMIO and ROM,
including runtime sources compiled in host tests. Null, bounds, lifetime, errno,
and dead-store analysis remain enabled. Assembly, generated sources, and external
x50ng build units are outside these compilation databases; their build/E2E gates
remain necessary. Static analysis is not a substitute for those tests.

| Workflow | Separate responsibility |
| --- | --- |
| Quality | Source lint, formatting, Clang analysis |
| Core | C and discovered Python unit tests, ARM build and hardware package |
| Runtime | Canonical ABI, keyboard, and original-system USB launch |
| Storage | Storage-enabled payload, filesystem fixtures, and SD regression |
| Font Gallery | Exhaustive font rendering and live Stock font controls |
| Font Assets / FatFs | Regeneration drift and vendor provenance |
| Documentation / Maintenance | Published documentation and evidence retention |

These gates retain distinct payloads and assertions. Their OS package installation
uses `tools/install-ci-packages.sh`; ATfE and x50ng setup remain shared actions.
Python semantic regressions belong to Core discovery, not one special named
Quality step. Model-specific UI sequences remain necessary where the real
calculators differ; required state must be established or witnessed rather than
inferred from an elapsed delay or a capture filename.

## Integration acceptance

The `integration/stock-usb-20260927` branch preserves `main` ancestry. Its
[Runtime #462](https://github.com/zyf722/hpclang/actions/runs/36305106569) passes,
but [Storage #210](https://github.com/zyf722/hpclang/actions/runs/36305106546)
timed out before HP50g payload execution and
[Gallery #154](https://github.com/zyf722/hpclang/actions/runs/36305106549)
failed HP50g Direct shard 14 at the pre-launch LCD capture. These failures are
historical integration failures, closed by the acceptance run below.

A controlled 800 ms host pause across the HP50g Storage launch key reproduces
the old timeout. x50ng uses host wall-clock deadlines: after a scheduling delay,
both key-down and key-up can expire before the guest runs. The shared event
queue preserves the minimum interval from actual delivery. The same paused
Storage reproduction passes with that change. Direct and Stock launch capture
also use a bounded two-second nonblank-LCD wait. This reproduces a failure
mechanism; it does not establish the exact host scheduling history of #210.

Local regression checks pass for all 30 host unit tests, the Kermit completion
tests, all six Direct runtime targets with 52 keyboard frames each, HP39g+
Stock USB with 52 frames and settled return, the paused HP50g Storage case,
and HP50g Direct Gallery shard 14. These focused checks do not replace the
complete CI matrix.

Formal cross-workflow acceptance is complete on the same functional commit
`12631370d69196a31a107cc2a57516e5c4803970`:

- [Runtime](https://github.com/zyf722/hpclang/actions/runs/36320408936): all Direct and Stock USB families pass.
- [Storage](https://github.com/zyf722/hpclang/actions/runs/36320408957): all jobs pass, including HP50g Stock.
- [Font Gallery](https://github.com/zyf722/hpclang/actions/runs/36320408833): the full Direct and Stock matrix passes.
- [Core runtime](https://github.com/zyf722/hpclang/actions/runs/36320408835) and [Quality](https://github.com/zyf722/hpclang/actions/runs/36320408847): both pass.

This closed the integration harness failures. The optional U5 repeated-run
benchmark and the later M7.3f acceptance below have separate evidence.

## Runtime PASS contract

The pre-integration USB baseline passes all six Direct and original-Stock USB
Runtime targets. Storage and Quality pass on the same functional head, and the
subsequent Gallery budget-only commit passes all Gallery jobs. Gallery Stock
still runs over UART/SD, while Storage explicitly uses SD.

Formal evidence: [Runtime #459](https://github.com/zyf722/hpclang/actions/runs/36286455537),
[Storage #208](https://github.com/zyf722/hpclang/actions/runs/36286455562),
[Quality #107](https://github.com/zyf722/hpclang/actions/runs/36286455548),
and [Gallery #151](https://github.com/zyf722/hpclang/actions/runs/36300460055).
The final commit changes only the Gallery job-time budget and documentation;
these are not four runs of one identical commit.

The canonical runtime test validates:

- initialized globals and zeroed BSS;
- static TLS and constructors;
- ARMv4T-safe integer and soft-float helpers;
- libc memory/string/formatting behavior;
- heap allocation;
- public display/font rendering used by visible result and keyboard screens;
- the complete 51-key physical keyboard/event path;
- hp4950 stream-storage behavior where supported;
- read-only calculator-object capability discovery: unsupported families/direct launchers reject the Stock-only stack view cleanly, while Stock HP48/HP4950 can observe depth and owned level-1 snapshots without changing stack depth;
- machine-readable `0x50415353` plus visible runtime evidence.

The Direct emulator gate also verifies that temporary mappings and the SWI vector are restored before the stock calculator resumes. Stable S3C2410 LCD-controller state is compared before harness cleanup so display-state damage cannot be hidden by host-side restoration.

## PASS, control-flow return, and visible return

These are separate claims and must remain separately observable:

1. **HPC1 PASS** proves the payload completed its assertions.
2. **Control-flow/state return** proves temporary hpclang mappings/state were removed and execution returned to the stock environment.
3. **Visible Stock return** proves the stock OS can process a normal UI action and redraw the LCD after return.

A nonblank `return.pgm` that still contains hpclang's last frame is insufficient for item 3. The harness must not manufacture success by copying the pre-launch framebuffer back. HP39 real-hardware validation uses **APLET** as the harmless post-return stock action.

## M7.3 object, conversion and stack-mutation evidence

M7.3 keeps Saturn addresses and mapped ARM aliases runtime-private. Public applications see opaque owned `struct hpclang_object` snapshots plus typed conversions and stack services. The object parser remains fail-closed: it snapshots only formats whose extent can be proven from verified fixed size, self-relative length, SEMI-terminated composite structure, tag+embedded-object structure, or another documented boundary rule.

The stack/object evidence progressed through these stages:

- **Core #285 (`35431683458`)** and the successful Stock hp48/hp4950 jobs in **Runtime #232 (`35431683496`)** established the first non-mutating `DSKTOP`/D1 + `EDITLINE` stack view, level-pointer resolution, proven-boundary parsing and owned snapshots;
- **Runtime #237 (`35435438471`)** is fully green after stage1 learned to dynamically recognize the hpclang Launcher-V3 DOCSTR and normalize the historical HPGCC 1.1 launcher level-1 bias, then restore it after HPC1 returns;
- **Core #296 (`35439583956`)** is fully green for the bounded mutation core and exact integer/raw-string conversions. In **Runtime #241 (`35439583995`)**, the canonical build, Direct hp39/hp48/hp4950, Stock hp48 and Stock hp4950 jobs all succeed. The Stock RPL witness constructs owned CHAR A/B snapshots inside HPC1, performs push → replace → peek → push → pop → drop, and requires the final user-visible depth to equal the initial depth. The Stock hp39 job was cancelled by a later push and is deliberately not counted as RPL-mutation evidence;
- on `87ce4349`, the Runtime/unit job in **Core #297 (`35440008729`)** passes the classic DOREAL conversion tests and ARM hardware-validation build, while the canonical-build job in **Runtime #242 (`35440008714`)** also succeeds.

Named-store and HP39 native-object validation then close the M7.3d read side:

- **Core #311 (`35452252332`)** and fully-green **Runtime #254 (`35452252452`)** on `9111b12` established the HP48/HP4950 read-only named-store implementation across Direct hp39/hp48/hp4950 and Stock hp39/hp48/hp4950. HP49g+/50g use the real USEROB `X50NG-VV.FAT` DOCSTR rather than the SD transfer file `HPCLANG.HP`: two independent HOME iterators must find it, classify it as a non-directory string, and `hpclang_object_store_get()` must return an owned zero-length string snapshot. The same path also checks `ENOTDIR` for child-open on the string and `ENOENT` for a missing name. The final HP48 witness was later reduced to the small transferred `ZZE2E` DOCSTR so object-copy validation does not depend on allocating an owned copy of the roughly 74 KiB launcher;
- HP39 Aplet-variable research after Runtime #254 recovered the current `ApletVars# = EIGHT` schema contract, preserved genuine extended-ID source references, and pinned the standard `NoteText` descriptor triple. **Runtime #304 (`35596158126`)** remains useful negative evidence: routing genuine `xNoteText #2C812` through ordinary Stock EVAL produces the same private `B9` result on HP39gs and HP40gs, proving that EVAL is not the generic descriptor-recall protocol;
- the successful path instead quotes the genuine extended variable ID into Stock and invokes Stock `@`, so the calculator follows descriptor word 0 through its own supported recall path. The HP39 portal enters with `GETPTR`, commits the final live RPL pointers with `SAVPTR`, and restores the suspended runstream pointer before returning to ARM. The **Stock / hp39 job in Runtime #320** validates this descriptor-driven recall on both HP39gs and HP40gs using `NoteText`: both reach HPC1 PASS, `stock-hp39.json` passes, and the complete 52-frame keyboard progression returns cleanly. Runtime #304 is therefore superseded as a blocker, but retained as evidence against the discarded EVAL shortcut;
- the HP39 side also has a strict private `HP39AscA` directory parser with unit coverage against the original HPGCC 39G-support `HP39DIR.000` / `HP39DIR.CUR` records and malformed/truncated/overflow cases. Those files remain typed transfer-protocol evidence rather than an internal HOME namespace. ROM analysis likewise keeps `DoRecv/GetOb`, `DoGetObFrSto`, `OpenIO` and `StdIOProc` classified as System-RPL/DOCOL transfer/I/O controllers rather than direct ARM object-fetch entry points;
- those historical gates established the read/stack baseline. M7.3f adds the public named-write and native-variable contract documented in [Object service](object-service.md#m73f-public-mutation-and-native-variable-lifetime); its final combined acceptance is tracked separately below.

M7.3e compatibility-adapter validation is closed above that read core:

- **Core #396 (`35739959095`)** is fully green with the first high-level
  HPGCC `satdir` adapter, safe `saturn.h` subset, calculator-String
  constructor, host units, unified ARM runtime build, and HP39 package build;
- later focused Core gates add historical textual numeric pushes, the exact ZINT
  constructor/exchange, the representative bundled HPGCC `stack.c` flow,
  classic REAL push, and failure-safe `stdargs`. Core #399/#400/#402/#404
  cover those steps through host units and ARMv4T hardware-validation builds;
- host units deliberately exercise malformed/truncated/overflow object extents
  and verify that failed ZINT/REAL typed pops and failed `stdargs` conversion
  do not consume the calculator stack;
- **Storage #125/#126** keep the normal Stock storage/return path green while
  the shared ARMToolbox stage-alias teardown monitor is made race-safe by
  recording the pre-PrRUN Stock L1 baseline;
- the final closure gate is **E2E / Runtime #338
  (`35812005032`) on `ba2c0398`**. Build, Direct hp39/hp48/hp4950,
  Stock hp39, Stock hp48, Stock hp4950, and evidence publication all pass.
  The focused Stock adapter witness preserves incoming stack depth across
  ZINT/REAL/String exchange and runs successful `stdargs` only on an
  initially empty stack. The existing canonical mutation witness independently
  restores push/replace/peek/pop/drop to the original depth;
- HP48's final successful named-store snapshot uses the bounded `ZZE2E`
  DOCSTR with payload `HPCLANG-E2E`. The transferred launcher remains visible
  to enumeration but is no longer used as an approximately 74 KiB owned-copy
  stress fixture. The long-running HP48 Kermit flake is closed at its actual
  synchronization boundary: the UI sequence now waits on a host marker written
  only after the Kermit process completes successfully, while the calculated
  timeout is merely a watchdog ceiling. This replaces the older fixed-delay
  workaround and prevents transfer/UI key races. The host sequence then proves
  stage baseline → active alias → HPC1 PASS → alias release → settled Stock
  return;
- HP39/40 canonical Runtime similarly gates the UI on actual serial-disk
  lazy-fetch completion. Sparse post-gate launch-only ENTER retries cover
  Stock's asynchronous install/UI settling; once a payload announces that it
  has started, the harness discards only pending retry key-downs while ordinary
  sequence input remains active. Runtime #338 therefore reaches HPC1 PASS and
  clean return on both HP39gs and HP40gs without changing the
  descriptor-recall/object implementation. Font Gallery #122 remains useful
  historical evidence for the older launch race; the Gallery path now shares
  the payload-aware retry mechanism. **Gallery #127 (`35822292175`)** first
  closes the root-cause fix; **Gallery #130 (`35832611436`)** is the final
  cleanup regression gate, revalidating the same all-green Direct/Stock matrix
  after the report renderer was folded into `tests/e2e/report.py` and the
  obsolete standalone renderer/path triggers were removed.

**Runtime #326 (`35739887799`)** is retained only as historical diagnostic
evidence and is superseded as the closure gate. Its pre-HPC1 launcher failures
and partial keyboard run are not adapter semantics; the later focused evidence
above resolves those harness paths instead of manufacturing success through
repeated unchanged reruns.

The mutation allocator does not trust Stock `AVMEM` as a complete physical-memory description. Stage1 maps hpclang-owned ARMToolbox pages back into the live Saturn address space and publishes conservative TEMPOB/user-stack fences through a tagged private platform cookie. Expected `ENOSPC`/invalid-level failures are checked before the first mutation. `AVMEM` is updated relative to the existing Stock reservation rather than recomputed from raw `DSKTOP-RSKTOP` geometry, so launcher/native reservations are not accidentally made available.

Conversions are explicit about representation loss: BINT/ZINT → `int64_t` is exact or `ERANGE`; classic DOREAL → `double` is approximate and returns `ERANGE` instead of silently clamping overflow or nonzero underflow to the old HPGCC behavior; CHAR and DOCSTR expose calculator 8-bit codes/bytes and are not mislabeled as UTF-8.

## Native-font validation and provenance

Calculator-native font bitmaps are borrowed stock assets and are never redistributed by hpclang. The common API deliberately hides two different platform mechanisms.

### HP48gII / HP49g+ / HP50g

Historical HPGCC calculator-native helpers resolve the minifont and bigfont through `sat_map_s2a()` and the live logical Saturn `Read[]` mapping. hpclang therefore keeps HP48/49/50 native lookup on the launch-preserved `SAT_CPU->Read[]` view. HPGCC3 explicitly defines bigfont as an eight-row raster with nominal advance 6; hpclang treats it as a concrete HP48/49/50 stock font, not a generic “large text” role.

### HP39gs / HP40gs

HP39/40 is a separate stock-ROM path. Historical HPGCC HP39 support binds stock `Put3x5` / `Put5x7` ROM entries, while independent ROM analysis establishes the raster tables themselves: mini-font physical Saturn base `0x07E695` and main-font base `0x07C651`. The HP39 UI reverse-engineering workspace independently establishes a third fixed 6×10 EditLine table at legacy base `0x07D3EA`, with 20 Saturn nibbles per character. This is a distinct ROM font, not the HP48/49/50 HPGCC bigfont.

The first physical-NOR implementation borrowed an apparently empty high L1 section descriptor. Diagnostics proved that descriptor storage could overlap the active LCD framebuffer and be overwritten by ordinary drawing. Production code therefore maps only physical NOR pages `0x0007e000` and `0x0007f000` as uncached 4 KiB small pages in the private prefix of hpclang's own code coarse table. The private virtual window is `0x01200000..0x01201FFF`; public HPC1 code still starts at `0x01220000`. The launch ABI keeps `platform_cookie` opaque to applications; current HP39 runtime code decodes a tagged private platform context whose validated `font_window_base` points at that window.

HP39g+ uses a separate physical layout, established below; it never borrows the gs offsets.

### HP39g+ native-font follow-up

The D1.55 physical full-NOR fixture places main, mini and EditLine at nibble
addresses `0x0DC096`, `0x0DE0DA` and `0x0DCE2F`. ASCII-run matching locates
main/EditLine; comparison of the complete tables confirms exact equality
with the reviewed HP39gs 2.22 full-NOR atlas, including the 155-record mini
atlas and its extended-character map. The launcher maps only pages
`0x0006e000` and `0x0006f000` into the existing private window.

The runtime now exposes all three concrete fonts on HP39g+. The oracle checks known
ASCII A/P/5/1 shapes and the previously established EditLine fingerprint
before comparing rendered pixels. This distinguishes genuine font identity
from agreement with an arbitrary ROM region. Local Direct shard 0 and compact
Stock USB Gallery pass all three native-font pixel oracles on HP39g+, HP39gs
and HP40gs. All three Stock runs reach application PASS and settled return;
the runtime, Stock and Gallery post-run assertions pass. The 54 host test
targets include ROM-identity/shifted-table rejection and three-model invalid
font-window checks. Clang analysis, formatting, workflow syntax and docs build
also pass.
The Stock Gallery now reuses the canonical USB install/launch harness and a
shared Gallery interaction suffix, adding HP39g+ to the CI matrix. Formal CI
acceptance is complete at `8f79c156`:
[Gallery](https://github.com/zyf722/hpclang/actions/runs/37739454659),
[Runtime](https://github.com/zyf722/hpclang/actions/runs/37739454651),
[Objects](https://github.com/zyf722/hpclang/actions/runs/37739454599),
[Storage](https://github.com/zyf722/hpclang/actions/runs/37739454593),
[Core](https://github.com/zyf722/hpclang/actions/runs/37739454634),
[Quality](https://github.com/zyf722/hpclang/actions/runs/37739454598) and
[FatFs vendor](https://github.com/zyf722/hpclang/actions/runs/37739454618) pass.
Storage's first attempt passed both HP49/50 application checks but failed to
upload evidence with a TLS error; the failed job passed on rerun. Exhaustive
generated-font coverage retains the existing 20-shard gate. HP39g+ composite
transfer also exceeded the unchanged 1200-second ceiling in local validation;
USB receive-notification/throughput reliability remains open despite this CI pass.

## Native ROM-font API contract

Native access is intentionally a small census of concrete stock fonts, not a semantic role layer. `hpclang_font_default()` remains a convenience selection. `hpclang_font_mini()` exposes the stock minifont. `hpclang_font_big()` is reserved for the historical HP48gII/49g+/50g bigfont and must be unavailable on the HP39 family rather than aliasing the 5×8 main raster. `hpclang_font_editline()` exposes the independently established HP39g+/39gs/40gs 6×10 EditLine tables and is unavailable on the HP48/49/50 family. HP39g+ uses its own D1.55 raster; its extension has a separate acceptance gate above.

The 2.22 HP39gs/40gs EditLine relocation is independently gated against stock NOR data rather than accepted from address arithmetic alone. HP39gs physical nibble `0x0FD882` and HP40gs `0x0FDA35` both match the legacy EditLine-table SHA-256 `f106efa599a5bb8a783f29cbc2dfa3afbace750975e783c223f53864a2945f53`; the Direct and Stock Gallery paths also pass the independent ROM-derived 6×10 pixel oracle.

Gallery **#103** on commit `0821c822` is the pre-change baseline, #111 is the first fully-green post-EditLine gate, and Gallery **#112** (`35298949457`) is the final M7.1 head gate after documentation consistency cleanup. HP39gs/40gs carry the additional concrete EditLine ROM font while HP48/49/50 retain their two-font native census.

## Final font validation closure

The current canonical source bundle is bitmap-only. The generated catalog is **245 faces / 1,639,485 glyph mappings / 3,472 assembly sources** with **245/245** source→generated parity. Font Assets **#24** regenerates the package without drift and reports zero exact duplicates, overflow, glyph-collapse, blank-heavy, or Powerline regressions.

The exhaustive Gallery is now family-dependent. HP39gs/40gs contain **248 entries: 245 generated + mini + main/default + EditLine**, rendering **496 captures**. HP48gII/49g+/50g contain **247 entries: 245 generated + minifont + bigfont/default**, rendering **494 captures**.

Formal production `E2E / Font Gallery` run **35298949457 / #112** is fully green:

- Build Font Gallery;
- universal x50ng;
- `Gallery / hp39`;
- `Gallery / hp48`;
- `Gallery / hp4950`;
- `Stock / hp48`;
- `Stock / hp4950`.

The Stock evidence was visually checked from the run artifacts. HP39gs/40gs now use four panels: calculator minifont, calculator main font, calculator EditLine font, then the generated control; both models pass the EditLine fingerprint and 6×10 pixel oracle. HP48gII/HP50g retain three panels: minifont, bigfont/default, generated control, and both reach settled PASS.

The Gallery also validates the production HP39 private two-page mapping, generated aliases/arbitrary face selection, arbitrary metrics/spacing, strengthened repertoire-sensitive specimens, and deterministic pagination. Earlier 242-face / 244-entry / 488-capture runs remain historical evidence but are no longer the production closure numbers.

## HP39 real-hardware gate

Use the target-specific hardware-validation artifact from `CI / HP39 package`. The transfer names are intentionally preserved:

- `HPGCC000.000`;
- `LIB275L.000`;
- `HP39DIR.000`;
- `HP39DIR.CUR`.

Transfer library 275 and the test aplet with Conn3x/HP Connectivity Kit, launch `HPCLANG HPC1 TEST`, and expect runtime PASS. After return, press APLET. A valid return opens/redraws the normal APLET library. A stale LCD page before that stock repaint is acceptable; a frozen keyboard, reset, ADDRESS ERROR, or persistent calculator-state damage is not.

The real HP39gs run has passed the complete canonical runtime and returned cleanly to the stock OS. HP39g+ now has its own verified model-1 full-NOR fixture; emulator E2E against that fixture, not the earlier hardware package or sibling ROMs, is the authority for model-1 runtime validation.

## HP39gs/40gs Stock emulator gate

`Stock / hp39` boots the verified HP39g+ D1.55 full-NOR fixture or the official HP39gs/40gs 2.22 ROM, transfers `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000` through the original USB path, launches the installed aplet through ARMToolbox39, and requires the canonical HPC1 PASS result. The gs/40gs UART serial-disk path remains an explicit override.

Closed-loop `KBRD -> KACK(n)` key synchronization is implemented for Stock as well as Direct. Runtime #135 first froze the exact 52-frame schedule: frame 0 is the initial state and frames 1..51 are emitted only after the preceding DOWN→UP cycle is acknowledged and the payload has accepted the key. Missing any frame fails the family; the final post-cleanup Runtime #357 repeats the fully-green Direct/Stock regression contract.

## HP48gII Direct and Stock gates

Direct boots the verified 2 MiB full-NOR image with 256 KiB RAM and a 131×64 display and validates the same target-independent HPC1 image.

Stock uses the verified NOR fixture, switches the factory-default Algebraic mode to RPN, selects USB wire I/O by default (UART remains an explicit fallback), starts the stock Kermit server, receives library 275 plus `HPCLANG.HP`, and launches through ARMToolbox48 / `PrRUN`.

HP48 Stock now reaches HPC1 PASS, control-flow restoration, temporary-stage teardown, and a genuinely stock-owned settled repaint. Runtime #122 is the first strict settled-return witness; Runtime #357 is the historical fully-green cross-family regression witness after M7.3a-e cleanup, and Gallery #130 independently retains the settled HP48 return around the live native-font path. The harness still retains `return-unsettled` as a failure label so future regressions cannot be silently reclassified.

## HP49g+/50g stream-storage and Stock gate

M7.2 keeps standard byte-stream I/O in picolibc, but the production filesystem backend is now hpclang-owned FatFs rather than Stock KOS file calls. The first storage operation acquires a whole-process SD namespace lease; from then until shutdown, stdio, metadata, capacity, rename/mkdir/rmdir and directory traversal all use the same direct S3C2410 SDI + FatFs stack.

KOS remains only at the Stock ownership boundary. The original implementation cached insertion and write-protect state before takeover because repeated `CHECKINSERT` is not return-safe on the HP49g+ path. The media-discovery follow-up replaces that cache with read-only GPIO sampling before controller acquisition: GPF3 reports insertion and GPD3 reports active-low protection. Unsupported pin configurations fail with `ENOTSUP` without changing Stock's configuration. On shutdown hpclang closes direct descriptors, unmounts FatFs, releases the controller, invokes KOS syscall 19 (`ResetSD`), and returns to Stock. Ordinary file data and namespace operations never alternate between KOS and FatFs within the process.

The follow-up has 55 passing host tests, including register-state transitions,
ownership guards and a real FatFs namespace retry after an absent-card failure.
Local six-model Direct Storage and HP49g+/HP50g SD Stock Storage pass,
including file operations and settled return. These runs validate integration;
insertion/protection transitions are simulated by host tests, not physical
hot-plug evidence. Formal CI acceptance is complete on `256e8496`: [Storage](https://github.com/zyf722/hpclang/actions/runs/37626827017), [Runtime](https://github.com/zyf722/hpclang/actions/runs/37626826775), [Objects](https://github.com/zyf722/hpclang/actions/runs/37626826929), [Core](https://github.com/zyf722/hpclang/actions/runs/37626827015) and [Quality](https://github.com/zyf722/hpclang/actions/runs/37626827014) all pass. Replacing an already mounted
card, rejecting stale handles and recovering ownership remain open M7 goals.

The focused Storage payload covers:

- missing-file `ENOENT`;
- create/truncate, read/write/flush, seek/tell, reopen and append;
- path size/type/portable FAT attributes and fixed last-modified civil time;
- free bytes and direct block-device total capacity;
- rename, mkdir and rmdir;
- UTF-8 long filename enumeration with preserved stored case;
- case-insensitive FAT lookup;
- two simultaneously live independent directory iterators;
- ordinary `fopen()` while an iterator is live;
- final stream access before runtime teardown;
- deterministic `ENOSYS` for HP39/40 and HP48 byte-stream operations.

Storage #22 (`35378151818`) remains the historical M7.2b evidence for the earlier KOS-backed metadata/stream bridge. Storage #23 independently established the Stock `ResetSD` reinitialization half. The later M7.2c runs, culminating in Storage #30, established the direct SDIO/FatFs backend, owned iterators, namespace mutation, capacity and process-wide lease. Final M7.2 closure is **Storage #53 (`35428824540`)** on `c4b827fe`: Build, Direct hp39/hp48/hp4950 and Stock hp4950 all pass after ordinary KOS file I/O is removed, mounted-volume capacity replaces whole-card capacity, the HPGCC filesystem adapter flow is exercised, and POSIX `ftruncate()` extension is zero-filled. **Core #254 (`35428824495`)** also passes on the same head.

The original timestamp fixture pins `HPCLANG.TST` to `2024-01-02 03:04:06`.
The accepted metadata extension also exposes validity-gated stored FAT creation
time and access date, with persistence and unsupported-case coverage. It does
not add timestamp setters or automatic access-date maintenance.

## Evidence retention

Machine-readable assertions remain the gate. Images are review/debug evidence rather than substitutes for assertions.

Canonical summaries are published to `ci-evidence` and standard Actions artifacts retain per-run evidence. Keyboard progression and Font Gallery contact sheets/summaries use separate paths. Runtime and Gallery use globally serialized publisher jobs, and cancelled runs do not enter that publisher queue; standard Actions artifacts remain the per-run fallback.


### M7.3f final implementation batch

The accepted pre-publication audit head is `71644639` (all CI green, confirmed
by the maintainer). The final implementation adds public put/remove, HP39
variable get/set, native type validation and error trapping, GC-safe portal
storage, and a focused HP39 mutation matrix. Runtime keeps its complete service
and 52-frame keyboard coverage.

The combined implementation was locally validated on 2026-10-02 with the pinned
toolchain and the repository's CI build/harness scripts:

| Gate | Result |
| --- | --- |
| Host units | All 34 pass, including public mutation modes, native-arena pinning, moved/error stack cleanup and MMU workspace preservation |
| Quality | Formatter check, lint and Clang analysis pass |
| Object mutation | All six Stock models pass through USB, including HP39 growth/shrink/empty writes across observed native GC and strict settled return |
| Runtime | All six Direct and all six Stock targets pass, including each complete 52-frame keyboard sequence |
| Storage regression | HP49g+/50g Direct and Stock pass, including native SD teardown and settled return |
| Gallery regression | HP39gs Stock passes the independent native-font pixel oracle and settled return |
| Build/package | Hardware validation builds and all three HP39-family packages succeed; documentation builds without issues |

The bootstrap now leaves two HP39 pool pages available to Stock, uses one page
for its C stack, and retains PIC return code in the unused half of the L2
workspace. Runtime, Storage and Gallery regressions cover this shared layout;
the MMU unit also checks that table operations preserve the return-code area.

The implementation and local acceptance scope are complete. The final CI
acceptance below closes M7.3f, including the GC and settled-return gates.

On `3ddbab3a`, Core, Quality, Storage, Font Gallery and the complete
[Runtime matrix](https://github.com/zyf722/hpclang/actions/runs/37004261685)
passed formal CI. [Object mutation](https://github.com/zyf722/hpclang/actions/runs/37004261635)
passed HP39gs/40gs and HP48/49/50; HP39g+ failed before emulator startup because
the new Stock job did not fetch its Git LFS ROM fixture. The pointer text
reproduces the reported XZ decode error; the resolved fixture passes the existing
NOR preparation checks. Stock mutation checkout now enables LFS, matching Runtime.
The corrected [Object mutation run](https://github.com/zyf722/hpclang/actions/runs/37007395058)
on `8348c142` is fully green across all six Stock models. Together with the
unchanged implementation gates on `3ddbab3a`, this completes formal acceptance
and closes **M7.3f**. Overall M7.3 remains open for the additional capabilities in
[Roadmap](roadmap.md).

## Main integration acceptance — M7.3f

Main `d1dd4688` passed [Runtime](https://github.com/zyf722/hpclang/actions/runs/37008732124),
[Object mutation](https://github.com/zyf722/hpclang/actions/runs/37008732154),
[Core](https://github.com/zyf722/hpclang/actions/runs/37008732178),
[Quality](https://github.com/zyf722/hpclang/actions/runs/37008732116),
[Font Gallery](https://github.com/zyf722/hpclang/actions/runs/37008732111),
[Docs](https://github.com/zyf722/hpclang/actions/runs/37008732108), and
[Storage](https://github.com/zyf722/hpclang/actions/runs/37008732176).
Storage required an unchanged failed-job rerun after a Stock HP49/50 launch
timeout. The same CI payload passed local reproduction; the root cause remains
unconfirmed. This is retained as a reliability investigation alongside U5,
not evidence that a timeout defect was fixed.

These gates accept M7.3a–f for their implemented scope. Directory operations,
Aplet-owned persistence, composite objects and native namespace coverage remain
open under M7.3g–j in [Roadmap](roadmap.md).

## M7.3g local directory-service acceptance

The directory implementation in `b2a69739` passed local validation on 2026-10-03:

- all 34 host unit tests, including directory lifecycle, retained interior
  references, active cwd/ancestor rejection and failure-before-write cases;
- Clang analysis across host, Runtime, Font Gallery and Object mutation
  compilation configurations; formatting and documentation builds;
- the focused Object mutation Stock matrix on all six models, using the shared
  build/asset-preparation/USB harness and settled-return gates. HP48gII/49g+/50g
  execute the new directory lifecycle; HP39g+/39gs/40gs revalidate native-variable
  mutation with the same rebuilt payload and freshly prepared Aplet packages.

Subsequent formal Object mutation regressions, including functional head
`06ab4deb` and main `3ad37ac6`, execute this directory lifecycle on all three
classic Stock models and pass with settled return. This closes M7.3g.
M7.3h–j remain separate acceptance items.

## M7.3h local Aplet-data regression

Local validation on 2026-10-03 covers the in-progress Aplet data service and
the shared stage mapping correction:

- 35 host tests, Clang analysis across the four compilation configurations,
  formatting, lint and documentation builds;
- Stock Object mutation on all six models; each HP39 model also completes
  eight successive invocations with checked write/read/clear phases, native GC,
  a Stock SAVE copy, per-instance isolation and settled return between invocations;
- Direct Runtime on all six models, including keyboard transitions;
- HP50g Stock Storage with the workflow's SD transport and storage witness.

The HP39g+ fourth-invocation abort was traced to a Stock IRQ byte store that
overwrote the former stage descriptor at `0x07f00240`. The stage now uses the
shared private mapping defined in `stage-layout.h` and restores its previous
descriptor on exit. The rebuilt payload passes the previously failing sequence.

The per-family Object mutation builds retain all applicable probes while
reducing HP39 code from about 45 KB to 31 KB, allowing two test Aplets to coexist.
This exposed a separate HP39g+ corruption path in the historical splash: the
46-character author description produces a negative drawing coordinate. A short
author credit fixes the package; all three HP39 models pass the eight-run gate.

HP40gs also exposed a stale-PASS observation before the next runtime cleared
BSS. Lifecycle monitoring now requires an object phase before accepting PASS.
HP40gs passes a fresh eight-run installation with this guard; HP39g+/39gs pass
four additional alternating-instance invocations with the same guard.

These are historical local emulator results. The expanded Reset and object
completion evidence below supersedes their incomplete Reset coverage.

### M7.3h-j local completion

Local validation on 2026-10-05 completes the implementation gate:

- Object mutation passes on all six Stock models. HP39g+/39gs/40gs each pass
  15 invocations covering native GC, exit/relaunch, two saved instances,
  cancelled Reset, confirmed Reset and isolation from the other instance.
- Composite objects pass on all six Stock models. Each HP39 model passes eight
  invocations with nested owned values surviving native GC and independent
  persistence. Classic models cover numeric/symbolic matrices, exact wider
  numbers, stack reorder/Booleans and representative flows from all four HPGCC
  generations.
- The dedicated HP39 catalog fixture passes Program/Note CRUD and Aplet
  list/clone/remove on all three models, with native record maintenance,
  caller-context restoration, active/default protection and owned snapshots.
- All 47 host tests, formatting, lint, host plus six ARM Clang analysis
  configurations, workflow matrix validation and documentation builds pass.

The native portal request writer is nonrecursive and has a guarded private ARM
stack. Aplet clone restores GC-traced caller contexts on success and error;
delete maintains Stock's cached display list as well as storage. These address
faults reproduced by the public catalog fixture.

Catalog and composite checks use separate payloads: combining them exceeded
the Stock startup memory available when two full test Aplets were saved.
The split retains every test and the original mutation/composite lifecycle
contracts. Suite applicability is declared in `tests/e2e/object-suites.json`.

Port 0/1/2 lack public read/write services. Each was accepted as an M7.3j
limitation on 2026-10-04, with separate future implementation goals retained.
M7.3h–j now await formal CI acceptance; these local results do not claim a
completed remote CI run or native-port support.


### M7.4 system services — local validation

The `m74-system-services` batch implements civil RTC read/set/conversion,
cooperative timers, cancellable tones, model/context capabilities, USB voltage
presence, contrast reads, bounded UTF-8 editing and bounded text rendering.
Formal CI acceptance is recorded below; this does not close the expanded M7.3 scope.

Local checks on 2026-10-03 use the pinned toolchain and repository harness:

| Gate | Result |
| --- | --- |
| Host units | All 41 pass, including the full Gregorian conversion range, invalid RTC samples, timer overflow, sound cleanup and bounded UTF-8 operations |
| Quality | Formatter, lint and Clang analysis pass for host units and all four ARM payload projects; modified workflows pass actionlint |
| System services | All six Direct and all six Stock models pass; forward/backward date changes preserve an existing timer deadline, tone cancellation restores borrowed registers, and Stock returns settle |
| Emulator | A fresh pinned x50ng checkout prepares and builds with the writable RTC; host tests cover leap/year boundaries, reset freeze/resume and invalid-register repair |
| Documentation | Zensical build passes |

The System services payload is separate from Runtime in one shared workflow
matrix. Combining them exhausted HP48's small application heap during the
existing object-directory probe. The split keeps that probe and the complete
52-frame Runtime keyboard sequence intact. After the split, Stock Runtime passes
HP48 and all three HP39 models, including every keyboard frame. The earlier
combined payload also passed all six Direct models and HP49g+/50g Stock.

One HP48 System services Stock attempt remained on the Stock desktop before
payload entry and timed out. An unchanged isolated rerun passed. The initiating
cause is unresolved; the retry is not evidence of a transport fix.

Emulator tests establish register activity, timing and restoration, not audible
speaker output on physical hardware. USB detection means voltage presence;
battery percentage and actual power-source selection are not inferred.


### HP50g Stock Gallery installation regression

On `770e2d5c`, [Runtime](https://github.com/zyf722/hpclang/actions/runs/37107606230),
[Object mutation](https://github.com/zyf722/hpclang/actions/runs/37107606212) and
[Quality](https://github.com/zyf722/hpclang/actions/runs/37107606256) pass.
[Gallery](https://github.com/zyf722/hpclang/actions/runs/37107606274) fails only
HP50g Stock. Its installer screenshot shows `Too Few Arguments`; the calculator
remains in ALG mode with no destination port on the stack, and ARMToolbox is
absent from the subsequent library menu. No Gallery payload entry is observed.
An unchanged local rerun passes, so that rerun does not close the failure.

HP50g Gallery now reuses canonical USB transfer/installation, including the
explicit Stock `-95 CF` RPN command. Only Gallery's interactive capture/exit
sequence is appended. This removes its duplicated, timing-sensitive MODE
switch and the unchecked SD install completion from the Gallery path.
The regular payload PASS, settled return and Gallery captures remain required.
Local HP50g USB Gallery validation passes with all three intended font pages,
payload PASS, `returned=true` and `return_settled=true`. Lint, actionlint and
the documentation build pass. Formal acceptance of this change is pending.


### Stock input delivery regression follow-up

On the second `7535af0f` Object mutation attempt, HP39g+ again selected
calculator-to-calculator reception instead of the disk service and never
entered the payload. The first M7.4 rerun also left HP39gs/HP48/HP50g at boot
or incorrectly configured installation screens. These are pre-payload failures;
none is an object-service or system-service assertion failure.

The Stock input sequencer now waits for actual guest GPIO sampling of each
injected edge, with a bounded `input-not-sampled` failure. Unit coverage checks
both matrix scan directions, unselected rows/columns, release polarity and
separate ON/modifier inputs. HP48's canonical and Gallery Kermit paths now
select RPN explicitly before installation, matching HP49/50.
Local HP39 object runs pass all three models, and HP48/HP50g Stock Gallery pass
with the new input gate and settled return. Formal regression acceptance remains pending.

The paired M7.4 branch also passes all three HP39 and both HP49/50 System
services Stock targets with this gate. HP48 Runtime passes USB installation,
the full 52-frame keyboard sequence and settled return after its USB sequence
explicitly configures Transfer before Start Server. The HP48 serial Gallery
path independently passes explicit RPN selection and settled return. The new
GPIO sampling unit, lint, formatting and documentation checks pass; M7.4 has
42 passing host units. These local results do not replace formal CI acceptance.


## Main boot/input and blank-return regressions

At `479dfbb6`, [Runtime](https://github.com/zyf722/hpclang/actions/runs/37120100691)
fails Stock HP50g before payload entry (`input-not-sampled`, first F6).
[Gallery](https://github.com/zyf722/hpclang/actions/runs/37120100744) fails Direct
HP49g+ shard 01 at `before-capture`, and HP48 shards 05/09 at `stock-resume` after
application PASS. These failures invalidate a general reliability claim based
on the preceding green branch run.

A controlled four-second host pause during HP50g cold boot reproduces the first
failure. The key is delivered while `GPGCON=0x55555555` (all outputs); when Stock
later configures the keyboard, that already-held key has no fresh input edge.
The shared key sequencer now waits for input readiness before delivery, then
waits for guest sampling. Direct boot keys use the same implementation.

The HP48 return error has a separate defect: quiescence accepted a stable blank
screen, and the following nonblank capture check immediately failed. Blank
samples now reset the stability count and remain pending within the existing
watchdog. Host regression tests cover initialization delay, stale samples,
separate readiness/sampling timeouts and blank-to-visible repaint transitions.

Local validation uses the pinned emulator, main's CI Gallery payloads and the
repository runners. HP48 shards 05 and 09 pass three consecutive runs each;
HP39gs Direct Gallery shard 00 passes. A fresh source preparation/build succeeds,
as do 38 host tests, lint, formatting and workflow validation. Controlled boot
pause checks on HP50g Stock Runtime and HP49g+ Direct Gallery pass after the fix.
This is evidence for the identified races, not proof that every Stock UI path
is deterministic. Formal acceptance of the follow-up remains pending.

### M7.4 integration and HP48 Gallery USB follow-up

The accepted M7.3 branch head `479dfbb6` is integrated into M7.4 in `a47b6b40`.
The merged tree passes 43 host units, System services and HP48 Runtime/Gallery
ARM builds, lint, formatting, workflow validation and documentation build.

On the preceding M7.4 head `993ae9bb`,
[Runtime](https://github.com/zyf722/hpclang/actions/runs/37117820561) passes the
Runtime/System services matrix. The remaining
[Gallery failure](https://github.com/zyf722/hpclang/actions/runs/37117820541)
is HP48 Stock: Kermit Send-Init retries expire before any payload execution.
The captures show a boot dialog followed by a server screen, but do not prove
which transport the calculator selected. They do not establish a UART root cause.

HP48 Gallery now shares the canonical USB transfer, installation and launch
runner. Its suffix contains only the three face captures, selection keys and
exit. This removes the separate UART setup and fixed 35-second transfer delay.
The workflow includes the shared HP48 dependencies and reads the canonical
Stock result. Local Gallery passes with three distinct nonblank frames,
HPC1 PASS and settled return. System services pass on HP39gs, HP48gII and
HP50g with launchers/packages rebuilt from the merged tree. Ordinary HP48
Runtime also passes with the complete 51-key acknowledged sequence.

A separate HP50g local attempt fails at the initial memory-recovery dialog with
`input-not-sampled` for the first F6 press, before server setup or payload entry.
A fresh unchanged rerun passes System services and settled return. That rerun
does not resolve the boot/input failure. GPIO sampling does not guarantee
boot/menu readiness.
Formal acceptance of the merged M7.4 follow-up remains pending.


### HP39 directory selection and HP48 Gallery transport follow-up

On `c11d02e6`, the preceding HP50g input and Direct Gallery failure points pass.
[Runtime](https://github.com/zyf722/hpclang/actions/runs/37123155107) fails HP39gs
before application entry: Kermit completes `HP39DIR.CUR` and `HP39DIR.000`, but
never receives a request for `HPGCC000.000`. Its selection capture still shows
`Preparing to get aplet`. The sequence had a fixed selection time while only
the later payload transfer was gated.

[Gallery](https://github.com/zyf722/hpclang/actions/runs/37123155104) fails HP48
Stock at Send-Init on main's separate UART path. The shared USB Gallery fix
from M7.4 `4a1d7115` is now carried onto main's follow-up branch; it passes locally
with three distinct nonblank captures and settled return.

HP39/40 sequences now have separate acknowledged directory and payload gates.
A transaction requires both successful EOF and its final acknowledgement; an
EOF from a previous file cannot release a new gate. The completion parser
handles split log lines and large reads, and produces an explicit failed marker
on its existing watchdog. Stock then saves failure evidence promptly.

Local validation uses the unchanged Runtime build and Stock assets downloaded
from the failing CI run. HP39gs passes the full keyboard and settled-return
checks. In the HP40 slow-directory experiment, each host response is delayed by
two seconds until the payload starts; the test peer's packet timeout is four
seconds to accommodate that injected latency. The directory gate releases at
44.5 seconds, beyond the old 41.5-second selection time, and only then does Stock
request the payload. This experiment stops after proving directory selection;
it is not counted as a full Runtime pass. Production protocol timeouts and
watchdogs are unchanged.
A separate 35-second full interruption exceeds Stock's own communication timeout;
that experiment is a protocol failure, not successful delay-tolerance evidence.
Host verification passes 38 C tests and all 9 discovered Python tests, including
the six completion-parser cases; lint, formatting, workflow validation and
documentation build pass. Formal acceptance of this follow-up remains pending.


### M7.4 synchronization with accepted main

All seven main workflows pass at `fe061d52`. M7.4 integrates that head,
including keyboard readiness, nonblank LCD stability and HP39 directory gates.
The remaining [M7.4 Gallery failure at `4a1d7115`](https://github.com/zyf722/hpclang/actions/runs/37121385063)
is HP40gs `input-not-sampled` on the first F6, before application entry;
main's keyboard-readiness fix covers that failure path.
The [Runtime workflow at the same head](https://github.com/zyf722/hpclang/actions/runs/37121385143)
passes both Runtime and System services across all six Direct/Stock models.
Full-branch acceptance remains pending after synchronization.

The merged tree passes all 45 host CTest entries (including the Python tool
suite), lint, formatting, workflow validation and documentation build.
Fresh preparation and compilation of the pinned emulator source succeed with
both the writable RTC and input-readiness changes. HP39gs System services
passes USB directory/payload transfer, application checks and settled return.
HP40gs Stock Gallery also passes with the freshly prepared emulator and the
merged branch's rebuilt Gallery payload: four distinct nonblank pages, native
font checks and settled Stock return. The earlier reference-payload run passes
separately; the rebuilt-payload run supplies the branch-specific evidence.

### M7.4 HP39 command-wait recovery

[Runtime at `144bd930`](https://github.com/zyf722/hpclang/actions/runs/37130948663)
fails on HP39gs before application entry: directory transfer completes, but
the Init-Info exchange for `HPGCC000.000` expires. The portable Kermit bundle
now preserves command receive mode and its active packet timer after an
Init-Info reply. Idle startup remains silent. Stock ROMs, payload assertions
and the existing transfer watchdogs are unchanged.

The PTY regression fails with the unmodified server at the missing recovery
NAK, and passes with the patched executable built by `tools/build-ckermit.sh`.
It also checks CRCs, all binary byte values, a lost data ACK and a second file
transaction. The CI bundle runs this check after both cache restore and build.
Local System services passes complete USB installation, application checks and
settled Stock return on HP39g+, HP39gs, HP40gs and HP48gII with the newly built
server. The three HP39 runs execute concurrently with normal CPU scheduling.
An additional experiment restricting all three to one CPU exceeds existing
launch/transfer bounds; it is not successful load-tolerance evidence.
Complete-branch CI acceptance is recorded below.

### M7.4 formal acceptance

Functional head `06ab4deb` passes [Runtime](https://github.com/zyf722/hpclang/actions/runs/37170021039),
[Object mutation](https://github.com/zyf722/hpclang/actions/runs/37170021046),
[Storage](https://github.com/zyf722/hpclang/actions/runs/37170021065),
[Font Gallery](https://github.com/zyf722/hpclang/actions/runs/37170021083),
[Quality](https://github.com/zyf722/hpclang/actions/runs/37170021057) and
[FatFs vendor verification](https://github.com/zyf722/hpclang/actions/runs/37170021118).
Runtime covers both Runtime and System services on all six Direct/Stock models,
including settled Stock return. Interrupted runner jobs were rerun after the
host shutdown; the accepted runs use unchanged code.

[Core runtime](https://github.com/zyf722/hpclang/actions/runs/37130948680)
passes at `144bd930`; the subsequent change affects the Kermit harness and its
packaging, not the core implementation, and does not select the Core workflow.

This closes M7.4's documented application-service and emulator acceptance.
Physical audible output remains unverified. Expanded M7.3 object services and
M7.5 display/runtime work remain open.

### M7 historical clients, volumes and card identity

Main `7303a3d6` accepts four FAT volumes, explicit native-font availability,
source-anchored graphics/keyboard/file clients and stable SD CID snapshots.
[Runtime](https://github.com/maxmixalex/hpclang/actions/runs/38152315900),
[Storage](https://github.com/maxmixalex/hpclang/actions/runs/38152315893),
[Objects](https://github.com/maxmixalex/hpclang/actions/runs/38152315907),
[Gallery](https://github.com/maxmixalex/hpclang/actions/runs/38152315888),
[Core](https://github.com/maxmixalex/hpclang/actions/runs/38152315899) and
[Quality](https://github.com/maxmixalex/hpclang/actions/runs/38152315901) all pass.
The earlier HP39g+ catalog transport failure on `11d85077` passes both exact
payload replay and its failed-job rerun. That failure occurred before
application entry and does not establish a calculator-object failure.
The subsequent main integration on the same `7303a3d6` also passes all eight
workflows: Runtime `38154282156`, Storage `38154282179`, Objects `38154282149`,
Gallery `38154282146`, Core `38154282199`, Quality `38154282143`, FatFs vendor
`38154282148` and Documentation `38154282141`.

The additional HPGCC 1.1/2.0 timed-sieve client passes six-model Direct and
Stock locally. Its bounded allocation/work checks 168 primes below 1000,
the final prime 997 and checksum 76127. Independent console bands check the
visible prime/iteration report, with a nonblank measured-time line. DMA padding
outside the physical 131-pixel width is excluded from the pixel comparison.
Formal CI for this additional client is still pending.

### M7 owned power policy — local validation complete, CI pending

HOST/ECONOMY profiles coordinate CPU clock, LCD divider and monotonic time,
reject active-sound transitions, and restore borrowed state on runtime exit.
All 72 host tests and six-model Direct/Stock System Services pass locally. The
Direct harness checks clock registers and LCD configuration before restoring
its own saved Stock state; the application deliberately leaves economy mode
active at return. HP49g+/HP50g Direct and Stock file operations also pass in
economy mode, followed by card replacement and stale-handle checks.
Native-object coexistence passes on all six Stock models, including HP39
catalogs and classic composite flows. Formal CI for this batch remains pending.

The comprehensive System Services payload has less available HP39 Stock heap
than the dedicated Display payload. Its clock/display release-order checks
therefore use two gray4 pages. An initial gray16 request correctly failed with
`ENOMEM` before the second clock transition; the saved RAM/TLS value established
the allocation failure. Gray16 rendering remains covered by the dedicated
Display suite. The power test separately waits for the timer's actual deadline,
rather than a timestamp sampled before that timer was started.

Emulator timing now preserves outstanding PWM counter ticks when PCLK changes.
Host tests cover scaling, rounding and saturation. These checks establish clock
selection and state restoration; they do not measure physical battery savings.

### M7 application termination — local validation complete, CI pending

Normal `main` return and `exit` share picolibc callback dispatch before the
runtime returns through its launcher boundary. `_Exit` skips that dispatch.
The linker now retains `.fini_array_onexit`, which picolibc uses for registered
C callbacks and C++ static destruction. Runtime cleanup also releases an
active sound session, including termination from a cancellation callback,
before restoring display, power policy and storage ownership.

All 72 host tests pass, including nested/constructor termination, zero and
extreme exit statuses, and cleanup order. Runtime, System Services and Display
pass Direct on all six models. Runtime requires both reverse-order C callbacks
and its C++ destructor before publishing PASS; Display rejects callback or
destructor execution on `_Exit`. System Services exits from an active sound
callback while in economy mode, and the harness checks clock, PWM and LCD
configuration before restoring its own saved host. HP49g+/HP50g Direct Storage
also passes with media replacement enabled. The initial termination batch
passes Stock Runtime, System Services and Display on all six models.
Changed-source Clang analysis, formatting and strict docs pass.

An earlier HP39gs System Services payload failed in the timed-client phase.
Saved MMU/RAM inspection shows `errno=ENOMEM` and only 16 KiB of writable
mapping including the boot page and 8 KiB stack. The benchmark now explicitly
packages printable ASCII, its actual output repertoire, reducing initialized
image size from 65,328 to 59,848 bytes. Stock revalidation checks this smaller
payload without reducing stack bounds or weakening the prime/pixel assertions;
the previously failing HP39gs/HP40gs checks now pass.

The additional stream-lifetime check found that the pinned ATFE picolibc has
`__STDIO_EXIT_FLUSH` disabled. The runtime now closes registered streams after
normal exit callbacks and implements `fflush(NULL)` over those streams and
live standard streams. `_Exit` continues to skip buffered-stream cleanup.
Host tests cover actual file/memory buffers, reopen, explicit close, allocation
failure and failed close/reopen; all 72 host tests pass, and the lifetime/exit
tests pass AddressSanitizer and UndefinedBehaviorSanitizer. Changed-source
Clang analysis passes.

The new Storage witness leaves a buffered file open and appends from an exit
callback. A Stock experiment demonstrated that VVFAT's directory overlay could
lose the new file despite successful native return; the identical payload on
a raw FAT image retained the complete `MAIN-ATEXIT` bytes. The fixture now keeps
card A mounted after A→B→A, eliminating the final directory-backend swap and
checking the exit-time file in card A's actual first partition. This final
Storage witness passes Direct and Stock on HP49g+/HP50g. The stream extension
also passes six-model Direct Runtime/System Services and a focused HP39gs
Stock System Services recheck with the smaller shared heap. Its changed-source
analysis, formatting, ShellCheck and strict docs pass. Formal CI is pending.

The public-runtime size increase was also checked against the last accepted
Gallery build: shard 13 was its largest Direct payload at 91,768 initialized
bytes. The new shard is 95,160 bytes; Stock's compact native-font/control-face
payload is 24,232 bytes, with 3,168 bytes of static writable data. Both build
successfully with the normal profile and remain within their launcher budgets.
This is a build/size check, not a replacement for Gallery's CI pixel gates.

At pushed functional head `216bb5d5`, [Core](https://github.com/maxmixalex/hpclang/actions/runs/38160477777),
[Quality](https://github.com/maxmixalex/hpclang/actions/runs/38160477820),
[Storage](https://github.com/maxmixalex/hpclang/actions/runs/38160477797) and
[Gallery](https://github.com/maxmixalex/hpclang/actions/runs/38160477819) pass.
The Storage CI hardware-check artifact's two launchers, installer and instructions
matched the manual package prepared for that head byte-for-byte. Runtime's
HP39gs USB transfer stopped before application entry after 90 seconds without
acknowledged packet progress; only the failed job was rerun. Objects failed at
the first copied-Aplet launch in mutation/composite on all three HP39 models.
The unchanged CI mutation artifact reproduces the failure locally: saved RAM
contains `errno=ENOMEM`, a 16 KiB writable mapping, and only 3,064 bytes between
static data and the reserved stack. Its 3,496-byte rejected-write probe cannot
even allocate the input buffer. Single-instance success therefore does not
establish the two-instance lifecycle budget. M7 remains open pending repair
and formal acceptance of these gates.

The repair keeps optional service cleanup behind acquisition-time registration.
It removes the forced link from every application's exit path to unused CPU
policy and FILE management. With the normal compiler profile and unchanged
test assertions, the HP39 mutation/composite HPC1 files shrink from 39,624/41,896
to 36,224/38,488 bytes. The copied-Aplet lifecycle still exercises large writes,
native collection, independent snapshots, instance isolation and Reset.
All 72 host tests pass, including standard-stream-only exit flushing; the
stream-lifetime and runtime-exit tests also pass ASan/UBSan. Changed-source
Clang analysis and formatting pass. Six-model Direct Runtime/System Services,
six-model Stock mutation/composite tests (including every HP39 lifecycle),
HP39gs Stock Runtime and both SD models' Direct/Stock exit-time file witnesses
pass locally. Stock System Services also pass on all six models. HP39g+ first
hit the installation-only absolute transfer ceiling under concurrent local
load, then passed unchanged with lower parallel load and the same watchdog
limits. These supplementary checks complete local acceptance of `2988e1de`;
formal CI acceptance and main integration remain pending.

The optional physical SD package has been rebuilt through the same build and
packaging inputs used by CI, with 49,824-byte launchers for HP49g+/HP50g. Its
updated runtime still requires the next CI acceptance; the maintainer's later
physical card test remains nonblocking.
