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
- `port-library`: Stock-only Port 0 creation of a minimal CRC-valid library,
  exact owned readback and duplicate-ID refusal on the three classic models.
  Before a second launch, native RPL recalls the stored library into HOME;
  the application checks that independent witness against the input encoding.
  HP49g+/HP50g also create libraries in Ports 1 and 2 and check separate native
  RCL witnesses. HP48gII skips absent ports through capability discovery.
  This verifies record storage and persistence, not registration or execution.
- `code-relocation`: HP49g+/HP50g Stock-only ARM-side branch planning and guarded
  RAM replay, followed by 88 native repaired/control cases. 38 cases also copy the
  repaired instruction to a new address and clear its original bytes before
  native execution; another 38 vary the branch target address. Four D0/D1 cases
  move data and distinguish repaired pointers from stale-pointer controls. Eight
  register-transfer cases exercise PC=A/C and PC=(A/C), with explicit guarded
  source cells distinguishing register provenance from indirect pointer fields.
  The standard install
  phase is followed by a required verifier checking every case, a final RPL
  return marker and unchanged Flash. Controlled target placement tests instruction
  behavior and the ARM-to-Stock handoff, not arbitrary library movement.
- `port-flash`: Stock-only SST identification, mapping restoration, bounded
  physical word/block programming and sector erasure on HP49g+/HP50g. Block
  checks cover complete-range preflight, bounds, identical rewrites and untouched
  neighbors. The native tail scan validates existing records, refuses data left
  behind an erased end marker, and recognizes free space again after erasure.
  A deliberately interrupted body is recovered into a deleted allocation,
  followed by a second recovery that must leave Flash unchanged.
  A native backup append after recovery uses an independent Stock record capture, verifies
  its value through the public read API, rejects occupied/cross-bank writes,
  and restores the saved bank-prefix sector. A separate completed-body fixture with partial status `73`
  is committed against the retained expected record, read through the public
  API, and committed again without changing Flash. Both fixture phases restore
  their original bank-prefix sector. The test requires an unused bank in a
  disposable emulator image and checks the complete Flash fingerprint before
  returning to Stock. Public `hpclang_port_backup_create(2, ...)` selects space among existing
  native records, verifies its value through the public read API and rejects
  duplicate creation without any change. The selected sector is saved and
  restored, with the whole-image fingerprint checked again. This covers
  internal physical/append primitives and public backup creation. Durable-intent
  fixtures also verify unstarted abort, committed-new recovery, incomplete-body
  rollback and partial-publication completion through public readback, restoring
  the complete image after each test run;
  Port replacement/deletion and power-loss recovery remain separate
  acceptance gates. The emulator does not model real cache or NOR timing.
- `port-flash-backup`: a separate public-API persistence client on HP49g+/HP50g.
  It creates one Flash backup, then validates it across two independent relaunches.
  Before each relaunch, an installed native RPL program recalls the backup and
  stores a HOME witness. The application validates and removes that witness,
  requiring native recall again next time. The three launches cover ordinary
  return, `exit()` and `_Exit()`, while repeated creation must report `EEXIST`.
  Stock setup uses C-Kermit character escapes for the native program delimiters
  and explicitly orders HPCLANG/SETUP for the existing installation sequence.
  Remote HOST commands are grouped so RPL operators cannot become local output
  redirection. This fixture retains its backup in the disposable test image.

`tools/e2e-runtime-plan.py` validates the registry before building. Each entry
requires `mode` and boolean `keyboard`, with optional `verifier`, `contexts`
(`direct`/`stock`), `families` and `build` (`universal`/`model`). Defaults preserve
both execution contexts and all families. The generated matrices select only
eligible profiles and pair every job with its own payload and Stock assets.
All Stock setup, lifecycle and media JSON travels with the application artifact.
The build helper accepts
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

### Native launcher observation in call-root cases

`tests/e2e/code-relocation/call_roots.py` creates a fixed 128-byte `ENTRYOBS`
string before launching its original-return case and five controlled call-root
cases. The native
payload fills that string after bootstrap, before creating its test library or
rewriting the suspended return path. It captures the native page-pool count and
table, saved Saturn PC/IP/caller, machine-stack cursor, hidden launcher root,
R0–R4 and the stage0 source prefix. Recording does not resize the string.

The verifier requires a complete versioned record and matches 32 source bytes
against the current model's generated stage0 image. `entry.json` keeps the
observed values separately from the later controlled mutation results. C still
names ROM helper `0x1596fc`; R3.H identifies the stage0 source. These fields must
not be conflated with an ARM PC sample. The record proves that the native
payload reached its observation point through Stock; it is not a trace of every
instruction before entry, nor proof of arbitrary live-register/banked safety.
Existing complete catalog bindings, fixture operands, final return marker and
unchanged Flash checks remain required after the observation.

The `native` case uses only the `HPCSCALL` marker string, with no encoded call.
After validating that exact string, the payload preserves the original suspended
PC and launcher-collected return roots, grows the catalog and resumes Stock.
The marker string must remain byte-identical, the real caller must reach the
post-launch marker, all existing catalog bindings must survive, and library 123
must resolve to its new Port0 record. This covers the unmodified launcher return
in the controlled clean-state fixture; it does not establish complete opaque
object or banked-reference safety for arbitrary applications.

Catalog-growth fixtures record native nibble dependencies from both root scans
and nested object traversal, coalesce them into a bounded guard array and pass
them to the repaired catalog commit. Guard exhaustion or observation failure
stops before commit. Only bytes actually read by the parsers are guarded;
compact synthetic prologues are excluded and opaque contents remain unresolved.
The unit regression changes each observed field and requires stale-plan rejection,
and checks callback failures, read faults, invalid nibbles and guard capacity.

### Offline original-ROM bank-switch check

`tests/e2e/code-relocation/bank_switch.py` accepts repeatable
`--state hp50g:/path/to/state` or `--state hp49gplus:/path/to/state` arguments,
an `--output` directory and optional host `--cc`. Each state contains `sram`
and `flash`; the Python environment requires Unicorn. For example:

```sh
python tests/e2e/code-relocation/bank_switch.py \
  --state hp50g:/work/saved-state --output /work/bank-switch-check
```

For each saved state it executes all sixteen profiled ROM Flash switch entries
and the ERAM entry at every return-stack cursor, using the original ARM Saturn
interpreter. Only the synthetic caller return and cursor are installed; no
switch body is skipped. Execution must return within the instruction bound.
All 256 resulting Read-map entries are checked: the window must match the
captured bank bases, and entries outside it must stay unchanged. The production
span resolver must accept real windows and reject unused-bank placeholders
without changing its output. The report retains traces and state hashes.

Four HP50g/HP49g+ saved states pass 544 cases and 9,632 executed-map
overlap/alias comparisons locally. Bank 0 maps 128 KiB despite its 96 KiB
allocation: its last 32 KiB aliases bank 1 at different Saturn addresses.
The comparisons include these aliases, opposite-parity non-aliases, every
pair of selected windows and rejection of bank 14/15's unaligned placeholders.
The caller must normalize physical identity and address offsets before using
qualified reference plans; unequal bank indices alone cannot prove disjointness.
The offset planner additionally passes 160 hypothetical alias repairs using
translations derived from these executed maps. Resolving each patched native
address must yield the canonical destination's physical nibble. This does not
write ROM or treat the aliased tail as a writable allocation. Unit regressions
cover independent relative endpoints, split fields, native encoding overflow
and unchanged output on rejection. The checker removes stale reports on entry.
Catalog-plan integration checks payload and switch offsets independently,
including sentinel preservation and invalidation of partial output. A controlled
alias catalog feeds its generated edits/guards into actual catalog insertion:
19 changed nibbles and 19 read failures reject with no native/ARM-root writes
and an unchanged memory snapshot; the unchanged case repairs the encoded alias,
moves the catalog/root and completes insertion. This synthetic mapping test
does not establish complete live roots for public library operations.
This is a reviewed firmware-profile check, not general ROM discovery,
allocation/liveness proof, hardware acceptance or permission to relocate.

### Offline original-ROM threaded return check

`tests/e2e/code-relocation/threaded_return.py` checks the reviewed HP49g+/HP50g
command-4 ARM-return/GETPTR/dispatch path independently of the Stock transfer harness.
Run on Linux with a host C compiler and Python `unicorn==2.1.4` in a virtual
environment. Supply a complete saved `sram`/`flash` pair and the corresponding
normal-return register trace from the diagnostic emulator; these private inputs
are not distributed with the repository.

```sh
python tests/e2e/code-relocation/threaded_return.py \
  --model hp50g --state /path/to/before-registration \
  --trace /path/to/normal-register-trace.log --output /tmp/threaded-return
```

The gate checks the reviewed instruction/entry layout and ordered trace evidence,
compiles the current C catalog mover, and runs all eight return-stack cursor
positions from both the GETPTR tail and original post-ARMSYS RTNCC. The latter
runs the condition, full GETPTR call and final ROM dispatch. The production
stage return-root collector identifies the original caller slot before the C
catalog mover repairs it and the suspended PC. The GETPTR return is generated by the
moved call instruction. Baseline/move runs must agree on the complete trace,
shifted RAM PCs, saved-IP reload and register results. Omitted saved-IP, caller
return and (for the RAM entry) suspended-PC repairs must fail. Each model runs
32 accepted baseline/move cases and 40 omission controls.

Seven distinct nonzero ring sentinels must survive and the consumed caller slot
must clear. Native cursor writes show no extra stack capacity is needed beyond
one existing caller slot. R0–R4 must have no observed reads or writes through
this bounded path. Initial carry is deliberately set for the ARM entry; RTNCC
must clear it before the conditional branch. Trace input must include the
original return-stack pop and ordered GETPTR/dispatch register evidence.
It writes `getptr-move.json` with source/input hashes only after all checks pass;
a rerun removes a prior report before validation. Python optimization is refused
because assertions are part of the verifier. Output also contains the locally
built `geometry.so`; use a separate output directory for each concurrent run.

The saved IP is reconstructed from the trace, and CPU/return-stack state is
controlled. CODE, SEMI and ROM contents remain original. This verifies catalog
geometry and this bounded return path; it does not publish a catalog record, prove
whole ARMJUMP reachability, or replace full Stock registration/startup acceptance.
A different ROM or cached-program layout requires review before extending this
gate. This optional offline check is not automatically included in CI.

### Offline MMU entry/return check

`tests/e2e/code-relocation/mmu_entry.py` connects the reviewed command-4 MMU
entry (`889A5` to ARMSYS at `88B43`) with its bounded return through GETPTR
and dispatch to `03130`. Use the same Linux/Unicorn environment and private
saved state, plus a generated Launcher V3 file:

```sh
python tests/e2e/code-relocation/mmu_entry.py \
  --model hp50g --state /path/to/before-registration \
  --launcher /path/to/hp50g.hp --output /tmp/mmu-entry
```

The launcher supplies the reviewed memory policy; physical entry/code addresses
and length are controlled. All eight cursors must reach the MMU ARM boundary.
R0 retains the physical ARM entry, R1 receives code address/length, R3 receives
entry/policy, R4 receives the ROM conversion entry, and R2's low field becomes
the table extent. The bounded return must neither read nor write R0–R4.
The original MMU helpers need three temporary return slots and the return
consumes one caller: safe initial live depth is independently known 1–5.
Four untouched sentinels survive; the other four slots end cleared.

This skips the intervening ARM execution and does not prove initial descriptor
loading, arbitrary memory geometry, actual code relocation, or later Stock
execution. Original instructions run in both segments; controlled values are
not evidence for every launcher. Changed policy/code guards remove stale
reports; successful `mmu-entry.json` reports retain traces and input hashes.

With `--descriptor` and a separate output directory, entry begins at `8890D`.
A controlled 40-nibble launch descriptor is placed in a hex-string object and
referenced by the data stack. Initial A/C/R0 values are poisoned; B retains the live RPL return-stack
position required by SAVPTR. That position must survive both SAVPTR and the
bounded return. Original
instructions must load the descriptor fields, save pointers and generate the
MMU caller return address. Each model checks all eight cursors. The full tested
segment peaks at four extra return slots and ends at its original depth;
four other stack sentinels survive. Independently known initial live depth must
therefore be at most four. This connects descriptor consumption to MMU setup
and return, but does not execute the earlier descriptor producer, validate an
arbitrary object supplied by a user, or run the intervening ARM application.

`--linked-descriptor` additionally places the supplied launcher at a controlled
main-RAM address and executes the original GETOBJINFO and GETLINKINFO helpers.
Their entry/address/size outputs feed the original allocation and descriptor
field-writing instructions (`88378`–`883BA`). The resulting object, rather than
a manually assembled descriptor, then feeds the descriptor/MMU/return segment.
Each model checks all eight cursors. The helper invocations have controlled
return boundaries and reset cursors; the published four-slot bound applies to
the descriptor/MMU/return segment only. ARMLAUNCH lookup and its final address
field remain controlled, and no ARM payload is executed. Traces retain these
boundaries explicitly; this is not complete PrLNK or whole-launch acceptance.

`--full-link` instead runs the cached PrLNK CODE continuously from `88311` to
the Stock continuation at `05143`. It uses the original `XLIB 275 11` reference
at `882F7`, as found in this saved state's PrLNK program, rather than the global
ARMLAUNCH name used by another historical source version. Lookup, allocation,
and all descriptor-field writes execute without helper-boundary resets. The
resolved launch entry returned by GETLINKINFO must equal the final descriptor
field and the MMU ARM target. The resulting descriptor then feeds the existing
descriptor/MMU/return check, with a controlled boundary between the two CODE
segments. The RPL preamble, intervening ARM payload, arbitrary bank lifetime
and complete launch remain outside this gate; the four-slot bound applies only
to the descriptor/MMU segment, not to the preceding PrLNK execution.

Combine `--full-link --relocate` to compile the current C catalog mover and
production return-root collector (a host C compiler is required). At the
controlled ARM boundary, grow the catalog by 16 nibbles and repair its saved
IP, suspended PC and proven caller slot before executing the original return.
The mutated memory, not an independently reconstructed snapshot, feeds GETPTR
and dispatch. R0–R4 remain untouched through this bounded return. Data-stack
inputs reserve real slots below EDITLINE and adjust the available-cell count;
stack bounds and the shifted RPL return-stack position are checked afterward.
Source hashes and write counts are included in the report. This still skips
the ARM payload and does not establish validity of its physical mappings
during arbitrary application execution or complete public-registration safety.

### Offline PrRUN prefix check

`tests/e2e/code-relocation/prrun.py` uses the same `--model`, `--state`,
`--launcher` and `--output` arguments as the MMU check. It executes the original
PrRUN prefix from a controlled valid data-stack input without resetting CPU
state. At the first encountered ARMSYS it follows the original ARM dispatch
and requires the ROM helper target `0x1596fc`. **This is not the application
entry.** The helper body is skipped; the check simulates its return and stops
at the first ROM SEMI dispatch `03130`.

Earlier descriptions overstated this gate as complete GC/link/MMU preparation
and an application return. Those claims are withdrawn: the helper body invokes
Stock services and has not been executed by this gate. Complete startup, actual
application entry/execution and subsequent Stock behavior remain unverified.
The separate segmented MMU gate and full native Stock tests retain their own
explicit boundaries and evidence.

Both input policy and initial cache layout are pinned. All eight cursors must
reach the reviewed helper and simulated continuation, with valid data-stack
bounds, no R0–R4 access in that continuation and unchanged saved registers.
Observed peak extra depth is four; four unused sentinels survive and four used
slots clear. This is only the prefix's depth budget, not a whole-launch budget.
`prrun.json` records input hashes, entry/dispatch/continuation traces, helper
identity and explicit false flags for helper-body/application execution. Failed
guards remove stale reports.

With `--relocate`, the gate compiles the current C catalog mover and production
return-root collector, grows the catalog by 16 nibbles at this helper boundary,
and checks the repaired caller, PC and saved IP during the simulated return.
It records source hashes and write counts. This is bounded synthetic mutation
coverage; it does not prove mutation during an actual application or authorize
public registration.

### Offline alternate ARM return check

`tests/e2e/code-relocation/alternate_return.py` checks the second reviewed
command-4 ARMSYS return, from `88C73` through GETPTR, PUTPTR and RTNCC to a
controlled ROM caller address (`03130`, stopped before execution), using the same
Linux/Unicorn environment and private saved-state inputs:

```sh
python tests/e2e/code-relocation/alternate_return.py \
  --model hp50g --state /path/to/before-registration \
  --output /tmp/alternate-return
```

Run separately for HP49g+. Each model checks eight stack cursors and four R2
low-bit combinations, before and after actual C catalog growth: 64 cases plus
eight intentionally omitted R0 repairs. When the low two R2 bits equal one,
the original code copies R0's low 20 bits into the RPL data stack. The C
assignment inspector identifies that field, and the production guarded register
repair preserves the upper bits while relocating the controlled address.
Other flag combinations must not read R0. Omitting the needed repair must
produce the stale address. All five saved registers remain unchanged during
the original interpreter execution.

GETPTR and PUTPTR each temporarily use the same free machine return-stack slot;
RTNCC then consumes the controlled caller slot. The peak depth increases by one
and the final depth decreases by one. Both used slots end cleared, while the
other six distinct sentinels survive. Safe entry therefore requires independently
known live depth 1–7; the cursor alone cannot establish this condition. Baseline
and moved instruction traces must match after RAM-PC normalization. PUTPTR must
publish the expected data-stack position and saved IP, and RTNCC must clear carry.

Reviewed instruction and target guards reject changed inputs. The gate removes
stale reports before checking and writes `alternate-return.json` with source
and input hashes only on success. CPU/register values and the target are
controlled: this does not establish incoming R0 provenance, reachability of
this entry, arbitrary flags/register states, or execution beyond the controlled
ROM caller. It supplements the first return-path gate and remains optional offline
evidence, not complete relocation or public-registration acceptance.

Add `--allocated-root` with a separate output directory to obtain R0 from the
original legacy allocation path (`88BC1` to just before ARMSYS at `88C6E`).
This uses a controlled 32-byte request with the leave-on-stack flag, runs the
ROM allocator, and verifies that R0 identifies its new string object. Each
model then runs 16 baseline/move cases and eight omitted-repair controls with
that allocated object. The allocator-created memory is carried into the C
mover and return check. The ARM program itself is not executed between those
segments; the return CPU state remains controlled, so this does not prove
preservation across arbitrary ARM execution or whole-launcher reachability.
The current hpclang wrapper selects the separate MMU path; legacy allocation
coverage must not be mistaken for a prerequisite of every generated launcher.

### Offline ACCESSRAM return check

`tests/e2e/code-relocation/accessram_return.py` checks four reviewed command-4
ACCESSRAM1 call sites and their subsequent SAT2ARM conversion, using the same
Linux/Unicorn environment and caller-supplied saved `sram`/`flash` inputs as the
threaded return check (a host C compiler is also required):

```sh
python tests/e2e/code-relocation/accessram_return.py \
  --state /path/to/hp50g-before-registration --model hp50g \
  --output /tmp/accessram-hp50g
```

Run separately with `--model hp49gplus` and its saved state. Each run checks all
eight return-stack cursors at four call sites: the ROM service must return to
the original continuation without writing R4, and six subsequent main-RAM
conversions must produce the expected physical byte addresses. It records input
and driver fingerprints, executed paths and changed mapping words. The P=8
call additionally walks one and two controlled library records and rejects an
overflowing extent. These inputs are placed through the mapping established by
the original CONFIG sequence; exact returned free space, carry, and traversal
counts are checked. Each model runs 56 service cases, 336 conversions and 672
original caller continuations. Input guards reject changed reviewed call,
exchange and alignment encodings and remove any stale report on that failure.

The gate compiles the current C literal/assignment inspectors and compares
their register predictions with execution of the original LC→R4 initialization
pair, after establishing P=0 through its original instruction. R4 is then
preserved across the service and conversion checks. Decoder source fingerprints
are retained; changed initialization/call encodings reject stale reports.
The subsequent original call at `88A82` executes SAT2ARM, restores a caller-saved
logical address through C=RSTK, and follows both ST-controlled paths to `88AA0`.
For the alignment path, the C exchange inspector predicts both ACEX W swaps;
the independent arithmetic expectation rounds the logical address to an 0x2000
boundary modulo 20 bits. All nine working/saved registers, D0, P, ST, the
return-stack cursor and mappings are checked. Every ring slot starts with a
distinct nonzero sentinel: both nested-call slots and the consumed caller slot
must end cleared, while the other five slots must retain their exact values.
Native cursor writes independently measure a peak depth increase of two, a
minimum of minus one and a final delta of minus one. Thus this path requires
an independently established initial live depth of 1–6 to avoid ring overflow
or underflow; the cursor and stale slot values cannot establish that depth.
The skip path preserves D0. The maximum-address case wraps to zero; matching
original arithmetic does not establish a usable address. Saved logical
addresses are explicit caller inputs.
Decoder and driver fingerprints must remain stable throughout the run.
This is a controlled entry, not execution of all preceding launcher branches.

The other entry registers and mappings are controlled. P=8 coverage tests the ROM
size walk, not validation of library bodies, arbitrary contents or every service
branch. Other bank configurations remain outside this gate.
ACCESSRAM1 can change non-main mappings: only the main read mapping and the
observed R4 lifetime are checked. This gate does not authorize general bank
lifetimes, public registration, or complete launcher relocation, and requires
private inputs rather than running automatically in CI.
