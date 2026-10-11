# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/maxmixalex/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Scope of the completed milestones

M0–M6 describe the **execution foundation**: compiler/runtime viability, the HPC1 ABI, calculator launch paths, stock-system return, packaging, fixtures, and automated E2E.

They do **not** mean that hpclang already reproduces the application-facing feature set of HPGCC 1.x, HPGCC 2.x, or HPGCC3. M7 now provides stable device facts, monotonic time, the complete six-model physical keyboard/event core, opaque 2/4/16-level software surfaces, geometry, native/generated font and text rendering, hp4950 stream stdio through picolibc, and accepted M7.3a–j object/stack/directory, composite-object, HP39 Aplet persistence/Reset and native-catalog services. M7.4 common system services and M7.5 display/runtime services are accepted. The final parity work remains open; the individually reviewed native-port limitations retain implementation goals below.

The project target is stronger than "substantial calculator-facing functionality":

> **First reach practical HPGCC capability parity for ordinary calculator applications, then exceed it.**

"Parity" here means that the major application-facing jobs supported by the mature HPGCC 1.1/2.0 ecosystem have a reliable hpclang equivalent on hardware where the capability genuinely exists: graphics/text, keyboard/input, time/system services, byte-stream storage, calculator objects/RPL/stack integration, and the helper functionality needed by real applications. It does **not** require exposing historical raw MMU addresses, KOS module numbers, stock syscall calling conventions, or binary compatibility as canonical hpclang APIs. Those low-level historical interfaces may be adapters or documented migrations when a clean service is safer.

"Exceed" means keeping those application capabilities while adding things HPGCC did not provide consistently: one LLVM-native toolchain and application ABI across all supported families, explicit capabilities and ownership, modern C/C++ wrappers, UTF-8/large bitmap-font support, cross-family semantics, stronger error handling, deterministic generated assets, and automated Direct + original-Stock validation.

HPGCC 1.1-p1 with 39G support is the historical HP39-family application reference; HPGCC 2.0 SP2 is the principal HP48/49/50 parity reference. HPGCC3 remains an additional API/design reference, not a ROMLIB ABI or custom-ROM target.

## Principles

- one application ABI: HPC1 / hpclang v1;
- one common ARMv4T runtime;
- thin platform implementations: `hp39`, `hp48`, and `hp4950`;
- no required GCC, historical HPGCC runtime, or ROM replacement;
- historical code is research evidence, not a compatibility mode;
- emulator fixtures are generated or prepared from verified inputs rather than committed as opaque calculator-state snapshots;
- model-specific differences remain metadata or platform code unless they genuinely require a different implementation;
- stock-hosted backends preserve host hardware ownership where practical;
- downstream firmware, operating systems, and applications remain consumers rather than named hpclang ABI branches.

## M0 — Unified application ABI

**Complete.**

HPC1, AAPCS32, Thumb-1/ARMv4T, soft float, and the fixed virtual layout are the single supported application contract. Existing model IDs 1–6 are stable ABI values.

## M1 — Modern language/runtime stack

**Complete.**

LLVM compiler builtins, ARMv4T-safe integer and soft-float helpers, picolibc integration, malloc, static TLS, constructors, and normal C/C++ linkage are in the common runtime. Historical GCC code generation, libgcc, r9/r10 static-base conventions, and legacy HPGCC runtime objects are not normal-build dependencies.

## M2 — Common ARM bootstrap

**Complete for the validated platforms.**

The common bootstrap installs the HPC1 mappings, enters the application, restores temporary state, and returns to the calculator OS. The common code coarse table also has a launcher/runtime-private prefix before `HPCLANG_CODE_BASE`; HP39gs/40gs native-font access uses two uncached small-page mappings there without changing the public HPC1 ABI.

HP 39gs real hardware plus the current emulator gates validate the design. HP 39g+, HP 39gs, and HP 40gs share the `hp39` platform; HP 48gII uses `hp48`; HP 49g+ and HP 50g share `hp4950`.

## M3 — HP39 platform

**Direct runtime and automated original-Stock USB launch are complete for HP 39g+/39gs/40gs.** HP39g+ uses its own verified D1.55 fixture for install, execution, and settled return.

- [x] HP 39gs real-hardware canonical runtime PASS and clean return.
- [x] HP 39gs and HP 40gs official-ROM Direct E2E.
- [x] HP 39gs/40gs hardware-validation packages.
- [x] HP 39g+ model-1 hardware-validation package using the shared HPGCC39 APLET/ARMToolbox39 boundary.
- [x] Exercise stock serial-disk transfer of `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000` through the original HP39gs/40gs ROM.
- [x] Drive stock APLET → HPCLANG → ARMToolbox39 → HPC1 PASS → stock return on HP 39gs and HP 40gs.
- [x] Validate a read-only HP39 flash-dump mechanism byte-for-byte against emulated physical NOR.
- [x] Acquire a real HP 39g+ and dump its complete 1 MiB D1.55 stock flash.
- [x] Add the verified HP 39g+ flash fixture to Direct runtime E2E, including strict 51-key identity/modifier validation and clean return.
- [x] Automate the original-system HP 39g+ USB Connectivity Kit install/launch path using the shared USB bridge and HP39 typed-file Kermit adapter.

The one-shot HP39g+ ROM dumper is retired after successful acquisition. The verified fixture establishes model-1 firmware identity and Direct execution, but does not by itself establish HP39gs/40gs native-font offsets.

## M4 — HP48 platform / HP 48gII

**Complete.**

- [x] Add model 4 / 131×64 E2E target profile.
- [x] Record the genuine HP48gII hardware differences: 256 KiB physical RAM, 64-line LCD, and HP48gII firmware identity.
- [x] Add the x50ng HP48 preparation mode.
- [x] Establish and pin a genuine 2 MiB full-NOR fixture.
- [x] Boot the full-NOR image with 256 KiB RAM and 131×64 LCD.
- [x] Execute the same HPC1 runtime and validate clean return.
- [x] Exercise the ARMToolbox48 calculator-side launch path through Stock Kermit (USB default; UART fallback).

Completion criterion:

> HP48gII executes the same HPC1 runtime image through the common bootstrap and returns to a responsive stock environment without a distinct application ABI.

## M5 — hp4950 platform / HP 49g+ and HP 50g

**Complete.**

- [x] HP 50g Direct runtime E2E PASS and clean stock return.
- [x] HP 50g original-system ARMToolbox install → `HPCLANG.HP` → `PrRUN` → PASS.
- [x] Add HP 49g+ model 5 to the shared hp4950 Direct runtime matrix.
- [x] Use `boot-49g+.bin` for HP 49g+ and `boot-50g.bin` for HP 50g.
- [x] HP 49g+ Direct runtime E2E PASS and clean stock return.
- [x] HP 49g+ original-system stock package/ARMToolbox path PASS.
- [x] Keep one stock File Manager/ARMToolbox sequence for both models.

HP 49g+ and HP 50g share the `hp4950` platform, official 2.15 OS image, stock File Manager/ARMToolbox sequence, and HPC1 ABI. Their distinct ARM bootloaders are target metadata rather than a runtime fork.

## M6 — Automated emulator E2E

**Direct runtime and original-Stock USB launch are complete for all six models.** HP39g+ uses its own verified D1.55 fixture. UART and SD remain explicit secondary paths. The dedicated Gallery and Storage Stock workflows keep their separate transport choices, as described in [Stock USB transport](stock-usb-transport.md).

Durable Direct launch evidence exists for HP39g+, HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. Historical pre-USB original-Stock launch evidence covers HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. Control-flow return and visible stock-UI repaint remain deliberately separate gates. Runtime **35298949365 / #135** is the final M7.1 fully-green canonical Runtime evidence across all Direct and Stock families, including HP48 payload PASS, stage teardown, control-flow restoration, settled Stock repaint, and the exact 52-frame keyboard progression. Runtime #122 remains the first run that closed that full contract.

Model-1 transport integration:

- [x] HP 39g+ Direct runtime on the verified native D1.55 stock flash fixture.
- [x] HP 39g+ original-system USB Connectivity Kit launch on that fixture.
- [x] All-six-model original-Stock USB Runtime and pre-integration Gallery/Storage/Quality acceptance.
- [x] Close integration Storage/Gallery harness timing failures and repeat cross-workflow acceptance on `12631370`. See [Validation](validation.md).
- [x] Accept explicit RPN setup, keyboard frame readiness, and full Quality gates on `fdcd38b7`. Branch retirement follows main acceptance.
- [ ] Optional U5: quantify repeated-run USB stability and elapsed time against secondary paths; this is not a functional-completion blocker.

Direct runtime owns common-bootstrap execution and return correctness. Stock-launch gates own calculator-side package/menu/launcher integration.

## M7 — Application-facing calculator services

**The existing M7.1/M7.2 acceptance remains valid for its tested scope. M7.3a–j object services, M7.4 common system services and M7.5 display/runtime services are accepted. Overall M7 remains open for the capability/evidence ledger, complete text/numeric/console behavior, media-change coverage and representative historical application flows listed below. Native Port 0/1/2 limitations remain individually approved follow-ups.**

M7 is where hpclang becomes a practical calculator SDK rather than only a validated native execution environment. Its completion criterion is **clean-core capability parity with the ordinary application-facing jobs of HPGCC**, not an arbitrary count of APIs.

M7 deliberately separates capability parity from historical spelling. A service belongs in M7 when a real application needs the capability even if the eventual HPGCC-compatible function name lives in M8. By the end of M7, the clean core should be sufficient to implement the broad HPGCC adapters without reaching back into raw KOS/Saturn/MMU internals.

Planned dependency order after M7.1:

1. **M7.2 — byte-stream filesystem and stdio parity:** complete namespace, metadata, directory, capacity, and standard-C file behavior on HP49g+/HP50g; unsupported families remain explicit.
2. **M7.3 — calculator objects, Saturn/RPL and stack:** typed object handles, copy/convert/ownership rules, named object stores, and stack exchange without treating calculator objects as files.
3. **M7.4 — remaining common system services:** RTC/wall clock, sound/beep and other broadly useful HPGCC-era system facilities that can be exposed without leaking raw hardware ownership.
4. **M7.5 — display/runtime capability gaps needed by real HPGCC-class applications:** resolve physical grayscale/page-flip/annunciator or equivalent application needs behind an ownership-safe broker; leave raw MMU/cache/IRQ controls as migration-only unless a genuine clean use case appears.
5. **M7 parity audit:** mechanically re-run the historical API inventory against implemented clean services and identify every remaining common application pattern as Direct, Adapter, Migration, or intentionally Unsupported before M8 broad source adapters begin.

### M7.0 — API inventory and compatibility design

- [x] inventory the public HPGCC 1.0.3/1.1 and 2.0 headers by functional area from complete distribution archives;
- [x] inventory the relevant HPGCC3 public interfaces separately;
- [x] classify major historical API families as direct source-compatible, compatibility-adapter, migration-only, or intentionally unsupported;
- [x] publish a progress-oriented API reference with per-service validation and HPGCC comparison;
- [x] define the C ABI boundary and C++ wrapper policy;
- [x] establish common conventions through the implemented device/time/keyboard/display/storage slices;
- [ ] finish conventions only when remaining object/UI services expose genuinely new ownership, error, string/buffer, or capability-query requirements.

See [SDK API design](sdk-design.md).

### M7.1 — Common services, display, keyboard, and fonts

Device information, stock-preserving monotonic time, keyboard scanning/events, physical key identity, the C++23 wrappers, display geometry, opaque software surfaces, blitting, text rendering, and LCD ownership preservation are implemented and validated on the current six-model Direct matrix. HP39g+ model-1 key identity is established by the real D1.55 fixture and the same strict 51-key transition gate.

At M7.1 closure, the clean core provided representation-independent circles/polygons
and owned 2/4/16-level surfaces. Mutable clip/style state, PAINT/XOR, patterns,
annunciator slots, XPM convenience loaders, raw framebuffer-pointer behavior,
physical gray4/gray16 and page flipping remained compatibility/future work.
M7.5 now supplies owned physical modes, page exchange and status segments, plus
tested clip/style/pattern/XPM prototypes; installed historical adapters remain M8.

#### Font package/gallery closure

**Complete.** The common-services/display/keyboard/font slice has implementation, Direct/Stock evidence, and repository-level integration closure.

The font system has two intentionally separate sources:

- **calculator-native fonts** are borrowed stock assets exposed by concrete accessors: `hpclang_font_mini()`, HP48/49/50-only `hpclang_font_big()`, HP39gs/40gs-only `hpclang_font_editline()`, plus the convenience `hpclang_font_default()`; no HP glyph bitmap is redistributed;
- **generated custom fonts** come from the canonical source bundle and checked-in generated catalog and are selected per target with `hpclang_target_font()`.

HP39/40 uses fixed physical stock-ROM raster tables exposed through two uncached 4 KiB mappings in hpclang's bootstrap-private code-L2 prefix. HP48gII/49g+/50g resolves stock native font objects through the live logical Saturn `Read[]` mapping. These platform details remain below the common font API.

Final evidence:

- [x] canonical bundle is bitmap-source only; the general TTF importer and its dependencies/tests/docs are removed;
- [x] generated catalog is **245 faces / 1,639,485 glyph mappings / 3,472 assembly sources** with **245/245** source→generated parity;
- [x] final curation adds Cubic 11, ChillBitmap 7px, and QuanPixel/May7 while retaining source/license provenance;
- [x] Font Assets **#24** regenerates the 245-face catalog without drift and reports no exact duplicates, overflow, glyph-collapse, blank-heavy, or Powerline regressions;
- [x] large faces remain split into bounded assembly chunks and glyph/bitmap sections remain individually garbage-collectable;
- [x] Ark Pixel labels/specimens expose size, Mono/Prop, locale, and common regional differences instead of obscure-only samples;
- [x] Gallery specimens cover width-sensitive Latin, digits/punctuation, accents, symbols, and supported CJK/Japanese/Korean repertoires;
- [x] exhaustive Direct Gallery is family-dependent: HP39gs/40gs traverse **248 entries / 496 captures** (245 generated + minifont + main/default + EditLine); HP48gII/49g+/50g remain **247 / 494** (245 generated + minifont + bigfont/default);
- [x] final production `E2E / Font Gallery` run **35298949457 / #112** is fully green across Build, universal x50ng, all three Direct Gallery families, and Stock hp39gs/hp40gs/hp48gii/hp50g;
- [x] HP50g Stock validates live native minifont/main font plus generated control and reaches settled PASS;
- [x] HP48gII Stock validates distinct nonblank live minifont/bigfont plus generated control and reaches HPC1 PASS, control-flow return, and settled Stock repaint; Gallery #112 repeats the strict HP48 gate on the final M7.1 head;
- [x] HP48 and HP50 Stock summaries use the same compact three-panel structure, while HP39gs/40gs add the independently verified EditLine panel; the final #112 artifacts cover both layouts;
- [x] HP39 Direct summary exists and is published with the exhaustive evidence.

The import contract remains exactly three modes:

- `STATIC_AUTO` — derive the retained repertoire only from explicit `HPCLANG_UI_TEXT()` literals;
- `STATIC_EXPLICIT` — retain caller-supplied codepoints plus configured fallback;
- `FULL` — retain the complete selected face.

Generated aliases remain `@mini = newrpl_6a`, `@normal = newrpl_8c`, and `@large = newrpl_10a`. There is deliberately no public semantic font-role layer in the core; `mini`, `big`, and `editline` in native accessor names are historical concrete ROM-font names, not UI roles.

Direct Gallery intentionally does not fabricate a live Saturn `Read[]` map. Generated-font exhaustive rendering is valid there on all six models; HP39/40 native fonts are also valid through verified physical-ROM rasters; HP48/49/50 native-font evidence comes from Stock launch where the live Saturn mapping exists.

The experimental launcher-private `HPZ1`/LZ4 staging format remains rejected. Standard HPC1 is the sole application payload contract.

M7.1 font closure checklist:

- [x] implement and CI-gate `STATIC_AUTO`, `STATIC_EXPLICIT`, and `FULL`;
- [x] implement and validate `HPCLANG_UI_TEXT` extraction;
- [x] separate calculator-native assets from generated package assets;
- [x] validate the HP39 physical-ROM native reader through the owned two-page L2 window;
- [x] exhaustively render all 245 generated faces on all five automated targets and both 64/80-line layouts;
- [x] validate compact Stock native evidence on HP39gs/HP40gs/HP48gII/HP50g, including the HP39 EditLine fingerprint and 6×10 pixel oracle;
- [x] remove obsolete TTF importing, normalize summaries, strengthen specimens, and complete additional bitmap-CJK curation;
- [x] close the production Gallery matrix (`E2E / Font Gallery` #112) and retire the obsolete HP48 post-PASS exception;

### M7.2 — Byte-stream filesystem and standard I/O

**Complete.**

M7.2 covers the HP49g+/HP50g **SD-card FAT namespace** and byte-stream semantics only. HOME/Port 0, Port 1 and Port 2 are calculator-object stores, not FAT volumes; they remain M7.3 work.

The final ownership model is deliberately simpler than the historical bring-up path:

- the first hpclang byte-stream storage operation acquires a **whole-process direct namespace lease**;
- hpclang then owns the S3C2410 SDI controller and mounts the card with the pinned FatFs R0.16 + official patch-1/patch-2 baseline;
- all ordinary `open/read/write/lseek/unlink/stat/rename/mkdir/rmdir`, picolibc `FILE *` operations, metadata, capacity and directory traversal use that same FatFs namespace;
- insertion/write-protect sampling reads configured GPIO inputs before and during ownership; Stock KOS is retained only for syscall 19 (`ResetSD`) after FatFs is unmounted and the controller is released;
- the direct namespace remains active until native-process shutdown, preventing live KOS and FatFs handles from observing/mutating the same FAT state concurrently;
- HP39/40 and HP48 continue to return deterministic `ENOSYS` for byte-stream storage rather than flattening their native calculator-object stores into POSIX files.

This supersedes the earlier M7.2a/b implementation detail where simple streams and metadata temporarily used Stock KOS until a namespace operation forced a direct takeover. That bridge was useful for establishing behavior, but keeping two filesystem implementations exposed unnecessary semantic splits.

#### M7.2a — standard stream core

- [x] standard `open/close/read/write/lseek/unlink` feed picolibc;
- [x] normal `FILE *` use covers `fopen/fread/fwrite/fseek/ftell/fflush/fclose/remove`;
- [x] create/truncate/append/exclusive-create and missing-file behavior have POSIX-facing semantics over FatFs;
- [x] unsupported families fail explicitly with `ENOSYS`;
- [x] HP49g+/HP50g share the same direct stream implementation.

#### M7.2b — namespace metadata and media semantics

- [x] versioned `hpclang_stream_path_info` exposes byte size, file/directory/volume-label type, portable FAT attributes and validity-gated FAT last-modified civil time;
- [x] `hpclang_stream_storage_query()` exposes media state, free bytes and direct-backend total capacity;
- [x] write-protect state sampled at the Stock ownership boundary is carried into the direct block backend rather than ignored;
- [x] no timezone/epoch meaning is invented for FAT civil timestamps;
- [x] non-stream families retain explicit unsupported/error behavior.

#### M7.2c — directories and path semantics

- [x] directory enumeration uses genuinely independent FatFs `DIR` state;
- [x] directory creation/removal and rename use the owned namespace rather than broken/absent KOS primitives;
- [x] paths are explicit and do not mutate the Stock calculator's global current-directory state;
- [x] multiple iterators may coexist while ordinary stdio remains active;
- [x] UTF-8 LFN and case-insensitive FAT lookup are supported while enumeration preserves stored name case;
- [x] the whole-process lease prevents mixed live KOS/FatFs file handles;
- [x] shutdown closes remaining direct descriptors, unmounts FatFs, releases SDI ownership, invokes Stock `ResetSD`, and only then returns to the calculator OS.

The pinned namespace backend is independently implemented. Historical HPGCC3 FSystem remains behavior evidence only and is not copied into hpclang.

#### M7.2d — HPGCC parity adapter boundary

- [x] map ordinary historical `hpstdio.h` use to standard libc directly; `fdelete()` remains a thin source adapter;
- [x] cover the useful `fsystem.h` filesystem subset on top of the clean namespace/metadata/directory services, including adapter-local cwd rather than Stock-global cwd;
- [x] classify raw KOS filesystem calls, raw handles/module helpers, controller sleep/wakeup and multi-volume global state that add no clean application value as Migration/Internal rather than canonical APIs;
- [x] close the representative historical-style filesystem flow in the final Storage E2E: standard/HP-style paths, read/write/seek/truncate, rename, mkdir/rmdir, adapter-local cwd, LFN enumeration, metadata and cleanup.

#### M7.2 completion criterion

The accepted M7.2 stream/directory subset supports typical HP49g+/HP50g file flows; attribute mutation and creation/access timestamp coverage remain open below, while hpclang applications use standard C/POSIX semantics or cleaner typed queries rather than KOS internals. Final focused evidence is **E2E / Storage #53 (`35428824540`)**, fully green across Build, Direct hp39/hp48/hp4950 and Stock hp4950 on `c4b827fe`; **CI / Core runtime #254 (`35428824495`)** is also green on the same head. The representative HPGCC adapter flow includes HP-style/standard paths, read/write/seek/truncate, local cwd, LFN enumeration, metadata, volume size/free queries and cleanup. POSIX `ftruncate()` extension is explicitly zero-filled and regression-tested.

### M7.3 — Calculator objects, Saturn/RPL and stack integration

**M7.3a–j are complete and accepted for their documented scope.** Historical application-flow validation and the individually approved Port 0/1/2 follow-ups remain listed below. The ownership/API boundary is frozen in
[Calculator object service design](object-service.md). The closed M7.3a-e baseline provides a
runtime-private Saturn view, conservative proven-boundary parsing, opaque owned
snapshots, typed construction/conversion, read-only stack observation, bounded
failure-safe stack mutation, read-only classic-RPL named-store handles, HP39
native Aplet/Program/Note/Variable reads, and the bounded HPGCC
`satdir.h` / `saturn.h` source adapters needed by representative 1.1/2.0
applications.

- [x] define the boundary: public objects are opaque owned snapshots; runtime
  Saturn nibble addresses remain an explicit private type and never ordinary
  ARM pointers;
- [x] define copy/borrow/ownership/error rules: reads return owned immutable
  snapshots, no borrowed Stock-memory alias escapes, and mutation is
  failure-safe;
- [x] factor the launch-preserved `SAT_CPU->Read[]` mapping into one private
  20-bit Saturn reader/writer; arbitrary public poke remains out of scope;
- [x] implement a conservative object-span parser: only fixed, verified
  length-prefixed, SEMI-terminated, tagged, directory, or otherwise proven
  formats are snapshotted; unknown/unfrozen prologues are rejected rather than
  guessed;
- [x] implement HP48gII/HP49g+/HP50g stack observation through
  `DSKTOP`/D1 + `EDITLINE`, with 1-based level addressing and owned
  `stack_peek` snapshots;
- [x] implement exact BINT/ZINT ↔ integer support, classic DOREAL conversion and
  finite-double construction, raw CHAR codes, and explicit-length DOCSTR bytes
  without pretending calculator bytes are UTF-8;
- [x] implement bounded `push/pop/drop/replace`: launcher-native pages are
  fenced, the historical Launcher-V3 level-1 bias is normalized/restored,
  TEMPOB allocation respects Stock `AVMEM`, pop snapshots before drop, and
  replace copies before pointer commit;
- [x] validate the mutation core on original Stock HP48gII and HP49g+/HP50g
  with a self-contained CHAR A/B round trip that returns to the original stack
  depth; Core #296 and the successful Stock jobs in Runtime #241 remain the
  focused historical evidence;
- [x] implement HP48gII/49g+/50g read-only named RPL object exchange through
  logical store handles and independent iterators without changing Stock cwd;
- [x] implement HP39 native Aplet snapshots and Program/Note catalog/body lookup
  using the real DORRP / `PGMCXT` / `NOTESCXT` structures rather than a
  synthetic HOME hierarchy;
- [x] recover the HP39 Aplet-variable schema/descriptor model and close generic
  descriptor-driven recall by quoting the genuine extended ID and invoking
  Stock `@`; Runtime #320 validates `NoteText` on both HP39gs and HP40gs;
- [x] implement high-level HPGCC 1.1/2.0 `satdir.h` navigation,
  get/recall/search and REAL helpers above owned object-store handles;
- [x] implement the bounded `saturn.h` application subset: lifecycle,
  depth/drop/dropn, String push/pick/pop, historical textual int/double pushes,
  exact ZINT push/pop, classic REAL push/pop, CHAR push, and failure-safe
  `stdargs`;
- [x] validate representative compatibility behavior in Core #396/#399/#400/
  #402/#404, including the bundled HPGCC `stack.c` flow and failed typed
  conversions that preserve the stack;
- [x] close cross-family Stock execution and return with **E2E / Runtime #338
  (`35812005032`) on `ba2c0398`**: Build, all Direct families, Stock
  HP39gs/HP40gs, Stock HP48gII, Stock HP49g+/HP50g and evidence publication are
  green. The focused adapter witness restores the incoming stack depth after
  ZINT/REAL/String exchange and only consumes `stdargs` on an initially
  empty stack;
- [x] freeze the compatibility boundary instead of chasing header counts:
  low-level address-returning `satdir`, `sat_peek/poke/map_s2a/memmove`,
  raw native-BCD/list helpers, and borrowed stack descriptors are
  Migration/Internal as historical signatures. Their useful list/numeric behavior
  and transactional `swap/nip` equivalents are required by M7.3i;
- [x] close M7.3e at the bounded HPGCC 1.1/2.0 adapter baseline. Historical
  `satdir.h` remains read/search/recall oriented, so the adapter closure does
  not depend on exposing HPGCC3 raw/native mutation interfaces;
- [x] **M7.3f — accepted:** public HP48gII/49g+/50g logical-directory
  put/remove and HP39g+/39gs/40gs descriptor-driven variable get/set are
  implemented. The final gate covers nested references, unchanged cwd/context,
  owned-input lifetime, error handling, size-changing HP39 writes across GC,
  and settled Stock return. Formal CI acceptance is recorded in
  [Validation](validation.md#m73f-final-implementation-batch).

The implementation and limits are described in
[Object service](object-service.md#m73f-public-mutation-and-native-variable-lifetime).
M7.3a-e's read/stack/adapter baseline remains accepted. The new public mutation
APIs close M7.3f without exposing HPGCC3 raw native pointers or inventing HP39
directory semantics. M7.3h–j are now formally accepted at `6a7f9a28`; remaining parity follow-ups below are retained.

### M7.4 — Common system services

**Complete and accepted at functional head `06ab4deb`.**
The six-model Direct/Stock gate and cross-workflow regressions pass; see [validation evidence](validation.md#m74-formal-acceptance).

Prioritize application capabilities that appear repeatedly in HPGCC programs and have a coherent stock-hosted implementation:

- [x] wall-clock/RTC date-time query and conversion;
- [x] tone/beep with explicit frequency/duration and safe host restoration;
- [x] useful battery/power/contrast/status queries where semantics are stable across families;
- [x] capability discovery for services that are genuinely model-specific;
- [x] classify clock-rate control, raw interrupt/timer programming, cache/MMU manipulation and physical allocation as low-level Migration/Unsupported unless an ownership-safe application use case justifies a typed service.

Additional M7.4 acceptance (preserving the expanded M7 requirements):

- [x] civil-time setting with forward/backward adjustment, valid date ranges and monotonic-deadline continuity;
- [x] cooperative one-shot/periodic expiration counts and cancellation;
- [x] bounded UTF-8 input, accept/cancel and bounded error/message presentation;
- [x] model/context capability discovery and deterministic unsupported/error results;
- [x] writable emulator RTC calendar, alarm-clock consistency and saved-state continuity;
- [x] six-model Direct/Stock regression with unchanged borrowed hardware state and settled return.

Implementation: `datetime.h`, `timer.h`, `sound.h`, `power.h`, `input.h`, device
capability queries, contrast reads and bounded font rendering. System-service
probes use a separate `system-services` payload in the Runtime workflow matrix;
this preserves sufficient heap for the HP48 Runtime/object probes.
Physical audio output still requires a hardware witness; simulator results prove
configuration/timing/restoration, not audible speaker behavior.

### M7.5 — Remaining display/runtime parity needs

**Implemented and formally accepted at integrated head `6a7f9a28`.**

The clean display service owns on-demand double buffers, validates every DMA
page and LCD bank boundary, publishes caches, and supports physical 2/4/16-level
modes, bounded frame synchronization, page exchange and six status segments.
Explicit end and normal runtime shutdown restore writable LCD configuration
before releasing memory. Host tests cover fragmentation, low-memory rollback,
timeouts and restoration order. Display E2E covers every selected Direct/Stock
model with pixel/register oracles and before/PASS/return gates.

A public-API-only HPG prototype demonstrates intersecting clips, PAINT/XOR,
surface patterns, clipped line masks and transparent RGB XPM import. Historical
state/spelling and broad parser compatibility remain in the planned M8 adapter;
these witnesses found no missing clean drawing primitive. No goal below is
removed. The integrated CI gate accepts the implemented scope below.
Real-panel flicker/luminance and electrical timing are not emulator claims.
See [M7.5 validation](validation.md#m75-display-runtime-local-acceptance).

- [x] determine whether physical gray4/gray16 and page flipping need an on-demand contiguous/DMA resource broker;
- [x] cover historical annunciator/status-plane behavior through a Stock-safe abstraction and representative application tests;
- [x] retain clip/style/pattern/XPM compatibility state in the future HPG adapter unless independent hpclang applications demonstrate a cleaner core need;
- [x] validate any temporary LCD ownership against the existing before/PASS/return state-restoration gates.

### M7 parity closure

Before calling M7 complete:

- [ ] every proven ordinary HPGCC application capability has a working clean equivalent; any omitted capability has an individually documented, maintainer-approved limitation. A Migration/Unsupported signature label alone does not close the capability;
- [ ] representative historical applications cover graphics/text, keyboard, time/system, filesystem, and calculator-object/stack flows;
- [ ] all supported hardware families expose coherent capability queries and deterministic failure for absent services;
- [ ] Stock E2E proves host-state restoration for every service that temporarily touches calculator-owned state.

M7 completion therefore means **practical HPGCC capability parity**, not binary compatibility and not literal reproduction of every unsafe low-level API.

## M8 — HPGCC source compatibility, SDK UX, and post-HPGCC extensions

**Planned, with adapters allowed to begin incrementally once each M7 clean service is stable.**

M8 turns M7's capability parity into broad source-level compatibility and then moves beyond the historical SDK. The common 59-function `hpgraphics.h` surface is signature-stable across HPGCC 1.1, HPGCC 2.0 and HPGCC3, and the historical keyboard/stdio/filesystem/object inventories give similarly concrete adapter targets. Binary compatibility remains out of scope.

M8 goals:

- provide maintained compatibility headers/adapters for high-value HPGCC 1.1/2.0 source surfaces on top of the clean core;
- compile representative historical HPGCC applications with minimal mechanical changes and document unavoidable migrations;
- establish stable public hpclang headers, examples, packaging/build UX and a fresh LLVM-only developer workflow;
- validate non-trivial applications on each platform family rather than only isolated API probes;
- provide migration tooling/guidance that distinguishes safe source adapters from intentionally retired raw KOS/Saturn/MMU idioms;
- add a renderer-agnostic declarative UI core: keyed identity/reconciliation, deterministic row/column layout, focus/navigation and event/action routing, controlled inputs, explicit async state/effect integration, dirty-region invalidation, and a canvas escape hatch for custom drawing;
- keep visual style, widget chrome, concrete rasterization, and calculator-native look-and-feel outside that UI core behind renderer/theme backends; downstream environments such as 39gsX may provide a renderer that reproduces the independently reverse-engineered native HP39 UI without making that style an hpclang ABI;
- use the clean declarative UI engine as a possible substrate for useful historical `win*` source adapters where semantics fit, rather than allowing the historical widget implementation to define the core;
- **surpass HPGCC** with cross-family capability discovery, modern C++ wrappers, UTF-8/font selection, stronger ownership/errors, deterministic assets/tests, declarative UI composition, and new services that are practical on the hardware even when no historical API existed;
- consider replacing the remaining ARMToolbox/library-275 stock-launch boundary with a minimal independent launcher if it materially improves packaging or portability;
- separate public application documentation from this internal Development reference.

## Additional M7–M8 acceptance requirements

The following requirements supplement the existing milestones and preserve the
accepted M7.3a–f evidence. Historical source evidence is recorded in
[HPGCC compatibility](hpgcc-compat.md).

### M7.0 follow-up — capability and evidence ledger

- [ ] Track historical declaration, implementation, clean equivalent, source
  adapter and validation separately, per model and execution context.
- [ ] Give each historical application capability an explicit destination;
  require maintainer approval for an actual omission rather than treating a
  raw historical signature as sufficient reason to omit its useful behavior.
- [ ] Define runtime discovery for supported operations and context-dependent
  availability before new object/system services proliferate.
  - Accepted on `9489e7d1`: distinguish classic RPL read access from mutation availability,
    share launcher-bound validation with the mutation backend, and validate the
    HP39 portal before reporting Aplet services. Host and calculator probes cover
    model/context differences; the broader historical capability ledger remains open.

### M7.1 / M7.2 follow-up — uncovered application behavior

- [x] Establish HP39g+ native-font correctness using its own ROM/pixel evidence.
  - D1.55 main/mini/EditLine tables are located and implemented through the
    private ROM window. Local Direct and Stock USB pixel/return checks pass on
    all three HP39 models; formal CI acceptance is complete on `8f79c156`.
    HP39 USB transfer reliability remains a separate follow-up. The physical keyboard
    contract is unchanged.
- [ ] Specify text/Alpha-plane, bounded text/numeric input and console behavior
  above the accepted physical keyboard core.
  - The bounded UTF-8 editor in `hpclang/input.h` already supports insertion,
    deletion, navigation, accept/cancel and basic physical-key editing.
  - Numeric extension accepted on `b92e2a7d`: exact signed 64-bit and finite
    double conversion from the bounded editor, full-line syntax/range checking,
    and correction after a rejected accept attempt. Alpha/Shift text policy,
    console event/rendering behavior and historical blocking adapters remain open.
  - Console extension accepted on `b92e2a7d`: owned fixed-cell UTF-8 output,
    wrapping/scrolling, clipped viewport rendering and explicit standard-stream
    binding with read/write/flush callbacks, distinct EOF/error/cancellation and
    reset on ownership changes. Alpha/Shift text maps and a ready-made keyboard
    line-input loop remain open; physical keyboard semantics are unchanged.
  - Line-session extension accepted on `c7451364`: callback-driven
    event/edit/accept/update loop, recoverable capacity/validation errors and
    explicit cancellation. The scripted session test supplies callback coverage;
    physical-key integration has separate acceptance below.
  - Explicit Latin Alpha lookup accepted on `c7451364`: all six models'
    printed A-Z/a-z and Alpha-space positions, checked against their manual
    diagrams and tested through physical key identities. Punctuation/symbol
    lookup remains open; no implicit modifier state is added to the keyboard core.
  - Default text-plane behavior follows each Stock system's one-shot, held and
    locked Alpha rules. HP39 note-editor lock is an application action; do not
    substitute the HP48/49/50 double-Alpha lock gesture. The Latin HP39 map is
    cross-checked against actual Stock input on all three models as well as
    the below-key manual legends; physical integration is accepted below.
  - Caller-owned native text-mode state accepted on `c7451364`: per-model
    one-shot/held Alpha, classic double-Alpha lock and case selection, HP39
    menu lock with temporary suspension, explicit symbol/command planes and
    status for application rendering. Native ROM probes establish the transition
    cases; host tests cover all six models and simultaneous-event ordering.
    The scripted editor/console composition is additional integration coverage.
    Full symbol
    lookup and a ready-made keyboard editor remain open.
  - Physical text integration accepted on `c7451364`, integrated into main:
    acknowledged GPIO transitions pass through the SDK scanner/debounce queue,
    native text mode, bounded editor and console. Single/held Alpha and a
    separate model-native lock sequence have independent pixel references;
    every injected key must be released before PASS. This extends the earlier
    scripted coverage; full symbol lookup and a ready-made editor remain open.
    Runtime, Core, Gallery, Storage, Quality and Objects all pass on that head.
    The merged text-input, text-mode and physical-input branches are retired.
- [x] Cover historical file attribute mutation and creation/access timestamp
  queries, including persistence after close/reopen and unsupported cases.
  Implemented in `m72-file-metadata`: clean masked attribute updates, stored FAT
  creation/access-date queries and `FSChAttr`/`FSGetCreatTime`/`FSGetAccessDate`
  adapters. Local persistence/error tests, six-model Direct and both SD-model
  Stock Storage pass; integrated CI acceptance is complete on `6a7f9a28`. This does not add
  automatic access-date maintenance or timestamp setters.
- [ ] Audit media-change and ownership behavior; retain the unresolved Stock
  HP49/50 Storage launch intermittency as a reliability investigation.
  - Pre-takeover insertion/protection refresh now uses read-only GPIO sampling
    instead of repeating Stock `CHECKINSERT`; regression acceptance is complete
    on `256e8496` (Storage, Runtime, Objects, Core and Quality).
    The owned-media extension below covers observed removal and recovery;
    this does not close the complete media-change goal.
  - Accepted on `56e4a9dd`: resample presence/protection during owned I/O, invalidate on
    observed removal or block-transfer failure, remount for new path operations,
    and reject old file/directory objects without flushing them to the new volume.
    Host tests exercise these transitions; Storage, Runtime, Objects, Core and
    Quality CI pass. Cycles entirely between observations
    and physical hot-plug validation remain open. On 2026-10-11 the maintainer
    confirmed an HP49g+ and SD card are available, requested a reproducible
    hardware test package, and designated the later physical run as a
    nonblocking follow-up. Preparing that package remains part of M7 closure;
    emulator results must not be reported as physical hot-plug evidence.

### M7.3g–j — object-service capability completion

M7.3h–j implementation and local validation are complete as of 2026-10-05;
formal CI acceptance is complete on `6a7f9a28`. Local evidence covers
all six Stock models, all three HP39 15-invocation Reset and eight-invocation
composite lifecycles, the dedicated HP39 catalog fixture, 47 host tests and
host/ARM static analysis. See [Validation](validation.md#m73h-j-local-completion).

- [x] **M7.3g:** directory creation, empty removal and explicit recursive tree
  removal; nested references, active cwd/ancestor safety and failure preflight.
- [x] **M7.3h:** HP39 Aplet-owned data read/replace/clear, GC-safe lifetime,
  per-instance isolation, reset behavior and persistence across exit/relaunch.
- [x] **M7.3i:** owned list/array/matrix construction and access; complex,
  identifier/tag/binary exchange; explicit wider-number precision; transactional
  stack reorder and Boolean exchange.
- [x] **M7.3j:** investigate HP39 Program/Note/Aplet creation/removal and
  Port 0/1/2 namespaces; produce tested operations or individually reviewed
  limitations for each actual native object model.
  - Port 0 (main RAM), Port 1 (extended RAM) and Port 2 (Flash): the maintainer
    accepted each port's current lack of read/write services as an M7.3j
    limitation on 2026-10-04. This does not mark port services implemented.
- [ ] Validate genuine historical application flows using the four HPGCC
  generations and HP39 Aplet template, not only scalar API probes.

### Native port services — retained follow-up

- [ ] Port 0: enumerate/read/store/remove native backup and library objects;
  define RAM ownership, active library safety and failure behavior.
- [ ] Port 1: provide the same native object operations with explicit handling
  of banked/extended RAM and its interaction with the ARM allocator.
- [ ] Port 2: provide native Flash object operations, including erase/write
  transactions, full-media behavior and persistence. Preserve library and
  backup semantics; these ports are not HOME directories or stream files.

### M7.4 / M7.5 — expanded acceptance requirements

- [x] For RTC, define civil-time validity/conversion and separately decide
  whether/how callers may set time; do not invent timezone semantics.
- [x] For sound, define frequency/duration/cancellation and restore shared
  hardware state; distinguish emulator activity from audible hardware evidence.
- [x] Add contrast/status/power observations, safe deadline/periodic events,
  bounded input/error presentation and model/context capability discovery.
- [x] Prove physical gray4/gray16 and flipping/vsync requirements against
  temporary DMA/contiguous-memory ownership, low-memory behavior and return.
- [x] Prototype HPG clip/style/PAINT-XOR/pattern/XPM and annunciator behavior
  over the clean services; identify missing primitives rather than assuming
  that existing geometry is sufficient for a future adapter.

### M7 final audit / M8 follow-up

Current `m7-parity-completion` work adds model-native Unicode symbol lookup,
the default bounded keyboard editor, contrast mutation/restoration and owned
LCD enable/disable. Local validation is in progress; these additions do not
close the remaining capability ledger, historical client or media-change goals.
The HP39 transport regression now covers embedded CRLF bytes: all three Stock
models pass the corrected newline transport locally. This is a byte-preservation
fix, not a retry or relaxation of the application result checks.

- [ ] Exercise a stateful HP39 Aplet, nested directories, numerical composites,
  files, text/keyboard, graphics and time/system clients on their actual models.
- [ ] Retain the existing M8 compatibility, C++, migration and declarative-UI
  goals; add installable SDK/examples, console/text adapters and representative
  HPGCC3 clients to the compatibility ledger.
- [ ] Keep missing clean capabilities in M7; reserve M8 for historical spelling,
  higher-level policy, SDK usability and the already planned extensions.
