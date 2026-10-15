# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/maxmixalex/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Scope of the completed milestones

M0–M6 describe the **execution foundation**: compiler/runtime viability, the HPC1 ABI, calculator launch paths, stock-system return, packaging, fixtures, and automated E2E.

They do **not** mean that hpclang already reproduces the application-facing feature set of HPGCC 1.x, HPGCC 2.x, or HPGCC3. M7 now provides stable device facts, monotonic time, the complete six-model physical keyboard/event core, opaque 2/4/16-level software surfaces, geometry, native/generated font and text rendering, hp4950 stream stdio through picolibc, and accepted M7.3a–j object/stack/directory, composite-object, HP39 Aplet persistence/Reset and native-catalog services. M7.4 common system services and M7.5 display/runtime services are accepted. M7 capability parity is accepted at functional head `2988e1de`; the individually reviewed native-port limitations retain implementation goals below.

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

**The existing M7.1/M7.2 acceptance remains valid for its tested scope. M7.3a–j object services, M7.4 common system services and M7.5 display/runtime services are accepted. Text/numeric/console behavior, media-change handling and historical graphics/keyboard/file/object clients are now accepted through `7303a3d6`. Overall M7 is complete within the individually approved scope: functional head `2988e1de` passes Core, Quality, Runtime, Objects and Storage; Gallery passed on `216bb5d5` and was not retriggered by the final cleanup-only repair. See [final acceptance](validation.md#m7-final-acceptance). Native Port 0/1/2, whole-calculator reset and reliable hardware serial-number discovery remain individually approved follow-ups.**

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
- [x] finish conventions only when remaining object/UI services expose genuinely new ownership, error, string/buffer, or capability-query requirements.
  - M7's implemented object, catalog, input, power and termination services now
    document ownership, supported contexts, error channels and buffer lifetimes.
    Normal termination includes registered-stream cleanup; immediate termination
    retains hardware restoration. Future declarative-UI policy remains in M8.

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

M7.2 covers the HP49g+/HP50g **SD-card FAT namespace** and byte-stream semantics only. HOME and native Port 0/1/2 are calculator-object namespaces, not FAT volumes. HOME services are accepted under M7.3; the individually approved native-port implementation goals remain below.

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

The accepted M7.2 stream/directory subset supports typical HP49g+/HP50g file flows. Attribute mutation and creation/access timestamp queries were subsequently accepted on `6a7f9a28`; four-volume and media-identity acceptance is recorded below. Applications use standard C/POSIX semantics or cleaner typed queries rather than KOS internals. The original focused evidence is **E2E / Storage #53 (`35428824540`)**, fully green across Build, Direct hp39/hp48/hp4950 and Stock hp4950 on `c4b827fe`; **CI / Core runtime #254 (`35428824495`)** is also green on the same head. The representative HPGCC adapter flow includes HP-style/standard paths, read/write/seek/truncate, local cwd, LFN enumeration, metadata, volume size/free queries and cleanup. POSIX `ftruncate()` extension is explicitly zero-filled and regression-tested.

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

M7 closure gates, accepted on `2988e1de`:

- [x] every proven ordinary HPGCC application capability has a working clean equivalent; any omitted capability has an individually documented, maintainer-approved limitation. A Migration/Unsupported signature label alone does not close the capability;
- [x] representative historical applications cover graphics/text, keyboard, time/system, filesystem, and calculator-object/stack flows;
- [x] all supported hardware families expose coherent capability queries and deterministic failure for absent services;
- [x] Stock E2E proves host-state restoration for every service that temporarily touches calculator-owned state.

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

### M8 stages

The following decomposition was approved for implementation on 2026-10-12. It adds
delivery and acceptance boundaries to the goals above; it does not remove any
existing goal or reopen the accepted M7 scope. The native-port follow-up is
scheduled explicitly, and declarative UI receives early independent stages.
Public UI names and renderer contracts remain unfrozen until M8.2 design review.

| Stage | Deliverable | Completion evidence |
| --- | --- | --- |
| M8.0 | Native Port 0/1/2 object services | Model-specific native enumeration/read/store/remove, ownership and persistence tests; explicit unsupported contexts |
| M8.1 | Consumable SDK foundation | An external C and C++ application builds from an installed SDK in a clean container, without source-tree paths or local tools |
| M8.2 | Declarative UI state, layout and interaction core | Stable identity, deterministic layout, focus/input and effect lifecycle tested independently of pixels; a small interactive calculator application |
| M8.3 | UI components and renderer/theme backends | Reusable controls, a usable default renderer, a second backend demonstrating visual independence, and six-model interaction/rendering evidence |
| M8.4 | Maintained HPGCC source adapters | Historical graphics, input, system, file and object applications compile through adapters with documented mechanical changes |
| M8.5 | Modern C++ and migration experience | Owned C++ wrappers, actionable migration guidance/tooling and public examples exercise the same clean contracts |
| M8.6 | Integrated applications and SDK release acceptance | Non-trivial downstream applications, packaging/documentation and cross-family resource/return acceptance |

#### M8.0 — Native port services

Local implementation now includes Port 0 backup create, variable-size replace
and remove, with owned reads and preservation of live references to old records.
The native library table participates in reference scanning. Library record
creation was initially internal and does not register or attach a library. Existing-library
relocation, public library mutation, Port 1 replacement/removal and Port 2 writes remain unfinished. The 85 host
tests and focused mutation/port sanitizer tests pass. The public grow/shrink/
read/remove flow passes Stock on HP48gII, HP49g+ and HP50g. All three also pass
a three-launch lifecycle: create an integer, return/relaunch and replace it
with a string, then return/relaunch and verify/remove the string. No reinstall
or calculator reset occurs between launches. These results do not close M8.0.
The subsequent public `hpclang_port_library_create` entry now exposes Port 0
record creation from an owned library snapshot, with a separate capability bit,
CRC validation and duplicate-ID refusal. All 96 host tests, focused sanitizers
and ARM analysis pass. The independent `port-library` E2E passes both launches
on HP48gII, HP49g+ and HP50g: native RCL reads the record between launches, the
owned witness matches, duplicate creation is refused and Stock return settles.
HP48 uses its own L275 installation order in the setup contract. Formal CI for
this addition is pending. Registration, attachment,
library replacement/removal and relocation remain separate unfinished work.
Port 1 library creation now uses the same bounded ERAM append as backup records,
without moving existing objects. The public capability query requires the
launcher-owned ERAM boundary. All 96 host targets, focused sanitizers and ARM
analysis pass; the expanded `port-library` fixture passes both Stock launches
on HP49g+ and HP50g, including independent original-system RCL of both ports.
Library registration/attachment, reference-preserving replacement/removal and
Flash reclamation remain open. This addition is local pending a batched push.
Port 1 enumeration and owned backup/library reads are implemented for HP49g+
and HP50g; native-RPL-created backup enumeration/readback passes Stock on both
models across the three-launch Port 0 lifecycle. Port 1 backup creation now
respects a launcher-derived physical-page fence, passes both models across
relaunches and is independently recalled by native RPL. Reference-preserving
replacement/removal and active-library safety remain open.

Port 2 owned enumeration and backup/library reads now pass HP49g+ and HP50g
Stock across all three lifecycle launches. The backend validates the live
Flash mapping and bank identities, and uses a separate slot iterator that skips
native deletion markers. Host tests cover cross-bank reads and snapshot
ownership. Port 2 writes and library mutation remain open.
The internal SST command engine now identifies documented chip/CFI geometry,
excludes firmware addresses from programming/erasure, and owns uncached Flash
mappings until the chip is ready. Its isolated physical write/erase fixture
passes local HP49g+/HP50g Stock and normal return, with byte-for-byte unchanged
Flash images afterward; all 93 host targets pass. A RAM-backed block writer
checks the entire range before programming under one mapping lease. Its bounds, late-conflict refusal,
identical rewrite and neighboring-byte checks pass both Stock models, with
byte-for-byte restored Flash after erasure. The tail scan preserves deleted
allocations, validates live records and checks every remaining nibble before
treating an erased end marker as writable space. An internal rollback planner
can cover uncommitted body bytes with a deleted allocation using only 1-to-0
header transitions. Host fault injection covers interrupted body/header writes
and unreadable input. Status-write interruption, space reclamation and complete
transaction recovery remain open.
Local HP49g+/HP50g experiments confirm why rollback is necessary: native STO
overwrites an FF-marked dirty tail and produces an invalid object. Converting
the uncommitted record into a deleted slot permits subsequent native STO/RCL
to succeed, with unrelated Flash bytes unchanged.
The internal backup append engine now verifies a CRC-valid source and the
reserved erased range, verifies each programmed word, then publishes status.
Returned write/read failures trigger a deleted-slot rollback, including failed
commit acknowledgement; an unverified cleanup has a distinct dirty result.
Both byte alignments, partial writes, silent write failures and each read-failure
position pass host fault injection. All 94 host targets pass. The Stock backend
holds one mapping lease and rejects firmware, bank headers and cross-bank records.
Public Port mutation integration and restart recovery remain open.
An internal recovery executor now applies the dirty-tail plan under one Stock
mapping lease, verifies the extent before publishing the deleted status, and
rescans the resulting chain. It preserves live predecessors and rejects unknown
status bytes without mutation. Host tests cover every interrupted body bit,
each recovery read/write failure, partial writes and idempotent recovery.
HP49g+/HP50g Stock tests pass recovery followed by backup append/public read,
repeat recovery without changes, normal return and byte-for-byte Flash restoration.
This handles an FF-marked interrupted body; partially published status bytes,
restart integration and power-loss transaction guarantees remain unfinished.
Whole-port explicit recovery now preflights all six banks and can tombstone
complete, CRC-valid backups with any hidden non-`FF` status,
preserving their contents and native invisibility. Fault tests bypass append
cleanup and restart using only Flash bytes. Partial library statuses,
replacement intent, startup integration and sector reclamation
remain open; this does not close the power-loss requirement.
Discarding a hidden status now clears and verifies a required live bit before
clearing the remaining bits: a cut cannot accidentally produce live status `53`.
Host tests exhaust the status-clear masks at both byte alignments and interrupt
whole-port recovery without caller cleanup. Incomplete-tail cleanup verifies
zero metadata and an exact discard extent before clearing status bit zero.
Subsequent even hidden states require a bounded allocation covering that
revalidated tail plan and an erased remainder. Failed-append cleanup records
the same marker while retaining its reserved extent; tests fail every append
step and then interrupt each cleanup step. Repeated cuts in extent, metadata
and status programming resume from Flash
alone, preserving preceding records and partial bodies. Complete backups retain
CRC checks and their original metadata. This is host fault-model coverage;
automatic startup recovery and the full power-loss requirement remain open.
Normal Port 2 mutations now prepare the port with recovery under the same Flash
mapping lease. Preflight errors do not write; once programming starts, transient
read/write errors are retried while ownership is retained. The public-create
fixture includes a hidden backup from an interrupted prior operation and requires
successful recovery without calling the internal recovery API. Port 2 reads and
enumeration now prepare recovery too, covering committed and partial new values
without an intervening write request. Capability queries and owned iterator
reads remain non-mutating. Calculator startup and library lifecycle remain
separate work; the full power-loss goal is unchanged.
Replacement-intent encoding/decoding now has a bounded 44-byte hidden format,
allocation identities and CRC32 protection. Host corruption/bounds tests and
native visibility checks of actual C-produced records pass on HP49g+/HP50g.
Read-only replacement planning validates every bank, requires a unique old
backup, computes both allocation CRCs and selects space for the intent and new
record, including separate banks in either order. Full-media, late corruption
and read failures leave Flash and the output plan unchanged. The format is
now emitted by the backup replacement API. Publication verifies the body
before setting hidden status `7E`; whole-port recovery can retire the intent if
the old backup still matches and the new reservation remains erased. Publication
cuts and returned errors preserve both value regions. A committed new value now
supports validated old-status completion and intent retirement, including all
old-deletion states and repeated recovery cuts. Allocation-boundary/name/checksum
checks and a single-active-intent gate reject inconsistent transactions without
writes. Incomplete new bodies hidden by FF now roll back using the exact
reservation and a durable zero-metadata discard marker, retaining the old value.
The intent is resolved before generic cleanup regardless of bank order; host
tests cover interrupted body programming and repeated rollback cuts. Partial
FF-to-53 publication now finishes only after validating a ready intent, live old
value and complete matching new body, then deletes the old value. All new status
bytes and repeated publication cuts are covered. Normal replacement now uses
the journal and durable recovery for completion and returned errors, retaining
mapping ownership until the old/new outcome is verified. Actual replacement
write cuts recover without transaction RAM; legacy duplicates without an intent
remain rejected. Automatic startup integration and reclamation remain open.
For a caller-owned pending backup with retained expected bytes, an internal
commit-finishing primitive validates the expected record and compares every
stored body/padding byte before completing the status. It accepts only status
bits reachable on the way to native `53`, changes no body bytes, and supports
an already completed commit. Host tests cover all 256 starting status bytes,
every partial status-write mask, read failures, mismatched bodies and repeated
completion. This requires known transaction intent; it does not classify an
unknown damaged record or establish automatic recovery after restart.
HP49g+/HP50g Stock acceptance completes a retained backup from status `73`,
reads integer 987 through the public API, repeats completion without mutation,
returns normally and restores the whole Flash image byte-for-byte. All 94 host
targets, focused sanitizers and ARM static analysis pass for this primitive.
An internal whole-port create operation now validates all six native bank
identities, existing records and erased tails before selecting a slot and
appending under one mapping lease. A name in a later bank prevents an earlier
write; invalid/dirty/unreadable later banks also fail before mutation. Tests
cover last-bank placement, full media without reclamation, duplicate names and
unchanged existing allocations. All 95 host targets pass; HP49g+/HP50g Stock
acceptance passes automatic placement, public readback, duplicate refusal,
normal return and byte-for-byte whole-Flash restoration.
The public `hpclang_port_backup_create(2, ...)` now encodes owned snapshots and
uses this operation; discovery advertises creation on HP49g+/HP50g Stock.
The backend retains the selected slot, expected record, IRQ state and mapping
until a failed append has been verified as deleted. Therefore ordinary return,
`exit()` and `_Exit()` cannot bypass cleanup. Persistent unrepairable hardware
retains RAM execution, matching the existing permanently-BUSY handling.
All 95 host tests pass, including public snapshot encoding/readback, cross-bank
duplicate rejection and recovery after repeated programming failures. Public
creation/readback and `EEXIST` pass HP49g+/HP50g Stock with normal return and
independently verified whole-Flash restoration. A separate persistence client
passes three independent Stock launches on both models, including ordinary
return, `exit()` and `_Exit()`. Native RPL recalls value 987 before both relaunches;
the application checks and deletes each HOME witness so a stale value cannot
pass the next run. Independent saved-image comparison finds exactly one new
30-byte allocation, with every unrelated Flash byte unchanged.
Native STO samples across four backup-name lengths establish four-byte
allocation sizes. The encoder now follows that rule while preserving cleanup
compatibility with older SDK halfword-sized records. The short-name `BB`
regression passes all three HP50g Stock launches and matches the native 28-byte
record exactly. All 96 host targets and focused ASan/UBSan checks pass. A separate
internal library encoder matches HP49g+/HP50g native samples; public Port 2
library creation, registration and attachment remain unfinished.
The subsequent public Port 2 library-create implementation now uses that encoder
and the shared exclusive Flash append/cleanup path. It checks IDs across all six
banks and preserves existing addresses. Both HP49g+ and HP50g pass two Stock
launches with native RPL witnesses for libraries stored in Ports 0, 1 and 2.
All 96 host targets, focused sanitizers, ARM analysis and strict docs pass;
library registration/attachment and replacement/removal remain open, as do
Port 1 replacement/removal, Flash reclamation and power-loss recovery.
Port 2 backup removal now validates all six banks before clearing only a unique
backup's native status byte. Payload and addresses remain unchanged; uncertain
status writes are completed before releasing ownership. Independent native
HP49g+/HP50g probes cover stack values, explicit Flash pointers and a program
purging itself. The separate `port-flash-remove` client passes three Stock
launches on both models: remove, native absence witness, recreate, native
presence witness and remove again. Owned snapshots survive deletion. Saved
images contain only the two expected deleted 30-byte records; unrelated Flash
is unchanged. All 95 host targets, focused ASan/UBSan and ARM static analysis
pass, including partial status writes and whole-port preflight failures.
Port 2 backup replacement publishes a durable intent and verifies the complete
new record before tombstoning the old one under exclusive ownership. Incomplete
bodies retain the old value; verified partial publication finishes the new value
before old-status deletion. Returned failures resolve through the same recovery
protocol before releasing the mapping. The initial implementation's
95 host targets pass, including every programming failure position, cross-bank
replacement, grow/shrink, duplicate refusal and full media without old-value
deletion. Focused ASan/UBSan and ARM analysis pass. The independent
`port-flash-replace` client passes three Stock launches on HP49g+/HP50g with
native RPL witnesses for 987 to 12345678901234 to 5, retained snapshots and normal
return/exit variants. Final images preserve all three old payloads and every
unrelated Flash byte. HP39g+ model builds exclude the Flash mutation backend.
Restart integration and reclamation remain unfinished; these mutation results
do not certify reference safety during later reclamation.
Native backup record encoding also matches Stock bytes and passes local
HP49g+/HP50g native RCL/PURGE/STO
interoperability, including unchanged unrelated Flash data. Native Port 2 object writes,
full-media behavior and failure/recovery acceptance remain unfinished. The
fixture does not establish real-hardware cache or NOR timing behavior.
The HP48gII three-launch regression also passes, preserving `ENOSYS` for its
unavailable Port 1/2 services. All 85 host tests, focused sanitizers, Clang
analysis and strict documentation checks pass for this read-service batch.

Local typed discovery now reports native medium, SDK operation masks and
launch-available operation masks through `hpclang_port_query()`. Six-model
host checks distinguish absent ports, Direct/Stock contexts, legacy launch
bounds and supported-but-full ERAM, including the exact legacy descriptor size
under ASan/UBSan. HP50g passes three Stock lifecycle launches; HP39g+ target
discovery acceptance now passes all 15 HP39g+ Stock lifecycle invocations,
including copied Aplets. The fixture reuses bounded scratch buffers so its
large-string checks fit the copied Aplet's smaller ARM heap, with unchanged
payload lengths and persistence/rejection assertions. All 85 host tests pass.

The subsequent HP39gs/HP40gs CI run exposed smaller heaps on later Aplet
launches. HP39 discovery checks now run in the catalog suite (Direct and Stock),
while classic discovery stays beside port mutation. The common probe preserves
the same medium/operation assertions. All three HP39 Direct catalog targets and
all 15 HP39gs and all 15 HP40gs Stock mutation launches pass locally; the
Stock catalog check is being verified before the follow-up push. Two HP40gs
attempts received an Aplet missing one byte from a doubled backslash, despite
successful Kermit completion. Quoting backslashes in the transfer copy makes
the same payload pass Stock. The server now applies that quoting without
changing source packages; PTY tests cover byte preservation and cleanup.
The production server passes HP40gs Stock Catalog with the originally failing
payload. The explicit retained byte witness also passes HP39g+ Stock Catalog,
including its normal return. Formal CI for this follow-up remains pending.

Internal Port 1 replacement/removal now requires an explicit read-only reference
guard before any write. Missing guards, live-reference rejection and guard read
errors preserve the port bytes. All 85 host tests, focused sanitizers and static
analysis pass. Native banked-reference discovery and public Port 1 replacement/
removal remain unfinished; an imported ACPTR experiment demonstrates that native
PURGE can leave a stale ERAM pointer, so native PURGE alone is not a safety proof.

A bounded internal library-reference visitor now validates the library CRC,
relative table bounds and link-table shape, then visits hash/message/link/config
objects and function targets. It handles backward relative links and propagates
visitor/read errors without writing native memory. All 85 host tests, focused
ASan/UBSan and Clang analysis pass; the same C implementation enumerates 16
references from library 275 in a saved HP50g native state. This supplies one
structural traversal component; complete live-root and executable-reference
coverage, and public Port 1 replacement/removal, remain open.

The internal compact-array visitor supplies each element's shared type and
bounded body without writing a synthetic prologue into native memory. Tests
cover strings, multidimensional numbers, banked-pointer bodies, empty arrays,
truncation, overflow and callback cancellation. The same C implementation reads
all 13 strings in library 275's native HP50g message array with an exact ending
boundary. Linked-array traversal also handles shared and reordered bodies using
bounded relative links; synthetic fixtures cover links into the header/table,
past the array end and truncated bodies. A native linked-array instance has not
yet been validated. Address-dependent directory bodies and out-of-array links
are rejected; this component does not by itself certify reference safety.

The internal sequence visitor distinguishes inline children from five-nibble
reference cells in lists, programs, symbolic objects and matrices. It validates
the enclosing sequence before invoking callbacks and leaves reference targets
unread. The combined directory/library/array/sequence experiment reaches 126
objects in a saved HP50g HOME graph and identifies the imported ERAM ACPTR.
Flash-pointer resolution, executable references and complete live-root coverage
remain open; this experiment is not authorization to move referenced Port 1 data.

A read-only root visitor now reuses the native relocation scanner for launcher
roots, data/return stacks and its supported system tables. It reports raw values
without assuming they are object beginnings; tests verify callback cancellation,
read failures and byte-for-byte preservation. The saved HP50g return stack also
contains execution addresses outside the HOME object graph, so interpreting these
roots and covering banked library entries remain necessary before enabling writes.
The C visitor enumerates 422 cells (51 distinct values) in the saved HP50g state
without writes. A separate TEMP allocation visitor validates the complete footer
chain before reporting payload bounds and reproduces all 40 saved allocations.
Three allocations contain a ROM-pointer object followed by a complete program,
and some roots point inside those blocks. Bounded parsing accounts for 43 typed
objects across the 40 allocations with no leftover nibbles in this saved state.
The trailing programs must enter reference traversal too; their Flash/ROM calls
still need resolution before execution-reference safety is established.
The combined HOME/TEMP experiment reaches 174 objects and reveals another Flash
pointer. In this HP50g snapshot, native catalog entries for ROM calls 161/10,
161/6 and 167/1 resolve to programs that match the three TEMP copies byte for
byte. This establishes their origin, not complete executable-reference coverage
or a portable ROM mapping for all models.
The bounded internal library-catalog lookup preserves the native bank-switch
identity and leaves its output unchanged on missing IDs, duplicate matching IDs
or read/bounds errors. Its C implementation matches all 62 records in the saved
HP50g catalog. This supplies binding metadata; bank resolution and executable
target traversal remain separate unfinished steps.
A bounded command resolver now validates the selected bank's library-ID header
and relative binary link table, preserves backward links and rejects table/target
addresses outside the caller's mapping. The actual C implementation resolves
161/10, 161/6 and 167/1 to the three independently matched HP50g ROM programs.
Selecting and validating mappings across models and following executable targets
remain unfinished; successful address resolution is not a mutation safety proof.
The internal bank lookup reads switch entries from caller-validated ROM tables,
rejects ambiguous matches and preserves output on error. Actual C lookup matches
all 16 Flash entries and the ERAM entry in saved HP48gII, HP49g+ and HP50g ROMs;
HP48gII's switch routine addresses differ from the other two models. This verifies
table interpretation, not physical bank availability or runtime mapping ownership.

The bounded ACPTR resolver now combines a validated object with those ROM switch
tables and reports a bank-qualified nibble offset, without dereferencing or
discarding deleted targets. The actual C implementation resolves live and
natively purged Flash references to the preserved integer 987 on HP49g+/HP50g,
and the saved HP50g ERAM pointer. All 95 host targets, focused sanitizers and ARM
analysis pass. Indexed FlashPtr tables, machine-code references and complete
live-root coverage remain required before reclamation is safe.

Bounded FlashPtr decoding and ROM-bank table lookup now preserve the two-index
format, validate the selected bank identity/count, and distinguish fixed-ROM
targets from addresses in the selected bank. The C implementation matches all
3,941 entries and end boundaries in each saved HP49g+/HP50g ROM. Logical
selectors must first be matched through the live RAM configuration tables;
they are not physical-bank indices. The C mapper matches both saved models,
including selector 1 to physical bank 2 and selector 2 to bank 3, without
hardcoding those relationships. Native selector-15 window probes on both
models now copy the 35-entry table, restore the original configuration, return
normally and leave all Flash bytes unchanged. Separate per-page probes identify
the special window's nonstandard physical mapping; ambiguous pages are not
guessed. A bounded count-prefixed-table resolver matches every captured entry
and its end boundary without weakening ordinary ROM-bank identity checks.
With those verified mappings the graph follows all 3,357 observed FlashPtr
edges across 20,642 nodes, with no unresolved FlashPtr configurations/table
entries. Machine-code, other unresolved nodes and complete live roots still
require work; no reclamation is enabled on this evidence. All 95 host targets,
sanitizers and ARM analysis pass.

The internal tagged-object visitor now exposes the bounded inline child without
treating label bytes as references. It matches the examined HP50g ROM tag at
`0x50725` and its 1,098-nibble child. Catalog traversal independently retains
all 62 examined native library bindings, including 56 banked entries that the
main-RAM relocation root scan deliberately skips. Duplicate IDs remain visible
to traversal rather than hiding a potential live reference.
Local HP49g+/HP50g probes now capture roots during ARM execution, rather than
inferring them from a post-return Stock snapshot. Each captures 434 cells,
including the saved launcher return position and 396 system-table cells, then
passes all three Flash-replacement lifecycle launches with normal return,
`exit()` and `_Exit()`. Captures are read directly from saved RAM: attempting
to also store the full sample in HOME returned `ENOSPC` in this native layout.
These checks establish read-only enumeration during invocation, not complete
bank-qualified execution-reference coverage or permission to reclaim objects.

Internal RAM-port and Flash-allocation containment now map interior references
to enclosing records only after validating the complete chain. Flash lookup
also checks the erased tail and retains deleted allocations: native ACPTRs on
HP49g+/HP50g resolve to the same 60-nibble record before and after PURGE. The
HP50g ERAM ACPTR resolves to the 52-nibble `PORTONE` backup. Tests cover every
interior position, boundaries, read failures and corruption after an apparent
match. These ownership bounds still require complete bank-qualified root and
execution-reference checks before movement or reclamation is enabled.

Root enumeration now preserves source provenance without changing the existing
flat scan or relocation semantics. Both HP49g+/HP50g active-invocation probes
report 2 launcher roots, 2 data-stack cells, 10 return positions, 4 TEMPENV cells,
14 TOUCHTAB cells, 396 system cells and 6 main-RAM library bindings. No VGER
cells are active in these captures; unit fixtures cover that source as well.
Both models pass all three Flash replacement/return lifecycles with this scan.
Source labels do not assert object types or authorize skipping any category;
execution-position interpretation and complete reference safety remain open.

A common structural-reference dispatcher now integrates sequences, tags, arrays,
CRC-validated backups and library tables. It preserves compact bodies and linked
aliases, separates native addresses from bank/index references, and explicitly
reports opaque code/binary/directory objects rather than certifying them empty.
The actual C path matches the 16 native library-275 edges and saved Flash/ERAM
ACPTRs, and expands 74 compact bodies from the existing ROM graph. All 96 host
targets, focused sanitizers and ARM analysis pass. Opaque cases, complete
directory/execution traversal and production guard integration remain unfinished
at that checkpoint.

Directory traversal now supplies bounded named children and both HOME library
attachment pointers, with library IDs preserved. The source-confirmed `7FF`
child-directory marker is recognized; other nonzero headers remain opaque.
All 96 host tests, focused sanitizers and ARM analysis pass. The native HP50g
HOME graph matches all 20 attachment pointers and exposes 21 ACPTR edges,
including ERAM. Rechecking with the current C visitor locates all 148 unsupported
targets in fixed ROM; 98 have a dispatch word equal to their address plus five.
The internal entry inspector now distinguishes those 98 self-dispatch headers
from bounded ordinary objects; known object prologues take precedence. It reports
no code extent or reference-safety conclusion, and 50 other dispatches remain
unsupported. Native checks use the current C implementation and confirm that
all 148 originate in direct runstream references rather than library metadata.
The 18 opaque code/binary objects also remain unresolved. Neither observation
proves absence of dynamic references; executable references and the production
mutation guard remain open.

Further ROM inspection identifies 11 unresolved entries sharing a dispatcher
that duplicates the stack top before executing the trailing entry. Native
HP49g+/HP50g probes verify ten of these as type predicates: each preserves an
empty input string and pushes a boolean, with only the string-type predicate
returning true. Their trailing five-nibble operands are type identifiers, not
object-reference fields. The remaining trailing DOCOL body has an exact
20-nibble span under the current C inspector. An internal descriptor-driven
decoder now exposes these embedded entries without treating their positions as
stored pointers or their type operands as references. Both native states pass
the actual C decoder; host bounds/read-failure tests, sanitizers and ARM analysis
pass. Descriptors require separately proven handler identity in a stable mapping;
automatic ROM recognition and production guard integration remain unfinished.
Execution-code edges remain explicit and reject relocation planning. Representative
unresolved bytes also match a freshly installed Stock image, including the
repeated-fill regions; those other entry semantics remain unresolved. Neither
fill bytes nor a shared dispatcher justify treating an entry as reference-free.

A native FlashTools PACK comparison on HP50g removes a deleted 32-byte record
and preserves live payloads, but leaves library 275's catalog pointing to its
old position after movement. The tool documents a Warmstart requirement for
moved attached libraries. Reusing native compaction therefore still requires
the active-reference and ARM-ownership work; it does not close the application
return or interrupted-erase gates.

Library relocation acceptance must also exercise embedded ARM code alignment.
Historical HPGCC3 has a PACK callback that realigns supported ARM envelopes in
the RAM staging copy and refreshes the library CRC before Flash storage. The
library-275 PACK fixture contains no matching legacy ARM envelope signature,
so its byte-preservation result does not test that obligation. Add an owned
embedded-code fixture with a changed alignment after movement, execute it after
relaunch, and verify its updated library CRC. Historical implementation code
is reference material only under the project's licensing policy.
The current hpclang launcher uses a Launcher V3 DOCSTR envelope; the HPGCC3
DOARMC envelope remains unsupported and is not interchangeable with it. An
owned test library containing the hpclang Stock program now passes CRC, link
resolution and complete command-span checks at three shifted bases, including
odd nibble alignment. This prepares a relocation witness, but native execution
after Flash movement and legacy-envelope alignment acceptance remain untested.
The executable witness also exposes a launcher-stack gap before relocation:
both direct-program and library-command launches reach PASS and settle back in
Stock, but lose a newly pushed integer. An invocation capture shows a HEXSTR
launcher descriptor at entry, the correct integer after the application's push,
and the descriptor left behind after native return. The bias detector now also
recognizes this descriptor when its entry and code-base words both match the
executing stage0. Local HP50g direct-program and Port 2 library-command probes
then both preserve the integer 124275 after PASS and settled Stock return,
confirmed by independent native-stack decoding. The shared stack-reservation
implementation handles both two-word and five-word descriptors and now has
repository host regression tests for descriptor identity,
bounds, failed reads/writes and preservation of application output and caller
input. Focused sanitizers and ARM launcher builds pass; the HP49g+ direct-program
probe also preserves 124275 after settled return. HP48's two-word descriptor
path passes the same independent post-return output check.

An offline HP50g relocation experiment now passes execution and output
preservation after native PACK moves libraries 275 and 124 by 32 bytes. The
qualified C planner computes the two catalog-address repairs in a stopped-state
copy; the resumed library command reaches PASS, settles in Stock and leaves
124275 on the native stack. All live Flash payloads remain identical. This
validates the sampled relocation/launch path, not live graph completeness,
atomic reference repair, embedded legacy ARM realignment or interrupted erase.

The proposed erase-recovery image codec now holds a complete 4 KiB desired
sector in a hidden native allocation outside that sector, with self/target
addresses and a status-independent CRC32. Host tests inject corruption at every
record byte, reject non-published status values and overlapping/out-of-bank
locations, and cover overlapping input buffers; sanitizers and ARM analysis pass.
This format is not yet used by public mutations or accepted by whole-port
recovery. Transaction ordering, journal retirement, allocation ownership and
startup integration remain required before any recovery claim.

The internal sector replay primitive reads and validates that durable image,
skips an already matching target, otherwise erases, checks blankness, programs
and verifies the entire sector. It retains the journal for subsequent reference
repair and retry. Host tests interrupt every read and programming operation and
every byte boundary of a partial erase, clear scratch RAM and replay from the
retained Flash record; unrelated sectors remain unchanged. Corrupt/unpublished
records cannot authorize erasure, and silent erase/program failures are detected.
Focused sanitizers and ARM analysis pass. A separate publication primitive
requires an erased reserved span and end marker, writes and verifies the whole
body before publishing status, and leaves the target sector untouched. Host
tests cut every read/program step, reject occupation at every byte and check
publication followed by replay. It does not resume partial publication or select
safe journal storage. Public reclamation, journal discovery and retirement,
complete reference repair and startup recovery remain open.

Read-only discovery can now locate a committed sector journal in an intact
native bank, checking bank identity, allocation boundaries, live-object CRCs,
journal integrity, uniqueness and the complete erased tail before returning
its location. A valid-looking journal embedded in a deleted payload is ignored;
corrupt tails, objects and conflicting journals fail without writes. The target
bank can already be damaged because this scan reads only the journal bank.
Whole-port read-only preflight now requires one discoverable journal, overlays
its image in RAM and validates all six restored bank identities, allocation
chains, live-object CRCs and erased tails. Tests reject cross-bank journal
conflicts, structurally invalid images with valid capsule CRCs, and damage
outside the target sector; an accepted image passes replay and revalidation.
Focused sanitizers and ARM analysis pass. This does not traverse a broken
journal-bank chain or prove transaction freshness, object-name/ID uniqueness,
reference correctness or erase authority. Recovery planning must retain an
independently discoverable journal and establish those remaining invariants.

Caller-owned journal retirement now verifies the complete retained record and
target sector before clearing only the status byte. Tests cover every partial
retirement status, each interrupted read, failed/silent programming and corruption
at every journal-body or target byte. An integrated host fixture discovers,
preflights, replays, retires and revalidates a structurally clean port. Sanitizers
and ARM analysis pass. Retirement requires the caller to finish reference repair
first. Discovery/preflight now distinguish ready and partially retired journals
from their durable status. A retiring journal requires the actual target to
match the image; replay rejects it without erasure. Tests clear scratch RAM,
rediscover partial retirement, reject changed targets and finish retirement
using only the durable record. The codec covers all 256 status values. Automatic
startup invocation, reconstruction of reference obligations and the public
reclamation transaction remain unimplemented.

Caller-owned RAM reference batches now support checked, repeatable writes through
an atomic nibble callback. Full metadata and expected/replacement-value checks
precede mutation; each changed nibble is read back. Tests interrupt every read
and write, including writes that complete but report failure, and verify retry,
unchanged neighbors, distinct mappings, conflict refusal and silent-write
detection. Focused sanitizers and ARM analysis pass. The caller must retain the
authorized plan and exclusive ownership; persistence of that plan, complete
root discovery and coordination with Flash transactions remain open.
In the stopped native HP50g PACK-state copy, the actual C writer now applies
both planned library-124/275 catalog repairs, changing only two RAM bytes.
Failures before or after either write retry to the same result, and repeating
the completed batch performs no writes. Resuming that copy executes the moved
library command, reaches PASS, settles back in Stock and retains the independently
decoded integer 124275. This replaces the earlier Python byte-patching step;
it does not establish live ownership, complete roots or restart-safe RAM addresses.
Catalog repair can now resolve the current field from library ID, bank-switch
identity and the authorized previous/replacement binding, rather than retaining
an old RAM field address. Host tests move and reorder the catalog, accept an
already repaired binding and reject duplicate IDs, changed banks, unexpected
targets and failures anywhere in the complete catalog. The actual C resolver
also finds both repair fields in the saved native HP50g catalog before checked
replay. Sanitizers and ARM analysis pass. The transaction still must locate the
current catalog, authenticate the relocation and reconstruct all other roots;
this locator alone does not make arbitrary persisted RAM edits restart-safe.
Locator checks also accept the atomic writer's interrupted old/new nibble
combinations. Tests cover all 32 combinations for a five-nibble binding and
reject unrelated values. An integrated test interrupts each write, rebuilds
the partial catalog elsewhere, discards the old RAM edit and resolves a fresh
field before finishing; old catalog bytes and neighbors stay unchanged.

An isolated native HP50g restart experiment now starts from the PACK state with
both stale bindings still unpatched. The ON/F3 sequence rebuilds library 124/275
bindings at their moved addresses without changing any Flash byte. The resumed
library command reaches PASS, returns to settled Stock and retains integer
124275. Recovery must distinguish an interrupted live invocation from a catalog
already rebuilt by Stock and resolve current bindings rather than replay saved
RAM addresses. This evidence covers intact compacted Flash; reaching a recovery
entry with a partially erased bank remains a separate startup requirement.
An isolated follow-up erases the first 4 KiB of the user bank containing
libraries 124 and 275 before the same native restart. Stock reaches its HOME
screen, but neither library appears in the rebuilt catalog; all later library
payload bytes remain intact and the bank identity remains erased. The existing
Library-275-dependent application path therefore cannot be assumed available
for recovery. Before public reclamation, establish a recovery entry independent
of the bank being erased and retain its journal in independently discoverable
storage. Recovery only when an application opens Port 2 is insufficient for
this observed failure. No firmware patch or automatic startup hook is implied.
The actual C publisher now places a recovery image in a different native HP50g
bank before that header-sector failure. Its full record survives ON/F3 unchanged;
C discovery and whole-port overlay preflight still accept it. Executing the C
replay against a stopped-state copy restores only the target sector, retaining
the journal and all other Flash bytes. A subsequent native restart rebuilds both
library bindings at the correct addresses without modifying Flash or the active
journal. This verifies native coexistence and restored-data acceptance, not
on-calculator execution of the recovery entry or automatic power-loss recovery.
An independently assembled minimal entry probe now runs on the damaged-bank
HP50g state with library 275 absent: native ARMSYS executes owned ARM code that
writes a marker, returns to Saturn and continues to push integer 124275. Flash
is unchanged. The private experiment aligns code and patches its entry address
inside the deposited object's owned RAM while the emulator is stopped. It proves
the entry/return boundary only; on-calculator placement/relocation, memory
ownership, HPC1 integration and an executable recovery application remain open.
The next native HP50g prototype removes that stopped-state patch: independently
assembled Saturn code obtains its own PC, copies the ARM marker probe to an
aligned destination within its CODE object's reserved payload, computes the
main-RAM ARM address and enters ARMSYS. Direct transfer/execution with library
275 absent writes the expected marker, returns and pushes integer 124275; Flash
is unchanged. Independent decoding confirms the aligned executed copy remains
inside the owned object. This is a small private entry probe, not generalized
allocation, cache-coherency/layout acceptance, HPC1 loading or a recovery payload.
An eight-probe native program now exercises every CODE nibble-alignment phase
in one invocation. All eight self-aligned ARM copies remain inside their own
reserved CODE payloads, write distinct markers and return eight independently
decoded integers. Library 275 remains absent and Flash unchanged. General
payload sizes, cache reuse, a dedicated stack/workspace and recovery execution
still need acceptance beyond this fixed-size marker probe.

A subsequent calculator-side counted-copy prototype accepts build-time bounded
payload lengths and replaces the fixed three-word copy. Native HP50g execution
passes eight payload sizes from 24 through 4,096 bytes across all eight CODE
nibble-alignment phases. Independent checks compare every copied byte, verify
each destination lies within its own reserved object space, and decode all
eight post-return integers; library 275 remains absent and Flash unchanged.
This establishes variable-size placement for these probes, not runtime input
validation or an accepted recovery launcher. Dedicated stack/workspace, cache
reuse, actual recovery execution and cross-model acceptance remain open.

Qualified reference planning now separates source and target mappings before
adjusting absolute or relative fields. Host tests cover bank aliasing and torn
fields; sanitizers and ARM analysis pass. Against the native PACK capture, the
actual C bank resolver/planner computes the relocated library-275 catalog target
while leaving an equal address in another bank unchanged. Supplied edit batches
now receive read-only bounds/overlap and expected-value preflight, including
bank-switch metadata guards. Host failure injection, sanitizers, ARM analysis
and the native catalog-state check pass. A read-only integration experiment
enumerates all 62 entries in the HP50g PACK capture, resolves nine mapping
identities and validates 124 target/switch fields as one batch. Only library
275 requires an address change; changing the final record's bank metadata
rejects the batch. Read-only short-field guards now check the catalog count,
IDs and flags: all 125 native guards pass, and each of 375 injected nibble
changes is rejected. Host failure tests, sanitizers and ARM analysis pass.
These checks require exclusive ownership through commit; they do not provide
synchronization. Full root discovery, assembly of a complete qualified batch and live
commit remain required before reclamation is enabled.

The Port development branch now integrates the standalone historical-system client
and system adapters. Both integrated HP40gs Stock clients pass normal return and
independent pixel checks, replacing the old combined fixture that failed with
`0x464100c0`. All 87 host tests, focused sanitizers/static analysis, formatting and
strict documentation checks pass. Public Port 1 replacement/removal remains open:
bank-qualified traversal reaches ROM machine-code entries and user-library code
whose execution references still require classification.

- [ ] Recheck each model's native port representation and supported contexts
  against historical implementations and Stock behavior; do not extrapolate a
  single model's mapping to all six calculators.
- [ ] Define typed port discovery, object enumeration/read/store/remove and
  owned snapshot lifetimes for native backup/library objects. Keep the
  [retained per-port requirements](#native-port-services-retained-follow-up).
- [ ] Implement and accept Port 0 RAM ownership and active-library safety,
  then Port 1 banked-memory/ARM-allocator coexistence, then Port 2 Flash
  erase/write, full-media failure and persistence. Each port has its own gate.
- [ ] Verify original-system visibility after return/relaunch, mutation failure
  without unrelated data loss, and coexistence with HOME objects and normal
  application exit. Flash failure/recovery claims require explicit evidence.

#### M8.1 — Installable SDK foundation

The first source-SDK slice provides installation, relocation, an external C/C++
build API, generated font selection and HPC1 packaging. Local relocation/build,
format/lint, ARM example analysis and strict docs pass. Core CI now exercises
the installed package. The first slice is accepted by Core, Quality, Runtime,
Objects and Storage CI on `9faa4882` and integrated into main. The complete
packaging slice adds six-model raw/wrapped launchers and HP39 Aplet envelopes
through builders shared with E2E. Relocated external C/C++ builds, missing-output
regeneration and all three HP39 Aplet packages pass with declared historical
dependencies and a Clang-built host packager. Core, Quality, Runtime, Objects,
Storage and Gallery CI accept `5eea74c9`; M8.1 is complete. See
[SDK getting started](../sdk/getting-started.md). This accepts the source SDK
and its build/package contract; UI and integrated release acceptance remain
in their subsequent stages.

- [x] Package the public headers, libraries, fonts/assets, target configuration
  and launcher/packaging tools with an explicit supported toolchain contract.
- [x] Build and package external C and C++ examples using only the installed
  SDK and declared dependencies in a fresh LLVM-only environment.
- [x] Start application-facing installation, build and capability documentation
  separately from the internal Development reference. Keep examples usable
  without depending on repository research or test infrastructure.

#### M8.2 — Declarative UI core

The implementation provides bounded transactional declarations, scoped stable
identity, row/column layout, controlled text and boolean proposals, native-key
input, nested focus-revealing scroll, revision-driven canvas damage and bounded
asynchronous effects with owner cancellation and exit cleanup. The surface/font
renderer has explicit themes, clipping leases and transactional raster pages.
The installed device-manager example combines the original application patterns,
including failure/retry/cancel, eight-device insertion/reversal and waveform updates.
Public names remain provisional pending milestone acceptance.

Local acceptance now passes all six Direct and all six Stock targets, including
physical-key events, alternate-theme replacement, native-font or explicit fallback
layout, and normal system return. Independent LCD checks verify application states,
32 model-derived waveform vertices, theme inversion and font-dependent geometry.
All 80 host targets, UI sanitizers, host/ARM Clang analysis and strict docs pass;
an installed SDK builds the alternate-palette/native-font example and six launchers.
The full view has 46 nodes; its 48-node/512-byte/one-task ARM pool uses 13,692 bytes
after the accepted compact-storage changes (previously 16,764 bytes).
HP39g+ and HP39gs complete the flow within the observed 36 KiB read/write window.
Host flow/pixel tests cover both palettes at 2/4/16 levels; emulator timings are
not physical-device performance guarantees. The shared Runtime matrix includes
the UI suite. CI acceptance of the implementation is complete at `a18302a3`:
[Core](https://github.com/maxmixalex/hpclang/actions/runs/38410381090),
[Quality](https://github.com/maxmixalex/hpclang/actions/runs/38410381128),
[Objects](https://github.com/maxmixalex/hpclang/actions/runs/38410381038),
[Runtime](https://github.com/maxmixalex/hpclang/actions/runs/38410381089) and
[Storage](https://github.com/maxmixalex/hpclang/actions/runs/38419383490) pass.
[Gallery](https://github.com/maxmixalex/hpclang/actions/runs/38406483751) passed
on the preceding implementation; the text-node enum/scanner correction did not
change Gallery inputs. The remaining milestone checklist below is retained;
integration alone does not close M8.2/M8.3.

The retained design is [View = f(Model)](sdk-design.md#declarative-ui-core-planned):
applications describe the current view, events update state, and stable keys
preserve node identity when that view is declared again. C functions suffice
for composition; later C++ syntax builds on the same core.

Deliver and review this core in the following slices, using the retained
[declarative application contract](sdk-design.md#declarative-application-contract):

- [x] **M8.2a — C declaration and identity:** nested scopes, ordinary conditional
  and loop syntax, function components and complete parent/key paths. Exercise
  two component instances and stable-ID list insertion/reversal, including
  duplicate keys, scope interruption and fixed-pool exhaustion.
- [x] **M8.2b — Layout, input and application updates:** row/column measurement,
  focus and navigation, controlled text/filter events and model-driven views.
  Define what happens when filtering removes the focused item and when the
  application accepts, transforms or rejects an edit.
- [x] **M8.2c — Effects, canvas and incremental frames:** explicit asynchronous
  states, loading/error/retry, cancellation, revision-driven custom painting
  and damaged-region updates. Accept the original sketch's six application
  patterns as one stateful example, with a deterministic asynchronous provider
  and real calculator keyboard/display evidence.

- [ ] Review a small public application sketch before freezing API names:
  nested rows/columns, conditional children, a keyed changing list, controlled
  text input, a custom canvas and an asynchronous operation with cancellation.
- [ ] Define bounded node storage, deterministic sizing/measurement/overflow,
  stable-key reconciliation and predictable allocation-failure behavior.
  Measure actual memory and update cost on the smallest supported target.
- [x] Implement focus retention/traversal, keyboard navigation and event/action
  routing above the unchanged physical-keyboard API. Text entry uses the
  model-aware input service rather than duplicating Alpha/Shift rules.
- [x] Define controlled-input edits and selection, explicit effect start/cancel
  ownership, stale-result rejection after removal, and cleanup at application
  exit. Integrate with the cooperative application loop.
- [x] Track damaged regions and bounded canvas revisions; exercise identity,
  layout, focus and effect lifecycle in host tests, then validate an interactive
  stateful example on calculator targets.

#### M8.3 — Components, rendering and themes

- [ ] Provide reusable text, action, selection, scrolling/list and editing
  components with composable containers; confirm the initial component set
  through real example screens rather than application-specific dialogs.
- [ ] Implement a usable default surface/font renderer and explicit theme
  inputs. Keep widget appearance, font choices and rasterization outside the
  state/layout engine. Measurement and drawing must agree across backends.
- [ ] Demonstrate the same application tree with a second renderer/theme,
  including focus and input behavior. Preserve the downstream 39gsX native
  appearance use case; a claim of faithful HP39 reproduction requires separate
  reverse-engineering and visual evidence.
- [ ] Adapt the original sketch's illustrative native drawing/pointer boundary
  to actual monochrome/grayscale surfaces and keyboard-only operation. Keep
  pointer support optional and test clipped drawing ownership/publication;
  RGB565 and device scanning in the sketch do not imply new target hardware.
- [ ] Validate six-model geometry, native/fallback fonts, clipping, scrolling,
  partial redraw and custom canvas, including low-memory failure and clean
  Stock return. Record frame/update and memory budgets.

#### M8.4 — Historical source compatibility

The system adapter now includes five `hpsys.h` entry points for millisecond delays,
750 kHz tick delays, nanosecond stopwatches, civil seconds-of-day and
midnight-wrapped differences.
All 87 host tests after integration with the accepted C++ ownership work, six
Direct targets and HP39g+/HP48gII/HP49g+/HP50g Stock pass, including independent
pixel checks for the combined payload. HP39gs then exposed insufficient heap
for its clock/display check. The timed-sieve client now has a separate
`hpgcc-system` payload and pixel verifier in the shared Runtime suite manifest;
both split payloads pass all six Direct and all six Stock targets, including
their independent pixel checks. All 87 host tests, formatting, static analysis,
focused sanitizers and strict documentation checks pass. All six workflows
accepted this system-adapter batch at `7a30541c`, merged into main at `5e4a12c2`.
The relocated SDK build passes C/C++, all six launchers and all three HP39
Aplet package families with the system adapter included. The installed
`examples/hpgcc-timer` migration client passes that same relocation/packaging
gate; it demonstrates the five historical calls and documents their units.
The follow-up calendar adapter adds `sys_tm_RTC`, the `hptime.h` include spelling
and date output in the installed timer example. Calendar conversion/failure
tests, all six Direct and all six Stock targets (including independent pixels
and normal return), and the updated relocated SDK acceptance pass. Formal CI
for this calendar addition is pending. The original void signature is retained with errno and unchanged
output on failure; weekday/year-day are computed rather than copied from the
historically unreliable RTC weekday field.
The remaining historical system, timer and console interfaces remain open.

Local HPG implementation now covers all 59 public functions. ARM declaration
checks match all four historical headers (55 functions in 1.0; 59 thereafter).
The integrated Display suite passes all six models in Direct and Stock,
including independent single/double-buffer, font, XPM and indicator checks.
Five original HPGCC 1.1 graphics examples compile and their drawing bodies
pass HP50g Direct execution with test-controlled screenshot waits. CI acceptance
of the HPG implementation is complete at `1e46aa3d`: Core, Quality, Runtime,
Objects and Storage pass. Integration with the accepted UI implementation
passes all 81 host targets and the ARM Display build. The other adapter groups
below are retained.

- [ ] Deliver the common 59-function HPG graphics surface and maintained
  keyboard, console/text, system, filesystem and object adapters in focused
  groups. Expand the four-generation declaration/implementation/evidence ledger.
- [ ] Compile representative original historical applications, recording each
  required source edit; distinguish these from M7's clean-API behavioral ports.
- [ ] Evaluate useful historical `win*` adapters on the accepted UI core where
  semantics fit. Keep raw KOS/Saturn/MMU migration guidance explicit, and
  preserve unsupported-context errors rather than supplying silent stubs.

#### M8.5 — Modern C++ and migration

Local implementation adds allocation-free C++ declaration/scope guards and
movable ownership for offscreen surfaces. A shipped C++ counter uses the same
C UI core; its actual view is also exercised by the UI E2E before the complete
device-manager flow. All six Direct and all six Stock targets and independent
UI pixel checks pass, as do 84 host targets, wrapper sanitizers and ARM compilation. The SDK
relocation gate includes the counter and has passed all six launchers and
three HP39 Aplet packages. Compact node storage reduces the full ARM declaration
pool by 3,072 bytes without reducing the eight-device scenario. All six CI
workflows pass at `e6f04dbd`, now merged into main. Broader owned wrappers and
the remaining migration/release requirements below stay open.

The next local ownership slice adds C++17 `owned_object`, with move-only
snapshot ownership and failure-preserving C producer calls. Host and sanitizer
checks cover transfer, replacement, failure cleanup and destruction. HP39g+ and
HP50g Direct Runtime tests pass, as does the installed SDK relocation gate with
the new object example, all six launchers and three HP39 Aplet packages. This
slice was subsequently accepted and merged at `70406cac`; broader migration
and release goals remain.

Directory handles and independent metadata iterators now have allocation-free
C++17 `unique_ptr` owners in `object-store.hpp`. They preserve the C service's
raw-byte names and context errors, close on normal scope exit, and preserve
`errno` during cleanup. The real object-store host fixture covers parent/child
independence, iterator use after the directory closes, move replacement,
failed-open preservation and release/adoption. The installed object example
uses the optional native HOME snapshot; Object Basics now exercises these
owners in its existing context-aware E2E flow. This ownership addition does not
claim that `exit()` unwinds automatic C++ objects.
All 95 host targets, focused ASan/UBSan and ARM C++ analysis pass. Object Basics
passes HP39g+ Direct and HP48gII/HP50g Stock locally, including normal return.
The relocated SDK gate passes C/C++, all six model builds/launchers and the
three HP39 Aplet package variants. CI acceptance of these new owners is pending.

- [ ] Extend owned, lifetime-safe C++ wrappers and composable UI helpers within
  the supported no-exceptions/no-RTTI embedded profile; keep C applications
  first-class and exercise destruction/exit behavior.
- [ ] Provide migration guidance and useful tooling for historical APIs,
  precision/ownership differences, target capabilities, UTF-8/font selection
  and retired raw hardware access.
- [ ] Publish C/C++ application examples, stable public API conventions and
  deterministic resource generation as part of the installable SDK.

#### M8.6 — Application and release acceptance

The initial `examples/notebook` application connects the C UI and civil clock
to native string persistence: HP39 Aplet-owned data or classic HOME. Saves are
explicit and reject foreign destination data. Host tests cover both paths,
bounded records, failure preservation and owned-resource cleanup. ARM syntax
and static analysis pass. The relocated installed SDK builds the application,
all six launchers and all three HP39 Aplet packages. Local multi-session
ROM-emulator acceptance now passes all six models. The application offers Port backup/restore
according to queried read/create/replace capabilities, with validated records
and no delete/recreate fallback. Exclusive file export handles short writes,
interruption and write/close errors without overwriting an existing file.
Host tests cover all three port indices and export failures; the focused Stock
fixtures below cover Port 0 and raw-media export acceptance.

The integrated application exposed a Stock startup RAM limit on HP39g+ and
HP48gII. Launcher compression and smaller C/C++ code generation are being
validated against the complete application; host decoder bounds and relocated
SDK packaging pass, but small-model execution acceptance is still pending.
The compact bootstrap now passes six-model Direct runtime/divmod/keyboard
acceptance. HP48gII, HP49g+ and HP50g also pass the complete notebook Stock
save/relaunch/replace/relaunch/cleanup flow and independent LCD checks locally.
Remaining Stock targets and formal CI remain open.

The approved model-specific SDK build keeps the application source and APIs,
uses a fixed model with whole-program optimization, and rejects another model
before application startup. All six model-specific notebook builds and the
default universal relocated SDK build pass locally. The HP39g+ application is
44,424 code bytes with 1,048 static writable bytes. Its separate integration
fixture passes all three HP39g+ Stock launches and independent pixels after
correcting the compact bootstrap's portal stack budget. Host coverage is now
91 passing targets, including startup
rejection before initialization. The independent `notebook-backup` fixture keeps
Port 0 lifecycle checks out of the UI image: HP48gII passes its three-launch
backup/restore/replace/remove flow and the separate UI/pixel lifecycle locally;
the independent backup fixture also passes on HP50g.
These are original-ROM emulator results, not physical-device acceptance.

The model-specific notebook also passes HP39gs and HP40gs Stock persistence and
pixel checks. The separate Port 0 fixture now passes all three classic models.
The file-export fixture passes HP49g+ and HP50g with a raw FAT16 image shared across two
launches: native HOME data and UTF-8 output persist, exclusive-create refuses
replacement, and independent mtools reads verify the output and retained card
content. Model-specific UI execution and independent pixels pass all six models.
The Runtime registry covers these separate Stock-only fixtures with per-model
builds where needed. Local verification also passes all 91 host targets, SDK
relocation and six-model builds, schema/negative-configuration checks, static
analysis, shellcheck, actionlint, formatting and strict documentation. Formal CI
acceptance for this integrated batch remains pending.

The public ARM bootstrap and synchronous KOS wrappers now return through `BX`
after restoring LR, as required when ARMv4T callers use Thumb. The relocated SDK
gate inspects their emitted instructions; restoring the old LDM-to-PC sequence
is rejected. Full SDK C/C++ builds, all six model-specific notebook builds and
three HP39 Aplet variants pass locally. Object Basics also passes HP39g+ and
HP50g Stock startup and normal return with the corrected wrappers. These checks
do not replace physical-device validation or close the remaining release goals.

- [ ] Exercise non-trivial downstream applications combining UI, object data,
  time/system services and, where supported, files and native ports. Cover each
  hardware family and state exactly which scenarios were emulated or physical.
- [ ] Validate release installation, reproducible assets, resource limits,
  error handling and Stock restoration; finish public documentation separately
  from internal design/history.
- [ ] Evaluate the retained independent-launcher option against concrete
  packaging/portability benefits. Any scope decision remains explicit; M8
  does not silently turn this evaluation into an implemented launcher claim.
  This is retained follow-up work, not the current Port 2 priority or a
  prerequisite for continuing native port services.
  - [ ] Prototype Stock-to-ARM entry and normal return without invoking
    ARMToolbox / library 275, then connect the existing HPC1 loader. A minimal
    entry probe alone does not establish an independent application launcher.
  - [ ] Evaluate owned page allocation, MMU setup, relocation roots, native
    service coexistence and exit recovery across all three platform families,
    including the HP39 Aplet boundary and smallest-model memory budget.
  - [ ] Present a migration decision and acceptance plan before retiring the
    historical dependency. Any replacement must preserve six-model packaging,
    application ABI, Stock return and documented compatibility; retirement is
    not implied by successful prototype tests.

Suggested work order: M8.0 and the M8.1 packaging foundation first, followed by
M8.2/M8.3 as the early UI track. UI design and host fixtures need not wait for
Port 2 Flash work. Adapter groups may progress once their clean services are
stable; `win*` follows the UI contract. M8.5 and M8.6 integrate the accepted
pieces. Stage numbers are acceptance boundaries, not a requirement to postpone
all UI work until every earlier optional investigation is exhausted.

For each stage, implement and validate locally before batching a meaningful
CI submission. Keep host semantic tests separate from calculator integration
and pixel witnesses, select affected E2E suites, and preserve normal PASS and
return gates. A pushed CI run is not a reason to stop independent work or poll.

## Additional M7–M8 acceptance requirements

The following requirements supplement the existing milestones and preserve the
accepted M7.3a–f evidence. Historical source evidence is recorded in
[HPGCC compatibility](hpgcc-compat.md).

### M7.0 follow-up — capability and evidence ledger

- [x] Track historical declaration, implementation, clean equivalent, source
  adapter and validation separately, per model and execution context.
- [x] Give each historical application capability an explicit destination;
  require maintainer approval for an actual omission rather than treating a
  raw historical signature as sufficient reason to omit its useful behavior.
  - The [capability ledger](hpgcc-compat.md#capability-and-application-evidence)
    records the four source generations, model/context boundaries and actual
    application witnesses. CPU policy and ordinary termination stay in M7;
    native ports, reset and serial-number research retain individually approved
    follow-ups. Remaining historical spellings and broader UI policy stay in M8.
    The ledger and functional CI gate are accepted on `2988e1de`.
- [x] Define runtime discovery for supported operations and context-dependent
  availability before new object/system services proliferate.
  - Accepted on `9489e7d1`: distinguish classic RPL read access from mutation availability,
    share launcher-bound validation with the mutation backend, and validate the
    HP39 portal before reporting Aplet services. Host and calculator probes cover
    model/context differences; the broader historical capability ledger is accepted on `2988e1de`.

### M7.1 / M7.2 follow-up — uncovered application behavior

- [x] Establish HP39g+ native-font correctness using its own ROM/pixel evidence.
  - D1.55 main/mini/EditLine tables are located and implemented through the
    private ROM window. Local Direct and Stock USB pixel/return checks pass on
    all three HP39 models; formal CI acceptance is complete on `8f79c156`.
    HP39 USB transfer reliability remains a separate follow-up. The physical keyboard
    contract is unchanged.
- [x] Specify text/Alpha-plane, bounded text/numeric input and console behavior
  above the accepted physical keyboard core.
  - The bounded UTF-8 editor in `hpclang/input.h` already supports insertion,
    deletion, navigation, accept/cancel and basic physical-key editing.
  - Numeric extension accepted on `b92e2a7d`: exact signed 64-bit and finite
    double conversion from the bounded editor, full-line syntax/range checking,
    and correction after a rejected accept attempt. Subsequent text/console
    acceptance is recorded below; historical spellings remain M8 adapter work.
  - Console extension accepted on `b92e2a7d`: owned fixed-cell UTF-8 output,
    wrapping/scrolling, clipped viewport rendering and explicit standard-stream
    binding with read/write/flush callbacks, distinct EOF/error/cancellation and
    reset on ownership changes. Subsequent Alpha/Shift and keyboard line-input
    acceptance is recorded below; physical keyboard semantics are unchanged.
  - Line-session extension accepted on `c7451364`: callback-driven
    event/edit/accept/update loop, recoverable capacity/validation errors and
    explicit cancellation. The scripted session test supplies callback coverage;
    physical-key integration has separate acceptance below.
  - Explicit Latin Alpha lookup accepted on `c7451364`: all six models'
    printed A-Z/a-z and Alpha-space positions, checked against their manual
    diagrams and tested through physical key identities. Punctuation/symbol
    lookup was subsequently accepted on `41d75dec`; no implicit modifier state
    is added to the keyboard core.
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
    Full symbol lookup and a ready-made keyboard editor were subsequently
    accepted on `41d75dec`.
  - Physical text integration accepted on `c7451364`, integrated into main:
    acknowledged GPIO transitions pass through the SDK scanner/debounce queue,
    native text mode, bounded editor and console. Single/held Alpha and a
    separate model-native lock sequence have independent pixel references;
    every injected key must be released before PASS. This extends the earlier
    scripted coverage; full symbol lookup and a ready-made editor were
    subsequently accepted on `41d75dec`.
    Runtime, Core, Gallery, Storage, Quality and Objects all pass on that head.
    The merged text-input, text-mode and physical-input branches are retired.
- [x] Cover historical file attribute mutation and creation/access timestamp
  queries, including persistence after close/reopen and unsupported cases.
  Implemented in `m72-file-metadata`: clean masked attribute updates, stored FAT
  creation/access-date queries and `FSChAttr`/`FSGetCreatTime`/`FSGetAccessDate`
  adapters. Local persistence/error tests, six-model Direct and both SD-model
  Stock Storage pass; integrated CI acceptance is complete on `6a7f9a28`. This does not add
  automatic access-date maintenance or timestamp setters.
- [x] Audit media-change and ownership behavior; retain the unresolved Stock
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
    Quality CI pass. Accepted on `cada0f2e`: the EINT3 removal latch rejects
    stale buffered writes even after reinsertion between observations, and
    restores leased card pin/interrupt configuration before Stock rescan.
    The 68 host tests and raw-FAT-image replacement tests pass locally in Direct
    and Stock on HP49g+/HP50g; Runtime, Storage, Objects, Gallery, Core and Quality
    CI all pass, and the changes are integrated into main. On 2026-10-11 the maintainer
    confirmed an HP49g+ and SD card are available, requested a reproducible
    hardware test package, and designated the later physical run as a
    nonblocking follow-up. The [manual test package](sd-media-check.md) is built
    from source by Storage CI, with the launcher installer and instructions;
    emulator results must not be reported as physical hot-plug evidence.

- [x] Provide clean access to multiple FAT partitions on one SD card. The
  HPGCC3 `fsystem/common/fsinit.c` implementation actually mounts up to four
  primary volumes; HPGCC 2.0's `fsystem/examples/example1.c` exercises that flow.
  The existing classification of historical global volume state does not close
  this data-access capability. Preserve unqualified paths for the default volume,
  add explicit volume discovery/paths, isolate open objects across volumes,
  reject cross-volume rename with `EXDEV`, and invalidate every volume on card
  replacement. This concerns SD FAT partitions, not the separately approved
  Port 0/1/2 calculator-object limitations.
  - Implemented on `m7-storage-volumes`: dense clean indexes, primary-slot
    source aliases, independent adapter working directories, overlap/extent
    validation and all-volume invalidation. All 68 host tests, six-model
    Direct Storage and HP49g+/HP50g Stock Storage pass locally, including
    dirty streams on both partitions during replacement. Static analysis,
    formatting, lint and strict documentation build pass. Formal CI passes on
    `7303a3d6`, now integrated into main; the topic branch is retired.

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
- [x] Validate genuine historical application flows using the four HPGCC
  generations and HP39 Aplet template, not only scalar API probes.
  - Accepted on `6a7f9a28`: four-generation object/stack clients and HP39
    multi-invocation Aplet persistence/Reset/composite lifecycles. The broader
    time/power/termination batch has its separate acceptance gate below.

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

Historical-client and card-identity batch, accepted on main `7303a3d6`:
`a6be226c` adds the source-anchored HPG animation/mixed-font, physical keyboard
and file application flows, explicit native-font availability and correct
historical file-volume metadata. Display and System Services pass on all six
models in Direct and Stock; Storage passes on both SD models in both contexts.
`0d755f11` adds stable CID queries, with all 68 host tests, six-model Direct
Storage and HP49g+/HP50g Stock replacement tests passing. Changed-source Clang
analysis, formatting/lint and strict docs pass. Runtime, Storage, Objects,
Font Gallery, Core and Quality CI all pass on `7303a3d6`.
The detailed per-capability evidence is in
[HPGCC compatibility](hpgcc-compat.md#capability-and-application-evidence).

Final source-confirmed application capabilities and retained follow-ups:

- [x] Complete ordinary application termination: returning from `main` and
  `exit()` must execute registered cleanup and return to the calculator;
  `_Exit()` must skip language callbacks while restoring SDK-owned hardware.
  - Implemented with a shared launcher return boundary and the picolibc
    finalizer section retained by the linker. Local host tests cover nested
    calls, constructor termination and exact status propagation; ARM tests
    check actual C callbacks, C++ destruction and owned hardware restoration.
    Six-model Direct/Stock termination checks pass. The additional buffered
    stream cleanup passes all 72 host tests, sanitizers, six-model Direct
    Runtime/System Services, HP39gs Stock System Services and HP49g+/HP50g
    Direct/Stock exit-time file persistence. Formal CI is accepted on `2988e1de`.
- [ ] Retain whole-calculator reset (`kos_Reset`) as a subsequent research
  goal. The historical entry is reset/panic recovery, not ordinary app exit.
  The maintainer approved this as nonblocking for M7 on 2026-10-12.
- [ ] Retain reliable hardware serial-number discovery as a subsequent
  research goal. All four HPGCC header versions warn that
  `kos_getSerialNumber` returns questionable data; a declaration alone is not
  evidence of usable identification. The maintainer approved this as
  nonblocking for M7 on 2026-10-12. SD CID remains a separate implemented API.
- [x] Expose stable SD card identity so an application can distinguish the same
  card reinserted from a replacement. HPGCC3 `FSVolumePresent` actually compares
  CID; removal detection alone does not provide this information. Keep stale
  handles invalid even if the same card returns.
  - Implemented through `hpclang_stream_media_identity_query`: the SD driver
    retains the card's CID and returns it through an owned versioned snapshot.
    All 68 host tests, six-model Direct Storage and HP49g+/HP50g Stock Storage
    pass locally, including A→B→A CID comparisons and stale-handle rejection.
    The emulator's serial follows its backing inode, not mutable file contents.
    Formal CI is accepted on `7303a3d6`; historical `FS_CHANGED` spelling is
    adapter work.
- [x] Review and provide safe CPU performance/power policy for the ordinary
  application behavior behind `sys_slowOn/sys_slowOff`. Historical benchmarks,
  games and interactive examples use this to change speed or reduce power;
  replacing busy delays with monotonic waits does not establish power parity.
  Coordinate clock consumers and restore host state rather than exposing raw
  clock-register writes.
  - HOST/ECONOMY profiles are implemented with an unchanged PLL and bus
    divisors, bounded LCD reclocking, monotonic rebasing and active-sound
    exclusion. All 72 host tests and six-model Direct/Stock System Services pass
    locally, including exit cleanup checked before the harness restores Stock.
    HP49g+/HP50g Direct and Stock SD file coexistence and six-model Stock
    object coexistence pass. Formal CI is accepted on `2988e1de`.
- [x] Add a genuine source-anchored historical time/system client alongside
  the accepted focused RTC, timer and sound tests.
  - `hpgcc-system/historical-system.c` ports the HPGCC 1.1/2.0 sieve
    example with bounded allocation/work, independently checked prime results
    and a visible elapsed-time report. Direct and Stock pass on all six models;
    formal CI is accepted on `2988e1de` for this additional client.

Accepted on `41d75dec`: model-native Unicode symbol lookup,
the default bounded keyboard editor, contrast mutation/restoration and owned
LCD enable/disable. All 67 host tests, six-model Direct System Services and
Display, and Stock System Services/Display on HP39g+, HP48gII, HP49g+ and HP50g
pass locally. Contrast and the corrected HP39 transport additionally pass
Stock on HP39gs and HP40gs. Runtime, Storage, Objects, Gallery, Core and Quality
CI all pass. This work and the media-event extension are integrated into main
at `cada0f2e`; their topic branches are retired. These additions do not close
the remaining capability ledger or historical-client goals.
The HP39 transport regression now covers embedded CRLF bytes: all three Stock
models pass the corrected newline transport locally. This is a byte-preservation
fix, not a retry or relaxation of the application result checks.

- [x] Exercise a stateful HP39 Aplet, nested directories, numerical composites,
  files, text/keyboard, graphics and time/system clients on their actual models.
- [x] Retain the existing M8 compatibility, C++, migration and declarative-UI
  goals; add installable SDK/examples, console/text adapters and representative
  HPGCC3 clients to the compatibility ledger.
- [x] Keep missing clean capabilities in M7; reserve M8 for historical spelling,
  higher-level policy, SDK usability and the already planned extensions.
