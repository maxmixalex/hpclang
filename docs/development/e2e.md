# E2E internals

## Display suite

The Runtime suite manifest includes `display`, reusing its payload build,
Direct/Stock family runners, asset preparation and evidence upload. Its manifest
entry names an additional verifier; the workflow runs that verifier after the
ordinary launch/PASS/return checks.

The payload tests actual allocator exhaustion/recovery, 2/4/16-level sessions,
two page exchanges per mode, status segments, explicit restoration and automatic
cleanup through `_Exit` with an owned session still active. Both an `atexit`
callback and a static finalizer fail the test if mistakenly invoked on that
immediate-exit path. A public-API-only HPG feasibility witness covers
clip intersections, PAINT/XOR, repeating patterns, line masks and transparent
RGB XPM import. Broad historical source adapters remain M8.

For deterministic image evidence, the test mailbox requests `FR(id)`. Both
frontends save the scanout plus LCD registers and only then acknowledge `FK(id)`.
The payload does not advance while a frame is pending; missing acknowledgements
fail within a bounded deadline. This acknowledgement is test instrumentation,
not a production API or a replacement for payload PASS. Each invocation clears
old frame evidence. Captures include column 131 for the six status segments.
`tests/e2e/display/verify.py` checks every pixel, the palette, BPP/stride/end/bank
registers, and distinct addresses after each page exchange.

The pinned emulator patch derives STN depth from LCDCON1 and row pitch from
LCDSADDR3, rather than treating power/polarity bits as a pixel-depth selector.
Direct restoration compares writable LCD configuration and seeds its return
image comparison at PASS, before runtime cleanup can restore Stock's buffer.
CPU/MMU return and nonblank stable Stock output remain separate required gates.
Emulator evidence does not measure physical-panel flicker, contrast or luminance.

System Services uses a test-only physical-input mailbox: `KI` requests one
electrical key transition and `KJ` acknowledges delivery. The guest obtains
events from the real SDK scanner/debounce queue; the harness supplies neither
characters nor SDK events. Duplicate edges and keys held at PASS fail the run.
Frame 199 proves the initial screen is clear; frames 200/201 compare the input
text with independently rendered literals for single/held and locked Alpha.
The same protocol runs under Direct and Stock, with the ordinary strict return
checks retained.

The separate `hpgcc-system` payload runs the source-anchored timed-sieve client,
including historical delay/stopwatch APIs, prime-count/checksum assertions and
the independent frame-204 report verifier. It is included in the Runtime suite
manifest for all Direct and Stock targets. Keeping it separate bounds the code
and heap demand of each application on HP39 Stock's shared launch arena.

System Services exercises HOST/ECONOMY clock policy, monotonic continuity and clock/display ownership in
both release orders. Its final action calls ordinary `exit` from a sound
cancellation callback while economy mode is active. Direct checks clock, timer
and LCD configuration before the harness restores its saved host state; Stock
requires the same payload checks and a settled native return.

The physical sequence also leaves pending keyboard interrupts before the
contrast query. Stock's syscall dispatcher enables IRQ/FIQ internally, so the
bounded query protects the interrupt-controller boundary, preserves source
pending bits, and restores routing and masks. System Services checks those
invariants; the LCD return gate catches unintended ROM wake/clock changes.

HP39 serial-disk transfer applies CRLF-to-LF conversion inside received files,
including binary Aplets. The Kermit server therefore sends transparent eight-bit
text with LF line endings in manual mode: its LF-to-CRLF expansion is the inverse
of Stock's conversion. Stock also contracts doubled backslashes. The server
quotes each backslash in a private transfer copy before starting Kermit and
removes that copy when the server exits. Input files remain unchanged. The PTY regression models
Stock decoding, covers all byte values, adjacent CR/LF runs and backslash runs, and rejects the
previous binary-mode configuration. System Services embeds a retained byte
witness with CRLF in both possible Aplet nibble alignments. A successful Kermit
EOF alone is insufficient evidence that Stock can load the resulting object.

The test tree has two layers. `tests/unit/` contains host-side `test_*.c`
tests for pure implementation logic. `tests/e2e/` contains calculator-side
HPC1 applications plus the harness data used to launch them through different
targets.

Host tests also discover Python `test_*.py` files. The native-font identity test
uses the Git LFS ROM fixture; fetch it before running the host suite with
`git lfs pull --include=tests/e2e/fixtures/hp39gplus/flash.bin.xz`. Core CI checks
out LFS contents for this test.

The canonical runtime application and focused visual/sample applications are
separate executables. In particular, Font Gallery is a **separate E2E
application**; it must not grow as extra screens inside the canonical runtime
test merely to reuse its launch harness.

The emulator workflows are split by what they prove, not by which historical
emulator happens to execute them.

The Runtime workflow reads `tests/e2e/runtime-suites.json` to build and run
focused payloads through the shared Direct/Stock family harness:

- `runtime`: core C/C++ runtime, drawing, physical keyboard progression and
  ordinary termination callbacks.
- `object-basics`: stack operations, named-object views and HPGCC Saturn compatibility.
- `system-services`: RTC read/set, deadline continuity, timers, sound, contrast,
  USB detection, bounded input and owned power policy.
- `hpgcc-system`: historical timing/calendar adapters and the timed client.
- `display`: display sessions, restoration and pixel verification described above.
- `ui`: declarative input, layout, rendering and application interaction.
- `notebook`: Stock-only primary persistence and pixels across three launches,
  built separately for each model to fit the original HP39g+ memory budget.
- `notebook-backup`: Stock-only Port 0 backup/restore/replacement/removal across
  three launches on HP48gII/HP49g+/HP50g, without the UI renderer's allocation.
- `notebook-export`: Stock-only native save and UTF-8 file export on HP49g+/HP50g,
  with a second launch checking persistence and exclusive-create refusal.
- `port-flash`: Stock-only SST identification, mapping restoration, bounded
  physical word/block programming and sector erasure on HP49g+/HP50g. Block
  checks cover complete-range preflight, bounds, identical rewrites and untouched
  neighbors. It requires
  an unused bank in the disposable emulator image, restores its erased bytes,
  and checks the complete Flash fingerprint before returning to Stock. This
  tests internal physical primitives, not native Port object mutation or
  power-loss recovery; the emulator does not model real cache or NOR timing.

`tools/e2e-runtime-plan.py` validates the registry before building. Each entry
requires `mode` and boolean `keyboard`, with optional `verifier`, `contexts`
(`direct`/`stock`), `families` and `build` (`universal`/`model`). Defaults preserve
both execution contexts and all families. The generated matrices select only
eligible profiles and pair every job with its own payload and Stock assets.
Lifecycle JSON travels with the application artifact. The build helper accepts
an optional fourth model argument, for example:
`tools/build-e2e.sh notebook build/notebook-39 hp39 hp39gplus`.

Raw-card fixtures declare `stock-media.json` version 1 with `initial_files` and
`expected_files`, mapping uppercase FAT 8.3 names to UTF-8 text (up to 4096 bytes
per file). The USB Stock harness creates a 16 MiB FAT16 image, mounts the same
image for every lifecycle launch, and verifies the expected output plus retained
initial files with independent mtools reads after exit. This fixture format
currently requires HP49g+/HP50g USB installation; unsupported contexts fail
explicitly. CI installs dosfstools/mtools when a fixture requests media and
retains the resulting raw image with its evidence.

Only `runtime` requires the injected keyboard sequence. Separate payloads reduce
each Stock upload and avoid exhausting HP48gII's heap with a monolithic binary.
Each suite has distinct artifacts and evidence paths; moving object checks out
of `runtime` preserves their assertions and Direct/Stock coverage. Transfer and
acknowledged-progress deadlines remain unchanged.

The prepared emulator models a writable civil RTC initialized from the host's
local calendar. Later register reads, alarms and saved-state restoration use its
own clock state. Its calendar conversion sources are explicit x50ng cache inputs.

## Test tiers

### Execution scheduling

An Actions execution job runs one emulator at a time. Runtime Stock and Object
jobs select an individual model from the canonical target profiles; the matrix
supplies `HPCLANG_E2E_TARGET` to the shared family harness. Payloads remain shared
or built per family as appropriate. `tools/e2e-target-matrix.py` applies the same
automation flags and family/model filters for planning and execution, rejects
empty selections, and keeps all six enabled models covered.

Direct Runtime and Storage family jobs execute their models sequentially, as
Gallery already does. Local family invocations without a model filter still run
every eligible model and collect failures. This avoids multiplying runner slots
by a hidden second level of emulator processes. Slow Stock models have separate
jobs so their transfer ceilings are not accumulated inside one job timeout.
Result, keyboard-frame, lifecycle and settled-return gates are unchanged. Stock
and Object evidence artifact names use model prefixes; each artifact retains
the selected-target list consumed by reports and suite-specific verifiers.
Stock Gallery uses the same 25-minute outer job limit as Runtime/Object Stock:
its job must not be cancelled before the shared 20-minute transfer watchdog can
report a result. The 90-second acknowledged-progress idle limit is unchanged.

Direct and Gallery failures retain a `*-failure.pgm` screen alongside the JSON
result, including failures before payload injection. Key-delivery failures also
report the key, requested edge, readiness, GPIO sample count and switch state.
Gallery archives the failure screen with the affected shard.

Stock failures also log all ARM registers, banked stack/link/status registers,
and MMU fault registers. Object mutation builds retain application and launcher
ELF symbols; failed Stock jobs additionally archive external and on-chip SRAM
in a separate failure artifact. Match these to the failing run's log and exact
build when investigating CPU faults or stale native pointers.

**Direct runtime** boots a target-appropriate stock firmware or full-NOR fixture, bypasses calculator menus/installers, enters the common hpclang ARM boundary, runs the canonical runtime HPC1 application, and then restores the stock execution state.

**Stock launch** drives the calculator's original UI and supported launcher path, then requires the same machine-readable E2E PASS result from that same runtime application.

The two tiers are deliberately separate. Direct runtime owns common-bootstrap execution and restoration correctness. Stock launch owns calculator-side package, transfer, menu, and launcher integration. A Stock job may additionally probe the post-return UI, but that visible-return evidence is not interchangeable with reaching HPC1 PASS.

## Calculator-object and stack-mutation witness

Object mutation artifacts are built per `hp39`, `hp48`, and `hp4950` family.
`tools/build-e2e.sh object-mutation <out-dir> <family>` selects the matching
payload sources and launchers from the target profiles. The default `all`
build remains available for source analysis and cross-family debugging.
This keeps classic directory probes out of HP39 Aplet RAM while preserving
their coverage in the classic-family jobs. Each artifact carries its own HPC1
image and result address; the workflow never substitutes another family's image.

The Objects workflow reads suite names and applicable families from
`tests/e2e/object-suites.json`. `object-composite` uses the same build command
shape and Stock harness. It covers lists, numeric/symbolic matrices, scalar
precision, stack reorder/Booleans and historical application flows. Splitting
these payloads keeps sufficient HP39 RAM for two saved Aplet instances; mutation
and composite retain their own lifecycle checks. `object-catalog` separately
checks HP39 Program/Note CRUD and native Aplet clone/remove, name-list snapshots,
protected Aplets and settled return. It does not load the numerical/GC fixture
into the same Aplet. The workflow publishes separate results for each applicable
suite/model and derives the available families and models from the target profiles.

The suite manifest also declares its launch modes. `object-catalog` preserves
both the HP39 catalog/schema/NoteText round-trip witnesses formerly embedded in
Runtime and their Direct no-Stock `ENOSYS` checks. Stock runs additionally exercise
catalog CRUD; Direct runs validate unavailable native services without inventing
a Stock context. These checks share one catalog payload. Moving them out of the
keyboard/runtime payload preserves HP39 launcher memory for its minimum stack
and writable pages; the launcher rejects insufficient memory rather than
shrinking those reservations.

The HP39 lifecycle contract includes Stock SAVE of a second Aplet and alternating
write/read/clear invocations of the two instances. The default chronological
library ordering moves the last-used Aplet to the first row, so each switch
selects the second row. Every invocation must match its data phase and reach a
settled Stock return.

Stock lifecycle contracts can select different flows by calculator family.
Version 2 uses a `families` map keyed by `hp39`, `hp48` or `hp4950`; each value
contains `initial_phase` and `relaunches`. Version 1's single-family contract
remains supported. The shared transport setup validates the complete contract
and enables only the current family's flow. Invalid contracts fail the run;
an absent family does not implicitly reuse another model's sequence.

Stock setup commands use the same explicit family selection when needed:
version 2's `families` map contains command lists, while version 1 retains a
single `commands` list. Kermit obtains the current family from the target
environment. All command lists are validated before selection, and a missing
family is an error. The object-mutation fixture creates `PORTONE` in Port 1
only on HP49g+/HP50g, whose ROM provides that namespace; HP48gII keeps its
Port 0 fixture and checks unavailable Port 1 reads.

The classic object-mutation flow now exercises Port 0 across three independent
launches from saved Stock state: create an integer backup, read and replace it
with a string, then read and remove that string. Each invocation also exercises
temporary backup growth/shrinkage and HOME mutation. The native VAR/library
keys recall the existing application through ARMToolbox; there is no reinstall,
fresh calculator state or data injection between invocations. HP39 retains its
existing instance-isolation and Reset sequence.

Lifecycle runs require a newly observed object phase before accepting PASS.
Saved emulator RAM can retain the previous invocation's PASS while the new
bootstrap has installed the data mapping but has not yet cleared BSS. Accepting
that value would also inject the return key while the new payload is still
running. `HPCLANG_STOCK_REQUIRE_OBJECT_PHASE` enables this launch guard for the
lifecycle runner; it does not change the payload's result or skip a phase check.

The historical HPGCC39 splash computes the author's horizontal position as
`66 - 2 * length`. The package uses a short author credit: a long description
produces a negative coordinate, and HP39g+ can write outside the destination
GROB into another stored Aplet. Instance relaunches cover this corruption path;
a PASS from only the first invocation would miss it.

The canonical runtime payload covers the closed M7.3a-e read/stack/adapter baseline and HP39 public variable access. The focused Object mutation payload uses the shared USB harness on all six Stock models: classic RPL put/remove and nested-reference preservation on HP48/49/50; size-changing native-variable writes, GC, rejection paths and context restoration on HP39. Both require settled Stock return. M7.3f is complete; final CI acceptance is recorded in [Validation](validation.md#m73f-final-implementation-batch).

- Direct/non-Stock environments and HP39-family targets reject the Stock-only RPL stack mutation surface cleanly with `ENOSYS`.
- Stock HP48gII and HP49g+/HP50g read `DSKTOP`/D1 and `EDITLINE`, derive user-visible depth after launcher-bias normalization, and snapshot only objects whose native extent is provable.
- The mutation witness is self-contained inside HPC1: it creates owned CHAR `A`/`B` snapshots, pushes A, replaces level 1 with B, peeks/validates B, pushes A again, pops/validates A, drops the remaining inserted object, and requires the final stack depth to equal the initial depth.
- Pop snapshots before it mutates D1. Replace allocates/copies the new object before committing the target pointer. Expected capacity/level failures are planned before the first mutation.
- No raw Saturn address or mapped ARM pointer is exposed to the E2E application.

Historical read-only evidence remains **Core #285 / Runtime #232**. Launcher-bias normalization is independently closed by fully-green **Runtime #237**. The stack-mutation focused evidence is **Core #296 (`35439583956`)** plus the successful canonical-build, Direct hp39/hp48/hp4950, Stock hp48 and Stock hp4950 jobs of **Runtime #241 (`35439583995`)**. M7.3d/e then add named-store reads, HP39 descriptor-driven Variable recall, and the bounded HPGCC adapters. The final read/adapter closure is **Runtime #338 (`35812005032`)**, fully green across Build, all Direct families, Stock hp39, Stock hp48, Stock hp4950, and evidence publication.

## Return evidence

Return validation has three distinct observations:

1. **payload PASS** — the guest reaches machine-readable `0x50415353`;
2. **control-flow restoration** — stage0 leaves HPC1 and the stock address space / saved machine state becomes active again;
3. **visible Stock-UI restoration** — after a harmless native UI action, the LCD is visibly redrawn by the stock OS rather than merely retaining the last hpclang framebuffer.

The common Runtime/Storage fixture publishes PASS only after both reverse-order
C `atexit` callbacks and its C++ static-object destructor have executed. Thus
returning from `main` without picolibc finalizer dispatch cannot pass merely by
reaching the launcher. The focused System Services and Display fixtures cover
explicit `exit` and `_Exit`, respectively, with owned hardware still active.

A nonblank stale hpclang frame proves neither item 2 nor item 3. The harness
must not copy the pre-launch framebuffer back into place and call that a return.
For HP39 hardware validation the established post-return action is **APLET**;
a valid return opens/redraws the normal APLET library. Other families should
use an equally harmless model-appropriate Stock-UI action.

## Stock Gallery on HP39

HP39g+/39gs/40gs use the canonical USB Aplet install/launch harness.
`run-stock-gallery-hp39.sh` prepares the compact Gallery package and appends
`hp39-stock-gallery-run.seq` for native mini/main/EditLine and generated-control
navigation. It retains the standard runtime and settled Stock return gates,
then compares native pixels with the target's own NOR. The former duplicated
gs/40gs serial-disk install sequences are retired.

## Stock transfer mechanisms

HP 39g+, HP 39gs, and HP 40gs use the original USB Connectivity Kit path by default. A headless Kermit server supplies the typed files:

- `HP39DIR.CUR`;
- `HP39DIR.000`;
- `HPGCC000.000`;
- `LIB275L.000`.

The stock ROM performs the transfer/install itself.

HP 48gII uses the Stock Kermit server over USB to receive `L275_48.LIB`, generated `HPCLANG.HP`, and any focused witness object such as `ZZE2E`, then installs library 275 into Port 0 and launches the payload through ARMToolbox48 / `PrRUN`. UART remains available as an override. The long-standing transfer race is synchronized by an explicit host-completion gate: calculator UI automation does not resume until the Kermit sender has exited successfully. The shared Kermit watchdog fails after 90 seconds without an acknowledged send-window advance, with a separate 20-minute absolute transfer ceiling. Repeated packets do not refresh the idle budget; neither limit paces UI actions.

HP 49g+ and HP 50g transfer SETUP, generated `HPCLANG.HP`, and `ZZE2E` over USB Kermit, then install ARMToolbox into Port 2. They share the same Stock UI sequence while retaining their native per-model ARM bootloaders. An explicit SD override retains the emulated-card path used by Storage. Current migration evidence is tracked in [Stock USB transport](stock-usb-transport.md).

When reproducing Stock Storage, use the workflow's
`HPCLANG_E2E_STOCK_TRANSPORT_OVERRIDE=sd`, `HPCLANG_E2E_STORAGE_WITNESS=1`
and `HPCLANG_E2E_REQUIRE_KEYBOARD=0` settings. The SD path prepares the FAT
fixture used by the file assertions; the default USB launch does not mount
that fixture. A missing-card error from such a USB run is not evidence of a
Storage regression.

The SD installer explicitly selects RPN in the Stock modes form once HOME is
ready. Its CHOOS selection works from either initial mode; a toggle during cold
boot could be lost to the Memory Clear dialog or switch in the wrong direction.
ALG command entry alone does not execute the stack-based `PrRUN` launch.
Installation and application PASS remain separate checks.

The historical pre-USB cleanup baseline is retained for regression provenance. Canonical **Runtime #357 (`35832611353`)** is fully green on the final cleanup functional head, including HP48gII after the Kermit host-gate and bounded `ZZE2E` object witness. **Font Gallery #130 (`35832611436`) is also fully green** across Direct hp39/hp48/hp4950 and Stock hp39gs/hp40gs/hp48gii/hp50g. **Storage #141 (`35832611387`)** and **Quality #13 (`35832611366`)** are green on the same head. The earlier HP39 Gallery red path had two independent harness regressions: launch/UI settling required payload-aware launch-only retries, and the Gallery application retained an obsolete direct `platform_cookie == code-window` availability check after runtime moved HP39 private state behind an opaque tagged platform context. The runtime font service now owns that private mapping decision and the independent ROM pixel oracle validates the rendered result.

## Target profiles

`tests/e2e/targets/*.json` contains emulator/model metadata shared by workflows:

- `target` — workflow and filename identity;
- `family` — `hp39`, `hp48`, or `hp4950`;
- `model` — stable hpclang model ID;
- `lcd_height` — 64- or 80-line capture height;
- `boot_dialog` — optional P5 confirmation pattern, relative to the target profile;
- `automated_e2e` — optional Direct/focused automation eligibility; it defaults to true;
- `automated_stock_e2e` — optional original-Stock automation eligibility; it defaults to true and may be false when the stock host transport is not modeled;
- `stock_transport` — `usb`, `uart`, or `sd`; all six current profiles select USB, with `HPCLANG_E2E_STOCK_TRANSPORT_OVERRIDE` available for supported secondary paths;
- `nor` — optional checked full-NOR fixture metadata: repository path, compression, decoded byte count, and SHA-256.

Only targets distributed as per-model firmware archives add a `firmware` object. HP 39gs and HP 40gs use their hpcalc archive/member names.

HP 39g+ uses the verified real-hardware 1 MiB D1.55 full-NOR fixture under `tests/e2e/fixtures/hp39gplus/`; its profile records the decoded size and SHA-256 and is enabled in the automated family matrices.

HP 49g+ and HP 50g share the 2.15 OS image but carry distinct `bootloader` filenames because the bootloader difference is genuine per-model metadata.

HP 48gII also uses a pinned full-NOR fixture rather than an update archive.

Profiles do not carry empty placeholder fields.

`tools/e2e-target-env.py` exposes a selected profile as `HPCLANG_E2E_*` environment variables for the workflows.

## Sequence files

Human-like stock-UI key input belongs under `tests/e2e/sequences/`. Sequence
filenames describe the target and lifecycle step (for example
`hp39gs-stock-run.seq`), not the historical experiment that first established
the path. Offsets are nominal milliseconds from sequence start:

```text
9200 capture files
9500 tap up
9800 tap f6
12000 chord rshift two
```

The stock runner accepts logical key actions rather than GUI coordinates.

`dismiss <key> <pattern.pgm>` waits for a specific confirmation dialog before
tapping its key. HP39 cold boots use it for the Memory Clear dialog. The P5
pattern is relative to the sequence file, matches the model's LCD dimensions,
and uses pixel value 128 as a wildcard for changing status indicators. A tap is
retried only while the original dialog remains; completion requires a stable,
nonblank changed view after key release. The operation fails after 15 seconds
and preserves the failure screen. This prevents startup repainting or a missed
confirmation from silently shifting the rest of the key sequence.

Direct runtime and Gallery use the same dialog event implementation as Stock.
Their HP39 target profiles select the Memory Clear pattern through `boot_dialog`;
the environment exporter resolves it relative to the profile and rejects missing
files. The launch event waits until dismissal completes, rather than sending F6
at fixed times while Stock may still be initializing its keyboard.

Events execute in offset order. The gap between adjacent offsets is a minimum
delay from actual delivery, so a host scheduling pause cannot collapse a key
press and release into one guest-invisible callback batch. `host-gate` pauses
the sequence until the transfer-completion marker exists; subsequent offsets
start at zero relative to gate release. A named gate such as `host-gate directory`
waits for the corresponding marker suffix. HP39/40 first await the acknowledged
`HP39DIR.000` transaction before selecting an Aplet, then await `LIB275L.000`
before launching it. EOF alone is insufficient: the sender must also observe
the transaction acknowledgement. A failed-transfer marker stops the sequence
with evidence instead of waiting for the outer emulator timeout. `monitor` first waits up to two seconds
for a nonblank pre-launch LCD frame, then arms the payload deadline before
allowing launch keys to proceed. Blank screens still fail when that bound expires.

The current model-specific sequences exist because the stock user interfaces are genuinely different. They should not be unified by adding conditional logic to one large script unless that makes the behavior easier to understand.

## Workflow path selection

Push filters compare the branch's previous head with the new head. Merging main
into an older work branch therefore includes the incoming main changes, even
when those changes were already tested on main. Gallery runs for font changes
and its shared bootstrap, launcher, compiler setup and emulator dependencies;
an isolated object-store implementation change does not select Gallery.

## x50ng backend

CI checks out a pinned external x50ng revision. `tools/prepare-x50ng.py` installs
the Direct, Stock, and Gallery frontends from `tests/e2e/harness/x50ng/` and
applies the shared flash, SRAM, reserved-RAM, USB, and UART adaptations.
Gallery mapping sizes live in its C frontend; preparation does not rewrite
frontend constants or add firmware-specific diagnostic probes.

Tests read the emulated LCD controller and drive the calculator key matrix directly. No GTK/SDL window automation, Xvfb, mouse coordinates, or host-screen scraping is used at runtime.

Fresh-state missing register keys/groups use reset defaults silently. Pass
x50ng `--verbose` to inspect those optional state lookups. Other state lookup
errors still print normally; this does not filter or discard emulator stderr.

x50ng remains an external emulator/test dependency. None of its GPL implementation is copied into hpclang's production runtime.

Current roles:

- Direct runtime on HP 39g+, HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g;
- Stock USB typed-file/APLET launch for HP 39g+/39gs/40gs;
- Stock USB Kermit/ARMToolbox48 launch for HP 48gII;
- Stock USB Kermit/ARMToolbox launch for HP 49g+/50g;
- explicit UART and SD/VVFAT overrides for secondary regressions; Storage uses SD.

The x50ng preparation helper accepts the same three platform names as hpclang: `hp39`, `hp48`, and `hp4950`. These names describe hpclang's platform grouping; they are not historical HP codenames.

## HP 39g+ E2E

HP 39g+ now participates in the automated `hp39` matrices using a complete
1 MiB NOR image acquired from real hardware. The fixture is installed directly
as x50ng persistent flash state and is verified against the profile's byte count
and SHA-256 before each run. No sibling firmware archive or foreign bootloader is
substituted.

HP39g+ D1.55 uses the USB Connectivity Kit `Disk drive...` path. The common
USB device/host bridge now supports the HP39 typed-file Kermit adapter and
the profile enables `automated_stock_e2e`. The canonical USB Runtime gate establishes Stock
PASS on HP39g+, HP39gs, and HP40gs. The Conn3x GUI is not a CI dependency.

The existence of a trustworthy full-NOR fixture closes firmware identity and
boot/runtime validation. It does not automatically prove that HP39g+ shares the
HP39gs/40gs physical ROM-font offsets. The native gs/40gs main/mini/EditLine
font mapping remains excluded for model 1 until separately established.

## CI build stages

E2E applications are target-independent. CI should therefore avoid rebuilding a selected application once per calculator.

For the canonical runtime application, the implemented pipeline has one shared build plus two execution tiers and one shared-asset stage for Stock:

1. **application build** — build one canonical runtime HPC1 application and all target launchers;
2. **Direct family execution** — run `hp39`, `hp48`, and `hp4950` against that same artifact;
3. **Stock family execution** — drive the original package/UI/launcher paths against that same artifact; Stock transfer assets are prepared once and shared.

Direct and Stock therefore do **not** have separately compiled runtime payloads.
A focused application such as Font Gallery gets its own build artifact,
specification and evidence path while reusing the common target/harness
infrastructure.

The same rule applies to the focused **Storage** gate. `E2E / Storage` builds the storage-enabled payload separately from the canonical keyboard/runtime payload so storage timing and fixtures do not perturb the canonical Runtime gate. Its hp4950 Stock path creates the named-directory VVFAT launch fixture after ARMToolbox installation but before the run-phase emulator opens the card, arms result monitoring before `PrRUN`, and requires payload PASS, ARMToolbox stage ownership release, and settled Stock repaint on both HP49g+ and HP50g.

Storage file operations run on raw FAT16 card images created by `dosfstools` and
`mtools`, installed explicitly by the Storage workflow. Once the payload starts,
an acknowledged media checkpoint inserts card A before the first storage call.
Later checkpoints replace A with B and return A, keeping that raw card mounted
through application exit and Stock return. Each replacement uses x50ng's actual card mount API, including
the falling/rising detect-pin transitions, while the guest is paused: a driver
that samples only current presence cannot observe the removal.

The guest must reject an old dirty stream and directory, preserve the replacement
card's sentinel, write a fresh file, and recover the original card's bytes. The
host independently reads card B's FAT image after unmount and requires all three
checkpoint phases before accepting PASS. Card A must additionally contain
`MAIN-ATEXIT`: the application leaves a buffered file open, then appends from an
exit callback; normal termination must flush and close it. The oracle obtains
the partition offset from the MBR. Raw images avoid VVFAT's delayed
directory-overlay commit semantics obscuring real persistence. The same workflow
builds an optional [physical SD-card check](sd-media-check.md); hardware execution
remains a separate, nonblocking follow-up.

Directory-backed cards enumerate host names in byte order before building FAT
entries, independent of host filesystem enumeration and locale. The Stock File
Manager groups directories first; the storage sequence selects the first regular
file after the volume label. A regression creates identical trees in opposite
orders and checks identical enumeration. This ordering applies to emulator card
construction, not to the public SDK's directory-iteration contract.

Storage #22 (`35378151818`) is the M7.2b historical closure evidence for the earlier KOS-backed stream/metadata bridge. Storage #23 then independently proved that Stock KOS syscall 19 can safely rebuild the calculator filesystem after direct ownership is released.

The integrated M7.2 implementation goes further: the first byte-stream operation acquires the process-wide SD/FAT lease, and all stdio, metadata, capacity, namespace mutation, and owned directory iteration use hpclang's S3C2410 SDI + FatFs backend until runtime shutdown. The focused payload verifies missing-file semantics, create/truncate/append, seek/tell, metadata and FAT timestamps, capacity/free space, rename/mkdir/rmdir, UTF-8 long filenames, case-insensitive lookup with preserved stored case, simultaneous independent iterators, ordinary stdio while an iterator is live, and final reopen/read before the runtime unmounts/releases the controller and invokes Stock ResetSD.

The M7.3f follow-up Storage run
[36509415707](https://github.com/zyf722/hpclang/actions/runs/36509415707)
passed the payload but crashed on HP49g+/HP50g during shutdown: Stock `ResetSD`
reused the launcher's live `L1[0x90]` mapping. The private KOS bridge now preserves
that descriptor across the call and publishes its restoration before returning
to stage1. KOS returns through the caller LR, so the bridge supplies an explicit
continuation before restoring the original ARM/Thumb caller. `ResetSD`, strict
return checks and the Stock UI witness remain required. Local Stock and Direct
hp4950 verification passes with this repair. Formal Storage
[36972578170](https://github.com/zyf722/hpclang/actions/runs/36972578170)
is fully green on `7cca953b` after an unchanged rerun of jobs interrupted by
runner communication loss. Storage
path filters include `runtime/libc/kos-call.S` so future bridge changes exercise
this boundary.

The current private stage window is `0x01300000`, defined once in
`platform/armtoolbox/stage-layout.h` for C, assembly, linking and emulator
observers. Stage0 restores the previous descriptor on exit. Stock HP39g+
interrupts use the byte at `0x07f00240`, so the historical `L1[0x90]` location
cannot remain a private mapping while native Stock calls run. The KOS bridge
protects the current shared-layout slot; the public HPC1 data/code ABI is
unchanged.

Final acceptance evidence for the original M7.2 subset is **E2E / Storage #53 (`35428824540`)** on `c4b827fe`. All jobs pass: Build, Direct hp39, Direct hp48, Direct hp4950, and Stock hp4950. The payload additionally runs the bounded HPGCC `hpstdio.h`/`fsystem.h` adapter flow and validates HP-style paths, adapter-local cwd, LFN directory enumeration, volume size/free queries, and POSIX `ftruncate()` extension/shrink semantics including zero-filled extension.

The metadata follow-up extends that same Storage payload with attributed file
creation, masked mutation, read-only rejection, creation/access-date getters,
close/reopen persistence and directory-entry ownership after iterator closure.
All six Direct models and both HP49g+/HP50g Stock models pass locally; formal CI
acceptance of this extension is complete on `6a7f9a28`. Host tests additionally construct
distinct on-disk FAT timestamps and verify malformed-date handling and remount
persistence. Stored access-date queries do not imply automatic atime updates.

HP 39g+ participates in Direct, focused, and original-Stock family matrices through its data-driven `nor` profile. All six profiles currently select `stock_transport: usb`. See [Stock USB transport](stock-usb-transport.md) for migration evidence and the distinction between implemented defaults and repeated-run stability.

GitHub Actions YAML is orchestration, not the test specification. Expected files, required captures, protocol events, and similar assertions belong under `tests/e2e/specs/` and are evaluated by the E2E harness. Fixture integrity checks such as a pinned firmware SHA-256 belong to fixture preparation, not to the behavioral test specification.

## Full-keyboard validation and progression evidence

Headless x50ng uses process CPU time plus bounded intentional idle waits for
both guest hardware timers and harness events. Time spent descheduled by the
host does not become a long calculator key press. Interactive frontends retain
the upstream wall clock. RTC calendar initialization still uses the host date;
subsequent RTC progression shares the guest clock with the other peripherals.
Shell and workflow watchdogs remain wall-clock limits, including the external
Kermit transfer watchdog. This avoids input drift under scheduling pauses; it
does not guarantee instruction-level deterministic emulation or USB success.

A local regression experiment paused the emulator during each of three DOWN
presses in the HP39gs receive menu. The wall-clock implementation selected the
wrong transport; the execution clock preserved the USB selection under the
same pauses. Formal integrated CI acceptance is recorded on `6a7f9a28`;
USB throughput remains a reliability follow-up.

The canonical runtime E2E injects every physical key position on each automated calculator model. The guest verifies DOWN/UP events, raw scan code, physical key identity, and dedicated-key modifier state for all 51 keys.

The final LCD screen renders the physical keyboard rather than the electrical scan matrix: six menu keys, two three-key control rows beside the cursor cluster, and seven five-key main rows. Passed positions are filled, and a failing position is left open and marked. The screen labels use the public font/text API, so calculator E2E exercises the same rasterization path instead of carrying a test-only glyph table. The machine-readable result remains the actual gate; the LCD diagram is a visual summary of the same checks.

Direct and Stock both capture an initial hollow keyboard plus one frame after every accepted key release: **52 progression frames** for the 51-key test. Stock frames are phase-gated by `KACK(n)`: a new frame is accepted only after the preceding DOWN→UP cycle has returned, the payload has updated its passed-bit state, and the display has been redrawn. Runtime #135 (`35298949365`) is the current canonical fully-green witness; #122 remains the first run that closed the same 52-frame contract. The merged progression contact sheet remains supplementary evidence beside the canonical `before / pass / return` summary.

## Font Gallery application

Font Gallery is a focused visual/sample E2E application, not part of the canonical runtime test. The public font core has no semantic theme-role layer: Gallery enumerates the concrete ROM fonts hpclang actually exposes, followed by generated font products. HP39gs/40gs expose minifont, main/default, and EditLine; HP48gII/49g+/50g expose minifont and bigfont/default.

The exhaustive Direct Gallery is family-dependent: HP39gs/40gs traverse **248 entries / 496 captures** (245 generated + three ROM fonts), while HP48gII/49g+/50g traverse **247 / 494** (245 generated + two ROM fonts). The production matrix still runs one family job for `hp39`, `hp48`, and `hp4950`, covering both 64- and 80-line display layouts. It validates generated aliases/arbitrary face selection, metrics/spacing, repertoire-sensitive specimens, deterministic pagination, PASS and settled Direct return.

Native-font evidence is intentionally family-aware. The synthetic Direct launcher does not fabricate a live Saturn `Read[]` map, so exhaustive generated-font rendering is authoritative on all six targets, while HP48/49/50 native-font pixels require a real Stock logical map. Compact Stock Gallery controls close both automated live-native families: HP50g validates the hp4950 path and HP48gII independently validates the 64-line hp48 path through the original UI/ARMToolbox launch.

The compact Stock navigation remains family-aware. HP39gs/40gs use **DOWN** across minifont → main/default → EditLine, then **RIGHT** to the generated control. HP48gII/HP50g use **DOWN** across minifont → bigfont/default, then **RIGHT** to the generated control. The #112 artifacts visibly retain those distinct concrete ROM-font pages.

HP39gs/40gs are different: their fixed native raster tables are physical stock-ROM assets. Production bootstrap maps only the two required physical NOR pages (`0x0007e000` and `0x0007f000`) as uncached small pages in the private prefix of hpclang's own code L2 table. This design was chosen after diagnostics proved that a long-lived high-L1 NOR section alias could have its descriptor word overwritten by the active LCD framebuffer itself. Font Gallery therefore also serves as the regression gate for this bootstrap-private mapping.

Gallery #99 was the first fully-green production closure across Build, universal x50ng, all three Direct families, and the four Stock Gallery targets; #102 later exercised the strict HP48 settled-return gate. The final M7.1 head is now covered by **35298949457 / #112**, which repeats the full matrix after the HP39 EditLine/native-font census correction and documentation sync.

HP48gII and HP50g Stock Gallery use the canonical USB install/launch runners.
Their sequence suffixes contain only face selection, captures and exit. This
keeps transfer readiness, explicit RPN setup and settled-return handling shared
with Runtime while preserving Gallery's three distinct, nonblank frame checks.

Keeping Gallery separate prevents font exploration from inflating the runtime ABI/keyboard/storage test and lets its visual evidence evolve independently of canonical runtime PASS evidence.

### Current bitmap-only font closure status (2026-09-14)

Canonical bundled assets are bitmap-source only. The generated catalog is **245 faces / 1,639,485 mapped glyphs / 3,472 assembly sources** with **245/245** source-to-generated parity. Font Assets **#24** regenerates the catalog without drift and reports no exact duplicates, overflow, glyph-collapse, blank-heavy, or Powerline regressions.

The general TTF importer has been removed. Final bitmap curation adds Cubic 11, ChillBitmap 7px, and QuanPixel/May7 with provenance retained in the source package. Gallery labels/specimens were strengthened for Ark Pixel regional variants, width-sensitive Latin, digits/punctuation, accents/symbols, and supported CJK/Japanese/Korean repertoires.

`E2E / Font Gallery` **#130 (`35832611436`)** is a historical fully-green regression gate. Current integration acceptance is tracked in [Validation](validation.md). HP39gs/40gs Direct traverse **248 entries / 496 captures** including EditLine; HP48gII/49g+/HP50g remain **247 / 494**. Stock hp39gs/hp40gs pass mini/main/EditLine/generated-control plus ROM fingerprint/pixel oracles, and HP48gII/HP50g retain settled live-native evidence. Historical #112 remains the original final M7.1 native-census closure baseline.

The tested font/model slice is accepted. Later main integration evidence is recorded in [Validation](validation.md). The HP39g+ native-font extension has its own Direct/Stock ROM-pixel validation and CI acceptance gate.


### Stock boot and repaint synchronization

Direct Runtime, Direct Gallery and Stock launch sequences share the key-delivery
state machine. A matrix key-down waits for a driven scan line and a readable
input line; it is not sent while Stock has configured the whole matrix as
outputs during cold boot. After a press, the sequence waits for a fresh guest
GPIO sample. After release, it confirms that the emulated switch is open;
Stock may stop scanning after handling the press, so a second guest read is
not required. Readiness, press sampling and switch release each have a bounded
watchdog and a distinct failure reason. None proves that a Stock menu accepted the action;
transfer completion, application results and settled return remain independent
gates.

Within the Direct Gallery application, automated builds enable
`HPCLANG_GALLERY_KEY_HANDSHAKE`: the harness holds each key until the real
event engine reports DOWN and the payload publishes `GKAC`. The harness then
releases it; the payload waits for the matching UP before rendering and
publishing the next ready frame. This avoids missed timed taps and repeated
navigation while a slow frame is drawn. Standalone interactive builds keep
normal key repeat. Failure JSON retains the last payload state, including
an unfinished key acknowledgement.

LCD return stability requires a changed, nonblank frame across consecutive
observations. A blank repaint interval resets stability instead of becoming an
immediate `stock-resume` failure. The existing return deadline still bounds a
screen that never recovers. Gallery artifacts retain each shard's result and
before/PASS/return images so later shards cannot hide the failing screen.
