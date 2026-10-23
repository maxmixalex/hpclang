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

**In progress. M8.1 is accepted; the other stages retain the completion conditions below.**

### Current M8 acceptance and execution order

This summary describes current work. Older implementation snapshots below retain
their original evidence scope; their then-pending statements are not the current
backlog. Local verification, CI acceptance and milestone completion are distinct.

Execution discipline: read this roadmap and remote issue #1 before selecting work;
reconcile implementation, branch and verification evidence, then update both when
results or scope facts change. M8.0 remains the primary track. Port 2 recovery
ownership, startup ordering and reclamation are required deliverables, not a
post-M8 deferral. Only M8.1 is accepted complete.

Submitted `02e81749` passes Quality (39903692796), Core runtime (39903692792,
including all fourteen Stock port jobs), Objects (39903692845), Runtime
(39903692814), Storage (39903692791) and Font Gallery (39903692794).
PR #6 merges the registered native installation/activation, interruption and
complete RAM-loss batch into main as `261dba7c`. Both full local registration
suites also pass. The local retirement/claim-closure follow-up is excluded from
that merge and its fresh-suite acceptance remains pending. New main CI is not
polled. Earlier checkpoint wording describes its state at that checkpoint.

Current batch through `92acb946` includes retryable claim closure, native combined
cancellation, unregistered-root pair cancellation, complete real-root cleanup,
RAM-bounded CODE packaging, independent acceptance suites and one-shot installer
release. Submitted head `59e3b88b` passes Quality 39894707699, Core runtime
39894707696 (including all twelve Stock suite jobs), Objects 39894707697 and
Runtime 39894707705. PR #5 merges that batch into main as `9b33e772`.
Local registered-lifecycle follow-up `a2a13331` and activation-cut work are not
included in that merge; their acceptance remains separate below. New main CI
is not polled. M8.0 remains open.

Previous CI submission: `af3e2a2e` includes the five implementation commits since
`50eadbea`, including the missing creation-layout CI artifact fix. Local acceptance
is recorded below. A later check finds Quality 39886905107, Objects 39886905191
and Runtime 39886905212 successful. Core 39886905229 completes its unit/package,
identity and recovery jobs, but both installation jobs are cancelled at the
30-minute job limit while progressing through the combined suite. That earlier
submission was not accepted; its split-suite successor is accepted above.
The runner now separates installation, publication,
real-template and resource-creation suites into independent clean-boot jobs with all existing
checks retained. The complete HP50g creation suite passes from a fresh boot
(about 11 minutes). The original combined installation/publication run also
passes, taking about 35 minutes locally, beyond the unchanged CI job limit.
The isolated template suite initially reached its previously uncompleted
recovery step without Flash writes; a 55-second restart control did not help.
The retained 150,186-byte
one-shot installer leaves only 22,776 bytes between the native stacks before
fault injection. Removing only that completed installer raises the gap to
194,409 bytes; the same injected fault then repairs in 1,923 operations and
passes the unchanged second-restart oracle. The real-store test now consumes
its one-shot HOME binding while preserving HPCTEMPLATE, and checks absence
explicitly. Fresh complete template-suite runs pass on HP49g+ and HP50g:
25,428 storage operations, 1,923 recovery operations, exact full Flash/catalog,
source preservation and unchanged second restart. This does not relax the
separate low-memory/resource-ownership requirements. Publication reuses the same
native installed-baseline helper, with its own fresh boot and evidence. Its
standalone entry passes conflicts, write failure, activation rejection,
publication, closed-repeat and both cancellation paths in about 14 minutes.
Both template suites take about 8 minutes. All 88 recovery Python tests,
pinned formatting (453 native / 84 Python files) and strict docs pass.

Current delivery checkpoints:
- **Claim closure interruption (locally verified, not submitted):**
  the maintained test cuts the actual phase-word program before any bit lands,
  after a partial clear and after complete closure. Exact emulator event fields
  and full Flash comparisons must match; reset must preserve the interrupted
  claim without writes, retry must close it, and two further resets must remain
  unchanged. HP49g+ and HP50g both pass: unwritten/partial/complete cases retry
  with one/one/zero SST operations respectively. Each first reset and both
  post-retry resets perform zero writes; exact cut events and complete Flash
  images match. Retirement/closure acceptance now
  has its own fresh-boot suite, sharing native installation setup while retaining
  all registration/activation/RAM-loss checks in the registration suite. The
  two-model CI matrix adds retirement with the existing 30-minute job limit.
  A separate clean ARM build passes; both full registration regressions are
  running against that build. Both full fresh retirement suites are now running
  against the same clean build; combined fresh-suite acceptance remains pending.
- **Retired claim closure (locally verified, not submitted):** a new
  internal operation revalidates the trusted installed transaction's retirement
  and requires the resource name to be absent from valid Stock HOME before
  clearing only its claim phase. Existing same-name objects, active transactions
  and invalid/unreadable RAM reject without writes. Trusted durable producer
  provenance, exclusive views and no allocation reuse remain caller obligations;
  the operation does not discover authority from a surviving claim. ARM analysis
  and the complete installation ASan/UBSan suite pass, including absence/presence,
  malformed RAM, active/intent rejection, partial-write retry and idempotent
  closure cases. A native SST witness restricts programming to the bound claim
  phase word and repeats closure to verify zero additional writes. Its maintained
  retirement checks compare all Flash bytes and preserve RAM resources; both
  model runs initially reject the active transaction with zero Flash changes,
  but fail the low stack guard before Stock return. The closure frame reserves
  568 bytes across deep validation. Moving directory/encoding buffers into a
  separate non-inlined phase reduces that frame to 56 bytes without increasing
  the 4 KiB witness stack. ARM packaging/analysis and the complete installation
  sanitizer rerun pass. Both native models now reject active transactions without
  writes, close the actually recovered claim with exactly one SST operation, and
  repeat closure with zero further writes. Full Flash comparison changes only
  the claim phase byte; resources, catalog, mappings and stack guards remain
  intact. Two subsequent resets per model preserve the complete Flash image and
  catalog with zero writes and no resource revival. These targeted native runs
  reuse the accepted installation snapshots; the expanded fresh registration
  suites are running. Native closure-interruption coverage passes as recorded`n  above; Flash reclamation remains open.
- **Post-recovery retirement prerequisite (locally verified, not submitted):**
  a read-only installation gate requires both the authority and journal to be
  fully retired before accepting a CREATED/CLOSING/CLOSED claim. It validates
  the trusted binding, installed root/catalog and exact claim allocation against
  actual Flash; the historical capsule cannot hide current damage. ARM static
  analysis and pinned formatting pass. After correcting the bank-damage fixture
  to target the actual native header, the complete installation ASan/UBSan suite
  passes, including partial phases, uncommitted claims, bank damage, read failure
  and unchanged outputs/storage. A native retirement witness now binds its
  expected fields from the independently reconstructed installation. The
  retirement suite checks rejection of the real active transaction and
  acceptance after actual recovery, preserving Flash, catalog, mappings and
  owning CODE/stack guards. ARM packaging, formatting and Ruff pass. Initial
  native probes stop during USB reception: Stock reports insufficient memory
  before the probe executes. The fixture now consumes its completed original
  HPCTEMPLATE copy through Stock commands in a separate snapshot before upload,
  checks unchanged Flash and active reservation bytes (or continued absence after
  recovery), then runs the read-only probe. Both HP49g+ and HP50g retests pass:
  active transactions reject with unchanged output, and recovered transactions
  accept the still-CREATED claim; complete Flash, catalog, mappings and execution
  guards remain unchanged. Both process exits are zero. This fixture preparation
  does not establish low-memory admission. The gate does not prove RAM ownership
  or permit release, claim mutation or Flash reclamation.
- **Native-created transaction with complete RAM loss (locally verified):**
  added a registration-path check that clears SRAM before resetting the actual
  installed/armed transaction, then declines Stock memory recovery. Initially both
  models fail: Flash remains unchanged, busy operation count is zero, captures
  are blank and the native Saturn mapping is unavailable. Retained-RAM recovery
  and activation-cut checks still pass. Clean-Stock and inert-registered-root
  cold-start controls both pass (61/62 catalog entries, zero writes). The armed
  intact-target control and a damaged-target control preserving its bank header
  both still fail at ROM PC 0x1dcd8 with zero writes, so erased target metadata
  alone does not explain the failure. Transplanting the same transaction Flash
  into the HP50g inert-root hardware snapshot passes both armed (two retirement writes)
  and unarmed (zero writes) cold starts. x50ng preserves a separate 4 KiB on-chip
  SRAM image across reset; the old RAM-loss setup cleared only external SRAM.
  Shared injection now clears both images and rejects incomplete snapshots before
  mutation; its 16 focused tests and Ruff pass. Clearing both alone lets HP50g
  recover the damaged sector (1,923 writes, unchanged repeat), but HP49g+ still
  fails before recovery. Neither USB/GPIO nor clock/timer/interrupt state alone
  fixes HP49g+ in corrected case-preserving controls. Initial grouped controls
  changed case-sensitive state keys and are invalid evidence. ROM disassembly
  identifies the stop as waiting for a RAM readiness sentinel before copying
  native state. HP49g+ whole-snapshot replacement passes; internal-SRAM-only
  replacement fails. Replacing only RTC state passes full damaged-sector repair
  (1,923 operations and unchanged repeat). The RTC loader restores an absolute
  timer anchor from the previous process, although the new execution clock starts
  at a different origin. The fix stores a within-second phase in version 2 and
  rebases it on load; version 1 retains its committed calendar with phase zero.
  Resume/stopped-clock/invalid-phase sanitizer tests and a CI-option emulator
  build pass. Both original failing snapshots now pass without transplanting
  device state or extending the timeout: 1,923 recovery writes, no resource/source
  surviving RAM loss, rebuilt catalog and zero writes on the second reset.
  Both output snapshots use valid version-2 fractional phases with obsolete
  absolute anchors removed. Pinned formatting and Ruff pass. Complete fresh
  registration suites, including activation cuts and RAM loss, now pass on both
  models with the fixed emulator: HP50g 942.2 seconds, HP49g+ 949.6 seconds.
  Each records 25,429 root-storage operations, 2,082 creation/cancellation
  operations, 2,077 activation operations and 1,923 damaged-sector recovery
  operations. Uncommitted/committed activation cuts require zero/two recovery
  writes; complete RAM loss requires 1,923, and repeated resets require zero.
  Both process exits and result files confirm success. This is local acceptance;
  CI acceptance remains pending. General startup ordering and transaction
  reclamation remain open.
- **Complete registered integration (locally verified):** both fresh-boot
  registration suites through `a2a13331` pass, taking 665/673 seconds on
  HP50g/HP49g+ respectively. Activation-cut coverage passes separately as below.
  Both full creation regression suites also pass (646/661 seconds). Later
  RAM-loss coverage is not included in those passing registration results.
- **Activation interruption boundary (locally verified, not submitted):** the
  registration suite now cuts the actual final authority write before or after
  its single changing phase bit reaches Flash. The oracle checks the exact
  operation/address/value/mask/read event and the complete Flash image against
  native-created records. Both reset paths retain the original template/catalog;
  an uncommitted authority must preserve its resource without writes, while a
  committed authority must recover/retire and release it. Both paths repeat reset
  to check stability. HP49g+ and HP50g both pass: the uncommitted case makes zero
  writes and retains the resource; the committed case retires the already intact
  target transaction with two writes and releases the resource. Both second
  resets make zero writes. These runs are separate from the completed
  pre-cut registration/creation suites recorded above. This does not
  reconstruct ownership of an interrupted unarmed resource or reclaim its claim.
- **Registered creation/configuration integration (local, not submitted):** the
  new preparation APIs require the exact registered root and reconstruct its
  fresh four-extent plan before permitting a transaction. Focused and full host
  sanitizer tests and ARM analysis/build pass. Both models pass registered
  creation, staged admission, owned cancellation and unchanged restart, with
  2,082 Flash operations each. Integration review then found that these creation
  witnesses used HPCLEASE while the real admission descriptor uses HPCRES.
  Those results do not establish automatic recovery through that descriptor.
  The packager now exports name/capacity from the actual linked descriptor;
  native generation, creation and the independent oracle consume that metadata.
  Invalid or mismatched descriptors are rejected, and unsupported witness
  capacity is rejected before root writes. Five template tests, including an
  alternate name/capacity, all 91 recovery/package Python tests, Ruff, ARM packaging,
  pinned formatting and strict docs pass. Fresh two-model flows with aligned
  configuration pass creation/cancellation/restart (2,082 operations each),
  followed by rejection of closed-transaction reuse with zero writes.
  A new registered activation witness checks the original fresh resource
  receipt after staged admission, arms the real authority and verifies an
  idempotent repeat. Its reset test injects only target-sector damage; all
  recovery records and the RAM resource must come from native installation.
  ARM packaging passes. Both HP49g+ and HP50g native activation/recovery runs
  pass: 2,077 operations create/arm the transaction, 1,923 restore the damaged
  sector through the installed guard, and the second reset makes zero writes.
  Complete Flash comparisons retain the root and claim; the reservation is
  released and the original source template/catalog survive both resets.
  A separate registration suite retains these growing integration cases without
  extending the existing creation job. Complete fresh-boot registration and
  creation suites through this checkpoint passed on both models, as recorded
  above; the expanded registration suite also passes as recorded above. This proves
  the isolated native-created recovery path, not arbitrary startup ordering,
  claim reclamation or general reconstruction of ownership after interruption.
  Those remaining gates and M8.0 stay open.
- **Registration before resource creation (locally verified):** an
  explicit claim-bound root-only installer uses the existing four-extent plan
  and writes only the recovery library. The maintained creation suite verifies
  complete Flash against that root alone, keeps journal/authority/claim extents
  erased, checks the absence of a resource and consumed installer binding, then
  requires Stock to register the unchanged root after reset without Flash writes.
  ARM packaging and formatting pass; HP49g+ and HP50g native runs pass,
  with 25,429 Flash operations each and zero additional writes on restart.
  This establishes an inert registration boundary for the next fresh transaction;
  transaction continuation, post-reset ownership and early startup remain open.
- **Real-root cleanup integration (locally verified; CI acceptance pending):**
  the maintained installer now cancels its same-owned staged pair, discards the
  original fresh RAM reservation and closes/repeats its creation claim. The new
  oracle compares complete Flash, closed envelopes and claim, consumed receipt,
  resource absence, retained template, execution guards and restart behavior.
  ARM build/package and 88 recovery Python tests pass. HP49g+ and HP50g both
  pass the complete cleanup oracle (27,511 Flash operations each) and monitored
  restart with zero additional writes. Restart monitoring is now explicitly
  enabled; the initial check otherwise read the prior saved operation counter.
  Pinned formatting and strict docs pass. The combined image has 35,076
  initialized bytes, verified intact on both models. Packaging now
  derives its copy limit from the main-RAM window instead of the conservative
  32 KiB cap: Saturn CE decrements the five-nibble A field. Complete source plus
  runtime envelope bounds remain enforced; count/extent tests cover the former
  boundary and 64 KiB; native execution above 32 KiB passes on both models.
  Root retirement, post-reset ownership, early startup and reclamation remain open.
- **Unregistered-root cancellation (locally verified; CI acceptance pending):** real installation
  has not registered its new root when resource creation finishes, so the
  registered-library cancellation gate cannot be reused unchanged. Added a
  separate same-owned complete-pair primitive: compare the retained READY journal,
  full unarmed authority and unchanged target; finish the journal tombstone before
  clearing authority metadata/status. It supports interrupted cancellation but
  rejects armed/publication states, damaged bodies and out-of-order closure.
  No RAM release, root retirement or post-reset adoption is implied. ARM build
  and focused ASan/UBSan pass, including every read cut, all status bytes and
  partial/failed/lost-ack/silent programming. The full installation ASan/UBSan
  regression, pinned formatting and strict docs also pass. Real-root native
  integration now passes in the real-root cleanup checkpoint above.
- **Native creation cancellation (locally verified; CI acceptance pending):** a maintained
  combined witness publishes INTENT, creates RAM, commits CREATED, stages the
  transaction, cancels it, discards its original fresh reservation and closes the
  claim under the same invocation. Repeat closure must not write. Its independent
  oracle checks full Flash, consumed receipt, resource absence, unrelated HOME,
  execution guards/Stock return and unchanged closed records after restart.
  The fresh-cancellation path deliberately omits the unrelated already-armed
  success shortcut. ARM packaging passes within the unchanged loader limit;
  87 Python tests, Ruff, pinned formatting and strict docs pass. HP49g+ and HP50g
  both pass the complete cancellation/release/closure/repeat and restart oracle.
  This is lifecycle fixture integration, not real-root installation ownership
  reconstruction.
- **Creation-claim closure (locally verified; CI acceptance pending):** the final record primitive
  compares the original INTENT's immutable bytes, accepts only CREATED/partial
  CLOSING/CLOSED, and clears the phase without touching adjacent allocations.
  Transaction closure, safe RAM release and uninterrupted ownership remain caller
  prerequisites; matching bytes cannot authorize post-restart adoption. ARM build
  and packaging pass. Host tests cover all phases, each immutable-byte corruption,
  every read cut, partial/failed/lost-ack writes and silent programming failure;
  the full installation ASan/UBSan suite passes, followed by focused sanitizer
  acceptance of the additional silent-write and invalid-original cases. The native
  lifecycle fixture and real-root cleanup above now pass; post-reset ownership
  integration remains open, and this primitive does not grant release permission.
- **Real-root resource creation (locally verified; submitted in `af3e2a2e`, CI pending):** after the durable
  INTENT, the real installation witness creates the bound 32 KiB HOME string in
  the same exclusive invocation, retaining the original creation receipt and
  immutable snapshot. It commits CREATED through receipt/content validation,
  then repeats completion without additional Flash writes. Authority stays 7F;
  registration/activation and restart ownership are not granted by this result.
  Independent verification compares the complete resource, claim and Flash, and
  requires resource persistence after restart. Local callback wrappers avoid
  unsupported ARM GOT relocations without relaxing packaging. ARM build/package,
  static analysis (excluding the native fixed-address diagnostic) and 66 Python
  tests pass. Initial uploads exceeded Stock memory before execution. The snapshot
  now reuses the no-longer-needed source-template copy after sealing/storing the
  prepared root, reducing the package from 214,210 to 181,450 bytes. Template
  buffers now explicitly require word alignment. Both reduced-footprint native
  name-rejection cases pass. Normal runs write root/pair/INTENT, then resource
  creation returns NOSPACE (-3), retaining execution with no CREATED claim.
  Captured diagnostics show 27,500 Flash operations and an empty receipt.
  An explicit owned in-place preparation API now retains all placement/sealing
  checks, accepts only exact source/destination identity and leaves outputs
  untouched on failure (the owned draft is disposable). Copying APIs still reject
  overlap. The store witness aliases its two template symbols, then reuses that
  buffer for the resource snapshot after Flash storage. Package size is 130,602
  bytes; the original HOME template and full 32 KiB resource remain required.
  ARM build/package, static analysis, 66 Python tests and the full host sanitizer
  suite pass, including ten new in-place cases. Both native name-rejection cases
  pass. Normal creation now returns INCOMPLETE (-14) after native writes, with
  no receipt/CREATED transition; captured initialized execution bytes have changed.
  This is not a passing resource install. Nibble-level comparison of both saved
  captures finds the execution copy relocated from physical SRAM nibble offset
  0xd0150 to 0xc012d: exactly 0x10023 nibbles, the resource plus its directory
  entry. The CPU remains at the old copy. The witness ran from movable USEROB
  without execution protection. Next validate Stock-owned TEMP execution before
  resource creation and reject unsafe placement before any Flash publication;
  retain ownership and staged state meanwhile.
  A subsequent HP50g one-shot RCL/PURGE execution creates the exact 32 KiB
  resource: all six stage results are zero, the original receipt is populated,
  and Flash operation count is 27,501. Full verification still fails Stock return:
  the 4 KiB stack low guard is overwritten and entry deliberately retains control.
  TEMP execution and sufficient measured stack space both need maintained
  acceptance; neither successful stage diagnostics nor process exit proves return.
  The witness now validates the complete live image against one TEMP allocation
  before publication and uses its bounds as mutation fences. Maintained E2E adds
  USEROB rejection and consumes only the one-shot installer binding for the valid
  path. Its guarded stack is 8 KiB; duplicate inventory diagnostics stay in the
  dedicated witnesses. ARM packaging passes at 131,002 bytes (31,560 initialized),
  with unchanged loader/relocation checks. Compiler stack reports, 87 Python tests,
  strict docs and pinned formatting pass. HP49g+ and HP50g now pass the complete
  maintained sequence: existing-name rejection, unsafe USEROB execution rejection,
  exact resource/CREATED publication, repeat without additional writes, intact
  code/stack guards, Stock return and unchanged root/resource after restart.
  Each performs 27,505 Flash operations. Authority remains unarmed; this does not
  close ownership reconstruction, startup ordering, activation or reclamation.
- **Real-root creation intent (locally verified; submitted in `af3e2a2e`, CI pending):** before any root write,
  the native witness resolves HOME and rejects an existing HPCLEASE name. After
  staging the real root's pair, it publishes INTENT at the fourth planned extent,
  binding the resealed root identity, all coordinates, journal CRC, name and RAM
  capacity. No RAM resource is created yet; this does not reconstruct ownership
  after a return/reset. Independent full-Flash verification includes the claim.
  ARM build/package and 66 Python tests pass. Both native storage/restart cases
  pass with 27,500 operations each. Existing-name cases initially hit Stock's
  remote-command limit; setup/execution are now separate short commands. Both
  saved rejection captures pass after correcting the verifier to expect the
  prepared (unwritten) slot in diagnostics: zero Flash operations, original string
  and template preserved. Strict docs and pinned formatting pass.
- **Real-root transaction staging (locally verified; submitted in `af3e2a2e`, CI pending):** the maintained
  claim-bound storage witness now writes its real recovery library, READY journal
  and unarmed authority in the same exclusive invocation, using the four-extent
  plan rather than fixture addresses. Each write callback is confined to its owned
  allocation. Failed pair publication retains the invocation. The creation claim
  stays erased; no RAM ownership or activation is implied. The independent oracle
  reconstructs all records and compares complete Flash, allowing planned records
  in different banks. ARM build/package, analysis and 66 Python tests pass;
  Initial saved inputs contained an older template; both programs rejected it
  without changing Flash. The runner now checks source-template equality before
  execution. Both current-template writes and restarts completed. Initial restart
  verification rejected known staged records; the corrected verifier requires their
  exact independent bytes and native allocation boundaries before interpreting only
  their statuses as tombstones. Both saved captures pass the complete verifier:
  27,378 operations each, root 0x160105, journal 0x157e75, authority 0x158e91.
  Physical Flash, source template and native root registration survive restart.
  CI artifact review also found the creation-template layout JSON missing from
  the upload list; it is now included and the workflow parses successfully.
- **Adjacent-allocation activation (locally verified; submitted in `af3e2a2e`, CI pending):** integration
  review found that publication retry requires an erased successor, whereas a
  completed staged authority may precede an already-published creation claim.
  A separate activation primitive accepts only exact 7F/7E and complete READY
  journal/binding, changing only its phase byte and preserving the successor.
  Partial-publication retry keeps its erased-tail requirement. ARM build and
  static analysis and full host sanitizer tests pass (all 256 phases, every read
  failure, failed/silent writes and lost acknowledgements). The existing native
  publication witness now uses this primitive after staged admission and fresh
  receipt validation; HP49g+/HP50g both pass activation and transaction closure
  with 1,958 Flash operations each. Strict docs and pinned formatting pass. Complete
  installer integration, fresh receipt checks and safe startup remain required.
- **Staged creation admission (locally verified; submitted in `af3e2a2e`, CI pending):** the
  streaming gate requires exact unarmed authority 7F, READY journal and CREATED
  claim, checking root/catalog and native boundaries in both current and proposed
  Flash views. Current damage cannot be excused by the recovery image. Host
  ASan/UBSan coverage passes all 256 authority and claim values, including direct
  staged output checks. ARM build/static analysis, 66 Python tests, strict docs
  and pinned formatting pass. HP49g+/HP50g both pass complete creation, staging
  and read-only admission sequences, rejecting a wrong binding while preserving
  Flash, catalog, reservation and unrelated HOME data. Publication leaves one
  checked stack result; the next command clears it, and exact input/output depths
  are verified. At that checkpoint, separate programs stayed within the then
  conservative 32 KiB packaging limit (superseded by the RAM-bound check above).
  Ownership, safe startup, activation and cancellation remain separate requirements.

Batch `50eadbea` is submitted for CI: admission, four-extent
planning/drafts, intent publication, receipt-gated completion and native real-root
storage/restart. All 452 native and 84 Python files pass the pinned CI formatters.
One later CI check finds Quality 39839641114, Objects 39839637487 and Runtime
39839637491 successful. Core 39839637484 failed in both installation jobs because
the artifact omitted template-creation-layout.json; local f43009f0 adds it. That
fix is not submitted yet, so CI acceptance and M8.0 completion remain open.
- **Claim-bound real library storage (locally verified; CI pending):** the existing
  selected-slot writer now has a maintained claim-bound template variant using
  the same preparation and storage code. Independent verification reconstructs
  the exact library from all three bound addresses, checks the four extents and
  original erased contents, and compares the complete resulting Flash. Restart
  must discover the same library without changes. Complete ARM build, Ruff and
  66 Python tests pass. Both native storage/restart runs pass with 25,329 Flash
  programming operations each; all unrelated Flash and source data remain intact. Journal,
  authority and claim publication/activation remain separate open work.
- **Real claim-bound template preparation (locally verified; CI pending):** normal
  and claim-bound templates share the build generator. A maintained native
  witness loads the actual sealed recovery/admission library, plans four extents,
  patches its owned claim coordinate and reseals it in RAM. The independent
  verifier checks complete draft bytes, all four erased-tail boundaries, source,
  catalog, CODE/guards and zero Flash writes. ARM build/static analysis, Ruff and
  66 Python tests pass. Both native executions completed; their initial verifier
  failed on an incorrectly wired argument. After correcting it, both saved
  captures pass the maintained full verifier without repeating execution. Storage/publication of this integrated root and
  restart ownership remain open.
- **Native creation publication (locally verified; CI pending):** a maintained
  witness checks name absence and the fixture's exact erased tail, publishes
  INTENT with a driver-level partial write/retry, creates the RAM reservation,
  and commits CREATED through the receipt gate; repeating completion must make
  no writes. The independent verifier checks exact full Flash, RAM binding,
  unrelated HOME data, owning CODE, stack guards and Flash busy accounting.
  Both initial runs passed publication/receipt/Flash checks but failed the
  unrelated-variable check because the fixture had not created that variable.
  Setup now creates it before execution; both corrected runs pass in full.
  The full ARM build, ARM analysis, Ruff and Flash-record ASan/UBSan pass. Native compilation exposed a GOT-based external memcpy reference in the
  creation encoder; a local noinline copy helper restores relocatable calls,
  with the strict relocation gate unchanged. This fixture is not the production
  claim-bound root installer, restart ownership, cancellation or reclamation.
- **Receipt-gated CREATED transition (locally verified; CI pending):** the RAM
  wrapper rechecks the original receipt, full snapshot and claim binding/name
  before the Flash primitive verifies all immutable bytes and changes only the
  INTENT-to-CREATED phase bit. CREATED is idempotent for the same owned attempt;
  all other phases reject. Tests cover 256 phases plus receipt/content/name/
  binding mismatches, read failure and both failed/lost-ack writes, preserving
  unrelated Flash, RAM and receipts. Complete ARM build, mutation and
  installation ASan/UBSan suites, ARM analysis and strict docs pass. Native producer
  integration and restart ownership remain open.
- **Creation-intent publication primitive (locally verified; CI pending):** writes
  immutable bytes, then the deleted envelope, then INTENT last. Same-attempt
  retry accepts compatible incomplete bytes or an exact published INTENT, never
  later phases or occupied following storage. Tests exercise every programming
  boundary with four partial-write masks, all 256 phases, binding corruption,
  occupied end markers, read failure and occupied fresh publication. Complete
  ARM build, full host ASan/UBSan, all 15 ARM analysis contexts and strict docs
  pass. Native producer
  integration must still establish name absence and owned allocation before
  publication; native integration, cancellation and restart ownership remain open.
- **Claim-aware installation drafts (locally verified; CI pending):** preparation
  preserves cross-port/catalog namespace checks, plans four extents, rebinds the
  trusted layout's claim field before identity sealing, and revalidates placement
  before publishing output coordinates. Nine host scenarios compare the entire
  draft to independently packaged bytes and reject invalid/overlapping claim
  fields, missing output and read failures without publishing coordinates. ARM
  build/static analysis, Ruff, strict docs and the complete host ASan/UBSan
  suite pass.
  Native preparation/publication, interruption and reclamation remain open.

- **Creation-claim space planning (locally verified; CI pending):** the shared
  erased-tail planner now supports library, journal, authority and creation-claim
  extents together, retrying bank combinations if the claim cannot fit. Existing
  three-extent callers retain their selection order. Eight new host scenarios
  cover split/same-bank placement, backtracking, insufficient space, target
  exclusion, read/dirty failures and unchanged outputs/Flash. The complete host
  installation suite passes ASan/UBSan, and all 15 ARM static-analysis contexts
  pass. Claim-aware draft integration is described below; durable publication
  remains open.

- **Creation-claim runtime admission (locally verified; CI pending):** the reader
  revalidates the installed transaction, exact binding and journal CRC, then
  requires the claim to occupy a complete native deleted allocation rather than
  an interior payload. Admission roots with a sealed claim slot require the
  CREATED phase. The complete ARM build, Ruff and full installation host
  ASan/UBSan suite pass, as do ARM static analysis, 66 recovery Python tests
  and strict docs. Both HP49g+/HP50g pass guarded low-memory recovery and
  repeated restart and RAM-loss recovery. Both also reject missing, intent-only
  and mismatched-binding claims without RAM deletion or Flash operations.
  Both maintained native suites now pass in full, including resource-guard
  regression cases. These are local results, not CI acceptance.
  The native fixture constructs claims offline, so this is not evidence of a
  production publisher, restart ownership, startup ordering or reclamation.

- **Sealed creation-slot packaging (locally verified; CI pending):** optional
  `--creation-claim-slot` binds only an owned aligned initialized ELF placeholder
  and records its generated admission-command coordinate before identity sealing.
  Missing slot/identity, wrong placeholders and header/end/transaction overlap
  reject without replacing outputs. Tests independently recompute the complete
  identity digest, show that changing the slot invalidates it, and preserve an
  identical unsymbolized word. All 21 packager and 66 recovery Python tests pass;
  the full ARM build passes. Packaging the same real recovery/admission ELFs with
  the previous/current tools produces byte-identical default libraries and equal
  layouts. Runtime claim admission and claim-aware placement/draft follow-ups
  are recorded above; durable creation publication and lifetime/reclamation
  remain open.

- **Persistent creation-claim format (locally verified; CI pending):** an internal
  `HPCOWN1` codec stores the exact reservation name and complete installation/
  transaction binding in a 240-byte native deleted allocation. A separate
  monotonic phase distinguishes intent, verified creation and closure; CRC
  excludes only that mutable phase. Tests cover all 256 phase bytes, each
  immutable-byte fault, each binding-byte mismatch, truncation, overlap/bounds,
  name/capacity errors and unchanged outputs, with an independent CRC oracle.
  The full Flash-record ASan/UBSan suite and ARM analysis pass. Both models pass
  the maintained offline five-phase Stock restart probe with exact Flash/catalog
  and zero Flash operations. This validates the codec and native visibility only:
  sealed-root allocation binding, publication before RAM creation, interrupted
  creation/retry, admission and reclamation are not implemented by this codec.

- **CI tool parity correction (locally verified; CI pending):** `22103fb5` Quality
  (39825721704) still rejects two cast/unary-operator spaces in bound replay.
  Previous local checks used system clang-format 18, while Quality selects the
  pinned ATfE 22.1.0 formatter. Passing the former did not fix or validate the
  CI failure. The two lines are now formatted with ATfE; all 452 tracked native
  sources pass that formatter, and all 83 tracked Python sources pass pinned
  Ruff after normalizing only checkout line endings to Git's LF form. Objects
  (39825721928) passed; Core (39825722002) is running and Runtime (39825721891)
  is queued at this check. No repeated polling or CI acceptance is claimed.

- **Fresh receipt activation gate (locally verified; CI pending):** publication now
  stages/retries an unarmed authority, validates the exact staged installation
  and rechecks its original creation receipt and entire reservation snapshot
  before activation. The shared read-only checker rejects wrong address,
  capacity, name, content, missing objects and read errors without writes or
  receipt changes. Both native models pass normal publication (1,958 Flash
  operations) and a maintained rejection witness with a mismatched receipt
  after successful staging. Rejection preserves exact unarmed Flash, reservation,
  unrelated HOME data, catalog, CODE/guards and retained IRQ ownership. Full
  mutation ASan/UBSan, ARM analysis, 66 recovery Python tests and Ruff pass.
  This is an invocation-scoped activation prerequisite; durable creation proof,
  interruption/restart ownership, startup ordering and reclamation remain open.

- **Same-bank admission parity (locally verified; CI pending):** the installation suite
  now checks both buffered and low-memory streaming admission against a journal
  and adjacent authority behind the target bank header. Six cases cover READY
  recovery, valid retirement, retirement before repair, closed records against
  new damage, a competing journal and an authority read failure. Successful
  paths agree on phase and exact installed coordinates; rejection leaves output
  and all Flash unchanged. The full installation suite passes ASan/UBSan and
  clang-format 18. This strengthens admission coverage; durable creation proof,
  interrupted RAM creation, startup ordering and reclamation remain open.
- **Previous submission CI (`1deb8872`):** Runtime (39483875385) and Objects
  (39483875257) passed. Quality (39483875241) rejected two formatting lines
  in bound replay; the earlier local clang-format 18 check did not match CI's
  pinned ATfE formatter (see correction above). Core (39483875319)
  failed both real-template storage paths at the busy-state verifier. The HP50g
  artifact stops in ARM code with operation 23,813 still busy (1,524,028 of
  1,524,032 status reads), under the fixed 17-second execution sequence. The
  local harness now exposes an execution budget; real-template storage uses
  60 seconds while retaining full Flash, catalog, CODE and busy-state checks.
  Both models now pass complete storage with 24,569 programming operations,
  exact busy-read totals, full Flash/catalog/CODE and source preservation.
  All 66 recovery Python tests, Ruff and strict docs pass. This harness
  follow-up is submitted with CI acceptance pending.

- **Fresh reservation cancellation cleanup (submitted, CI pending):** an internal discard
  operation checks the original name, creation receipt address/capacity and
  complete untouched snapshot before removing a reservation. Success consumes
  the receipt; pre-write rejection preserves RAM, while any failure after a
  write attempt retains the invocation as INCOMPLETE. The mutation ASan/UBSan
  suite covers mismatches, every lost-write acknowledgement and failed final
  readback. Both HP49g+/HP50g pass the maintained create/stage/cancel/discard
  path with unrelated HOME data, exact Flash/catalog and CODE/guards preserved.
  The witness executes from Stock-preserved TEMPOB, avoiding movable USEROB;
  separate directory-check frames keep removal within its 4 KiB stack.
  ARM analysis and Python checks pass. This is same-invocation cancellation
  cleanup, not ownership reconstruction after reset, active workspace release
  or Flash reclamation. Commit `4ac72852` is included in the current submission.

- **Installed transaction recovery (submitted, CI pending):** the real planner places
  the library in the second user bank and the journal/authority in the first
  bank's free tail. Injecting damage into that first bank's header after a
  real installation leaves both native models unrepaired. A new host regression
  reproduces the cause: bound replay discovers journals through the damaged
  current bank header before applying the validated target image. The local fix
  scans the restored view with separate image/scanner buffers, while retiring
  transactions still require equality with the actual target. The regression,
  66 Python tests, ARM analysis and complete authority ASan/UBSan suite pass.
  Additional focused sanitizer checks preserve Flash/output on rejection of
  unfinished retirement and competing journals. Both native models pass fresh
  real-template installation, recovery/retirement with 1,923 SST operations,
  exact remaining Flash/catalog/source preservation and a second boot with zero
  writes. This path is connected to maintained installation E2E; commit `a9b720f4`
  is included in the current submission, with CI acceptance pending. Fault preparation validates tails and sector overlap
  instead of requiring a separate empty bank. This injected transaction does
  not prove production activation authority or safe reclamation.

- **Current submission batch:** damaged-bank bound recovery (`a9b720f4`)
  and fresh reservation cancellation cleanup (`4ac72852`) follow the previous
  real-template/staging submission (`1deb8872`). Both models pass the maintained
  native paths described above; final complete ARM build and strict docs pass.
  CI acceptance is pending; new runs are not polled. M8.0 is not closed.

- **Real recovery template integration (local, open):** packaging the actual
  guarded recovery/admission ELF images produces a 49,588-byte native record.
  Embedding that record in the installation witness exceeds the recovery CODE
  then-conservative 32 KiB initialized-image packaging limit; native packaging
  rejected it at that checkpoint. The RAM-bound check above supersedes that cap.
  The small empty-configuration installation fixture therefore does not prove
  real-template installation. A separate-input prototype now passes native
  preparation on HP49g+/HP50g: the installer verifies the full template against
  its compiled sealed identity, and its 16,596-byte initialized image fits the
  loader bound (138,706-byte package). The complete 49,588-byte prepared record
  matches an independently rebound/sealed package; placement, owning CODE,
  stack guards and startup inventory pass. The source template, Flash and
  library catalog remain unchanged, with no Flash programming operations.
  HP50g also rejects a different valid sealed library before copying or
  preparation, preserving Flash/catalog/CODE/guards with no programming.
  Both models additionally store the complete independently rebound record at
  the selected `0x160105` slot with 24,761 SST operations, preserving every
  other Flash byte and the executing CODE/guards/catalog. Stock restart
  discovers the exact library at that slot without changing Flash. The write
  witness initializes 20,436 bytes and packages to 146,474 bytes.
  The maintained build now generates the real template, sealed identity and
  layout constants from the current recovery/admission ELFs. Separate input
  loading and both preparation/storage witnesses are included in installation
  E2E; the workflow carries their layout artifact. A clean repository build
  matches the tested packages exactly. Both HP49g+/HP50g pass the maintained
  preparation, storage, restart, source-preservation and wrong-template
  rejection flow. The maintained preparation/storage initialized images are
  16,684/20,520 bytes, with packages of 138,882/146,634 bytes. Sixty-three recovery
  Python tests, ARM analysis and formatting pass. These are local results;
  CI acceptance and the full transaction lifecycle remain open.
  Next: connect real installation to transaction recovery and safe reclamation.

- **Unarmed transaction staging (local):** new internal staging
  and staging-retry operations store the complete authority body at `7F`,
  without enabling replay. Retry accepts only compatible `FF` or complete `7F`;
  it cannot downgrade an activation/closure state. Explicit activation uses
  the existing authority-finishing operation. A native staging witness on both
  HP49g+/HP50g writes the independently encoded journal/binding, rejects
  installed replay preflight and repeats staging without further writes.
  Exact other Flash, target, catalog and owning CODE/guards are preserved;
  both runs observe 1,953 SST operations. Eleven verifier tests, ARM packaging
  and core ARM analysis pass, as does the complete authority ASan/UBSan suite.
  Both native models restart with exact Flash/catalog preserved and unchanged
  cumulative SST counters. The maintained staging/restart flow is now connected
  to installation E2E and passes on both native models.
  A read-only staged-installation preflight now verifies complete `7F`/READY
  records, actual allocation boundaries and exact library/catalog identity in
  both current and proposed views. It rejects a damaged current target rather
  than hiding it with the journal image. Twenty-one host scenarios cover
  alternate valid target contents, contiguous/separate placement, wrong phases,
  corrupt data/catalog/root, gaps/interior records, read failure and insufficient
  scratch; errors preserve outputs and all storage. The complete authority
  ASan/UBSan suite, core ARM analysis and both native integrated flows pass.
  Status `7F` alone is not installation provenance or RAM ownership: an older
  interrupted direct publication can have the same bytes. This step still
  needs the trusted fresh-installation claim and resource lifecycle.

- **Cleanup/inventory CI submission:** the locally validated cleanup
  (`0dbd6429`) and Port 1 inventory (`444bbe8f`) changes are submitted together
  with their maintained E2E paths. CI acceptance is pending; new runs are not
  polled. Startup entry selection, durable installation authority and resource
  reclamation remain M8.0 requirements.

- **Port 1 startup inventory extension:** the read-only native witness
  now resolves ERAM catalog bindings through the existing validated Port 1
  mapping, keeping its Saturn-window bounds separate from Flash geometry.
  The independent oracle reads the corresponding saved SRAM bytes and rejects
  mismatched IDs/out-of-window callbacks. Ten verifier tests, ARM packaging and
  ARM static analysis pass. Both native models report the expected 62 libraries
  and 17 callbacks with a Port 1 library installed, preserving all Port 1 bytes,
  Flash, catalog and owning CODE. The maintained Port 1 setup/inventory flow
  also passes on HP50g and is included in installation E2E. This extends
  callback coverage, not startup authorization or durable installation
  ownership. The proposed internal use of research ID 4 is awaiting
  maintainer confirmation; the public recovery packager's ID policy is unchanged.

- **Abandoned-library cleanup implementation:** tail
  recovery now accepts a complete CRC-checked unpublished library only as the
  final allocation, clears its metadata before status, and preserves its
  payload. Live libraries and partial removal states are not adopted. Host
  installation tests pass with 126 interruption combinations across fourteen
  unpublished statuses and all three cleanup writes, including retry and
  unchanged payload checks. Both HP49g+/HP50g clean the saved `0xFB` native
  interruption in three SST operations, with exact other Flash bytes, original
  catalog, owning CODE and stack guards preserved. Nine Python verifier tests
  pass. Both models additionally restart and finish cleanup after cuts at the
  metadata and live-status writes, requiring only the remaining two or one
  operations. The maintained storage suite now includes final-commit
  interruption, Stock restart and cleanup; integrated runs pass on both models.
  These are local results, not CI acceptance or M8.0 closure.
  Broader regression exposed an overly permissive zero-metadata path for
  backups; the implementation now restricts odd unpublished cancellation
  markers to parsed libraries. The existing damaged-backup rejection test is
  retained unchanged. The narrowed implementation passes the installation
  ASan/UBSan suite and ARM static analysis. Both native models also pass the
  narrowed zero-metadata retry, with exactly two remaining writes and preserved
  Flash/catalog/guards. The complete Flash creation/write ASan/UBSan regressions
  finish successfully, as does a focused run of the unchanged partial-backup
  recovery/corruption-rejection tests. Implementation at `0dbd6429` is included
  in the accumulated submission above.

- **Interrupted fresh-library commit gap (local):** on both HP49g+/HP50g,
  cutting the final status program at SST operation 286, busy read 1, mask
  `0x0400` leaves status `0xFB` at slot `0x157e75`. All other Flash bytes match
  the successful stored draft. Stock restarts normally, preserves that Flash
  and keeps the original 61-library catalog without registering ID 1703.
  Before the local cleanup change, C tail recovery against both saved native
  states returned DIRTY with zero writes: hidden-library cleanup was not covered
  by the backup-only path.
  The cleanup implementation above addresses this bounded interruption case;
  matching payload bytes still do not authorize completing a live installation.
  These results do not establish
  arbitrary torn-state boot safety or durable installation ownership.

- **Previous placement/storage CI submission:** the accumulated placement, namespace exclusion, grouped
  space planning, runtime sealing, draft preparation and selected-slot storage
  work is submitted together after local validation. The native preparation
  checks reuse existing E2E launches; storage adds one write and one restart
  phase per installation model. A later check of submitted `7c958c30` finds
  Quality successful (39471377075), Core running (39471377029), and Objects
  (39471377195) / Runtime (39471377067) queued. CI acceptance remains pending;
  no repeated polling or M8.0 closure.

- **Local selected-slot library storage:** an internal storage primitive now
  revalidates all six banks, library-ID absence and the exact selected tail before
  appending at the planned address. Ordinary create still chooses its first free
  slot; this explicit variant allows installation planning to choose another
  bank without silently moving the record. Fourteen host cases pass ASan/UBSan:
  all six bank locations, gaps/interior/out-of-range addresses, selected-tail
  exhaustion, later read/damage/ID conflicts and a failed program followed by
  verified discard. Bytes outside the selected allocation remain unchanged.
  Both HP49g+/HP50g now prepare and store the exact rebound library at the
  selected address through the SST driver, with busy-read operation counts
  checked. Full Flash differs only by that record; prepared content, native
  catalog, owning CODE and guards validate. A subsequent Stock restart discovers
  the same library at the same slot with exact Flash preserved. The maintained
  installation suite now includes storage and restart verification. Eight
  verifier tests and all witness package builds pass; strict PIC checks remain
  enabled. This stores only a library and inherits append's returned-error
  cleanup contract; durable
  installation claims, registration and power-loss/startup recovery remain open.

- **Local installation draft preparation:** the runtime now validates an owned
  sealed template, plans all three allocations with namespace exclusion, updates
  both layout-named HPCBIND1 descriptors in separate staging RAM, reseals the
  identity/CRC and repeats placement checks before returning coordinates.
  The complete prepared record matches an independent Python-packaged reference.
  Eleven preparation scenarios pass ASan/UBSan, including invalid/overlapping
  layout offsets, corrupted templates, short/aliased staging and a Flash read
  failure after the draft is populated. Failures preserve coordinate outputs;
  staging is explicitly disposable, while source/Flash/catalog remain intact.
  ARM build/static analysis pass. Both HP49g+/HP50g now execute the complete
  path: absent-ID drafts match independently repackaged bytes, and existing-ID
  rejection preserves sentinel coordinates/staging. Independent allocation
  walks validate all three extents, without holes or target overlap. Exact
  Flash, catalog, owning CODE and stack guards remain intact; no Flash operations
  occur. The maintained missing/matched E2E launches include these checks without
  extra boots. Seven verifier tests pass, including altered draft/coordinates,
  failed-output/staging and Flash-change rejection. This remains a draft, not
  durable ownership, registration, safe startup or permission to publish.
  These lifecycle gates remain open; this follow-up is local, not CI acceptance.

- **Local runtime identity sealing:** an owned packed-library template can now
  receive its HPCROOT1 span/offset/digest and native CRC in RAM, matching the host
  packager. All twenty independent hashlib/native-CRC vectors match byte for
  byte across descriptor/object nibble alignment and SHA padding boundaries.
  Invalid bounds, damaged/nonempty placeholders and insufficient capacity leave
  the input unchanged; allocation padding is preserved. Identity ASan/UBSan and
  ARM build/static analysis pass. The native preparation fixtures above also
  exercise sealing, without granting permission to authenticate arbitrary code:
  trusted template/layout provenance remains required. The draft-preparation
  step connects rebinding and final placement checks; installation publication
  and lifetime gates remain open.

- **Local fresh-installation space planning:** installation preflight can now
  place the library, journal and authority together after namespace exclusion.
  It validates all bank tails and searches bank combinations, consumes tails
  without gaps and keeps every extent outside the selected target sector.
  Separate banks are supported; it backtracks when placing the library in the
  first candidate would consume the only space large enough for the journal.
  Eleven space cases plus the twelve namespace cases pass ASan/UBSan in the
  installation suite, including insufficient pair space, target overlap,
  malformed/read-failed Flash and unchanged failure outputs. Successful split
  layouts also pass the existing journal/authority encoders. ARM build/static
  analysis pass. The native preparation fixtures above now exercise grouped
  planning as part of the full draft path. It reserves no bytes and grants no
  authority: exclusive storage ownership, owned publication, registration,
  startup and reclamation remain
  required. This is local work, not a new CI submission or milestone closure.

- **Local cross-port installation preflight:** the recovery installation planner
  combines read-only Flash selection with complete caller-supplied Port 0/1
  chains and a strictly ordered native catalog. Live unregistered RAM libraries
  conflict too; malformed later RAM records/catalog entries and unreadable views
  reject without publishing an address. Empty ports require readable terminators.
  Twelve host scenarios cover success, each RAM port, catalog and Flash conflicts,
  read failures, malformed chains/catalogs and missing views; all preserve Flash,
  RAM and catalog bytes. The installation suite passes ASan/UBSan and ARM static
  analysis passes. The maintained installation witness now captures Port 0 from
  the native start pointer and Port 1 through the validated RAM mapping. Both
  HP49g+/HP50g pass absent-ID selection and existing Flash-library rejection:
  an independent allocation walk checks the chosen address, exact Flash remains
  unchanged, and the existing owning-CODE, guards, catalog and startup-inventory
  checks pass. These checks reuse the existing missing/matched E2E launches.
  Six verifier tests pass, including wrong-result/address and changed-Flash
  rejection. Unregistered RAM conflicts have host coverage only: native
  preparation showed that ordinary Port 0/1 STO already registers the library
  on both models, so those fixtures cannot prove an unregistered conflict.
  Separate native probes reject each RAM-port conflict on both models with
  exact library content/terminator preserved, unchanged catalog and zero Flash
  writes. Port 1's ERAM callback inventory is outside the original startup
  inventory fixture; its conflict result is not an inventory acceptance claim.
  Stable complete mappings, storage reservation,
  registration, early startup and durable ownership remain caller/installer
  obligations; ordinary storage APIs retain per-port semantics.

- **Local installation placement planning:** a read-only library planner now
  validates all six Port 2 banks and selects the same erased allocation as
  creation under a stable view, with no programming callback required. Creation
  shares its selection implementation. Existing IDs, including unregistered
  Flash libraries, remain conflicts; matching deleted payloads are not adopted.
  This provides coordinates before installation metadata is finalized, without
  reserving storage or granting publication authority. Actual C against saved
  HP49g+/HP50g Stock states agrees with an independent allocation parser and
  preserves the output on an existing-library conflict. Cross-port/catalog
  exclusion, owned publication, registration and early entry remain installer
  requirements; the ordinary storage API retains its per-port semantics.
  ARM build/static analysis and complete Flash creation/write ASan/UBSan
  regressions pass, including selection, duplicates, read failures, no-space,
  interrupted writes and recovery. Planning rejects before changing its output.
  This follow-up is local, separate from the submitted batch below.

- **Accepted CI batch:** bounded configuration resolution, captured ROM-bank
  validation, native startup inventory, partial bank-header recovery coverage
  and the retained-RAM prerequisite below passed all four workflows at
  `b31a3a6b`: Quality `39462737479`, Core `39462737456`, Runtime `39462737482`
  and Objects `39462737472`. PR #4 merged that exact head into main at
  `97275d3a`; the active branch incorporated main without conflict. Main CI is
  not awaited. The later placement/preparation work remains local; no additional
  M8 milestone is complete.

- **Retained-RAM installation prerequisite:** reservation creation now checks
  that HOME's oldest item is the inline null-named hidden directory before any
  writes, then checks it again after native movement when verifying the created
  resource. Missing structure returns unsupported; unreadable, out-of-range or
  malformed metadata rejects without changing memory or the caller's output.
  The normal mutation preflight still validates the complete directory contents.
  Twenty-seven targeted rejection cases and the existing creation/movement/fault
  suite pass, including ASan/UBSan, pinned formatting and ARM static analysis.
  Native creation/publication and failure-retention checks pass on both models,
  preserving exact Flash/catalog, bound resource contents, owning CODE, guards
  and mappings. This is an installation precondition, not proof
  of durable ownership, arbitrary later RAM integrity or global startup order.

- **Active startup decision and existing counterexample:** the earlier native
  dependency fixture remains decisive: Port 0 library 768 can repeatedly enter
  its configuration while recalling damaged EQLIB, preventing recovery library
  769 from running. Research ID 4 repairs that case but is not a production
  allocation policy. The existing library-229 body-fault and explicit banked
  UserKeys-reference probes already cover those particular paths; repeating
  narrower intact-data probes does not close the remaining gate. The next
  prerequisite is to establish creation/GC safety during startup, then a
  supported installation entry before dependent callbacks. Cold-boot read
  watchpoints now prove both models enter the hidden-directory creation routine:
  the Saturn instruction pointer is `0xada5`, with the interpreter fetch returning
  to `0x288dc`. This distinguishes execution from the earlier ROM scanner's
  read of the same bytes. Follow-up SRAM snapshots taken after `SAVPTR` and
  `ROOM`, at Saturn `0xadb3`, establish the actual allocation-time gap on both
  models: 494,024 nibbles with intact Flash and 494,117 with the bound pending
  bank-header recovery. HOME is empty at that point. Both exceed the routine's
  20-nibble requirement, so this creation path skips its explicit GC branch in
  these cold fixtures. The damaged cold boots then restore exact Flash, close
  the transaction and rebuild the baseline catalog. Entry snapshots taken before
  pointer synchronization are not the decision evidence. Retained-RAM hidden
  directory validity, general movement/reference safety and a supported early
  installation entry remain required alongside durable ownership and reclamation.

- **Local torn-bank startup coverage:** both models complete low-memory automatic
  recovery for all sixteen `F0`–`FF` values reachable while programming the native
  bank-status byte, with the remaining bank identity bytes still erased. All
  32 cases restore exact Flash/catalog state, preserve 24,000 user-array elements,
  release the matching reservation and pass the maintained execution-image,
  owning-CODE, guard and reset-checkpoint oracle. The maintained fixture generator
  reproduces those native inputs byte-for-byte and rejects impossible status
  values or body-sector misuse. Existing low-memory E2E now exercises `F0`
  partial-header publication without another emulator invocation; the existing
  absent-reservation case retains fully erased-sector coverage. Sixty-one related
  Python tests and formatting pass. This bounds one scanner-facing fault family;
  other metadata/object tears, earlier callback access and installation lifetime
  remain open. Local verification is not CI or M8 acceptance.

- **Local pre-callback Flash access:** native read watchpoints on both models
  observe the original ARM library scanner reading physical `0x140100`, inside
  the installation fixture's `0x140000` target sector, during warm startup.
  ROM instructions identify the bank-status load in the scanner called at
  `0x2ed40`, immediately after native state initialization; subsequent Saturn
  reads also reach that metadata. Both observed runs return normally. This
  establishes an additional startup dependency, not a failure of an interrupted
  transaction: these probes use intact Flash. Merely moving the recovery
  configuration ahead of other library callbacks cannot establish safety.
  Acceptance must also cover scanner behavior on torn target metadata and
  recovery reachability before affected data is consumed. The four-byte
  sector-start probe saw no access; only the metadata watchpoint supplies
  positive evidence, and it is not a whole-sector access trace.

- **Local attachment/startup boundary:** on both supported hp4950 fixtures,
  ordinary `ATTACH` rejects experimental library ID 4, although Stock previously
  accepted its installation and configuration callback. This does not justify
  relaxing the production packager's user-library range. Repeated ordinary
  attachment of ID 768 and direct execution of the system library-2 callback do
  not run the observer user callbacks. The internal library-2 path can insert a
  duplicate HOME attachment; warm startup rebuilds the table, removing that
  duplicate and the manual ID 768 attachment whose configuration does not attach
  itself. Across two restarts on each model, the observed startup sequence and
  unrelated HOME/child payloads, exact catalog and Flash remain correct.
  Preserving the old attachment table is
  therefore not the right oracle. Earlier callbacks mutate HOME metadata;
  safe movement/reference handling and their possible access to pending Flash
  remain required before accepting this as a recovery entry.

- **Local earlier-callback evidence:** targeted warm-start probes on HP49g+
  and HP50g cover valid user-key bindings and an invalid hidden UserKeys CRC.
  The valid 739-nibble binding remains byte-for-byte intact; the invalid-CRC
  case resets it to an empty list. Neither case executes the assigned user
  program. Both preserve unrelated HOME objects and complete Flash contents,
  and reach the observed Port 2 / Port 0 / HOME startup sequence. These are
  representative native cases, not a complete proof of the preceding system
  callbacks or authorization to replay a pending transaction. HOME initialization,
  attachment behavior and production entry allocation remain open. This evidence
  is local only; no additional milestone is complete.

- **Local native startup inventory:** the existing installation witness now
  captures the actual ROM size directory and emulator bank bases, validates
  their geometry and resolves each catalog configuration entry on ARM. An
  independent host oracle compares every ID/header/bank/target row. Both models
  pass the original clean catalog (61 libraries / 16 callbacks) and the installed
  recovery fixture (62 / 17), preserving initialized code, owning CODE, stack
  guards, mappings, catalog and Flash. The existing missing/matched invocations
  carry this gate without additional emulator launches. Verifier corruption
  tests, ARM build/analysis and formatting pass. This is fixture-profile capture,
  not production firmware discovery or authorization of earlier callback behavior.
  No further M8 stage is complete.

- **Local ROM-layout validation:** the shared parser now derives bank extents
  from the captured KiB-size directory and compares every native base, including
  unused cells. It checks complete Flash coverage, window sizes, alignment and
  arithmetic bounds before publishing a selected bank. ASan/UBSan, pinned host
  analysis and ARM compilation pass. Both saved native states still resolve all
  62 library bindings using the actual first bank's 96 KiB bound, without a
  special-case address formula. Firmware-profile discovery and live capture
  integration remain separate requirements; this local work is not CI acceptance.
  Native ordering probes additionally confirm on both models, across two
  restarts, that an available Port 2 library ID 4 executes before a Port 0 ID 768
  callback and HOME `STARTUP`, despite reverse installation order. Exact catalog
  IDs and Flash are preserved. This is a candidate entry in a clean fixture,
  not an allocated production ID or proof that earlier system callbacks are safe.

- **Local startup prerequisite:** a bounded library-configuration resolver now
  distinguishes an absent callback from unreadable/mismatched metadata, preserves
  bank-local identity and never dereferences or executes the result. Read-failure,
  malformed-nibble and relative-pointer boundary tests pass under ASan/UBSan;
  pinned host analysis passes. Actual C against both native saved states resolves
  all 62 bindings using the observed emulator-state physical bank bases: 17
  callbacks (including recovery) and 45 absent callbacks. The ordinary physical
  Flash formula incorrectly mapped bank-table entry zero; its saved base is
  `0x48000`, not `0x40000`. This restores previously skipped libraries 221/223
  (both have callbacks) and 257 (none). Native watchpoints on both models confirm
  that Stock initializes the bank array from its ROM size directory and uses it
  to rebuild mappings and scan libraries. The directory describes a 96 KiB first
  bank and thirteen 128 KiB banks, ending exactly at 2 MiB. Production mapping
  capture and callback behavior/order analysis remain open; a low library number
  alone is insufficient.
  This follow-up is local and has not been submitted to CI.

- **Current automatic-binding follow-up:** owned ELF namespace/capacity fields
  are filled before sealing the library. The automatic predicate compares the
  complete HPCMEM2 binding after streaming installation/transaction preflight,
  before RAM release or replay. Both native models pass low-memory recovery and
  repeated restart with all 24,000 user array elements preserved and a measured
  20,902-byte free gap. Existing rejection cases pass. Separate native runs of
  the maintained new cases reject changed resource identity, location, transaction
  CRC and capacity with valid active Flash transactions, preserving the complete
  altered resource and user data, with no Flash write/reset; the completion
  marker proves callback execution. Both also recover with no reservation
  object present. There is no reduced memory-pressure requirement.
  All 83 related Python tests and ARM analysis pass. Durable ownership proof,
  installation/restart/orphan handling and global startup ordering remain open.
  Submitted at `26f312e5` (implementation `542d832d`, synchronized with main).
  CI inspected once on 2026-10-21: Quality `39450806498` passes; Core
  `39450806503`, Runtime `39450806511` and Objects `39450806502` are queued.
  This is not a completed M8 stage.

- **Earlier accepted delivery:** all four workflows at `96bcf1f5` pass: Quality
  (`39442340894`), Core (`39442341248`), Runtime (`39442340864`) and Objects
  (`39442340824`). PR #3 merged that exact submitted head into main as
  `b10699df`. Main CI is not awaited. The newer creation/resource-binding work
  was submitted together at `26f312e5` on the active branch.

- **Local installation/resource binding:** the installation witness now creates
  an `HPCMEM2` reservation header from its sealed installed identity, physical
  library/journal/authority coordinates, target, journal CRC and capacity.
  It verifies fresh creation before publication and never persists a RAM
  pointer. A read-only matcher checks exact native type/capacity and all header
  bytes. Host tests cover independent encoded fields, every header-nibble
  corruption/read failure, old transaction CRCs, changed identities and invalid
  input; integrated mutation tests and ASan/UBSan pass, as do 82 Python tests
  and ARM analysis. Both native models install the newly sealed fixture and
  pass creation/publication and retained failure, including the independently
  reconstructed header, full Flash/catalog/code/owner/guard checks and zero
  Flash writes on failed creation. Automatic admission is covered by the
  current follow-up above; durable creation/publication proof, restart/orphan handling and
  global startup ordering remain open; a matching header alone is not authority.

- **Local reservation creation follow-up:** a shared internal creation step
  replaces the fixture's direct allocator call. It rejects existing names,
  resolves HOME again after movement and verifies the complete stored string
  before exposing its address/capacity for subsequent binding. Every lost-write-
  acknowledgement cut, silent payload corruption and failed readback produce
  INCOMPLETE without publishing a resource result; malformed/preflight failures
  do not write. The integrated mutation tests, ASan/UBSan and ARM analysis pass.
  Initial native runs correctly rejected a lower-stack-guard overwrite.
  Creation and readback now have separate frames (120 and 312 bytes), keeping
  the directory-entry buffer out of the allocator's deeper call chain.
  Both native models now pass normal creation/publication and retained-
  failure checks with the existing 4 KiB stack, exact code/owning CODE and
  intact guards. The result describes
  current-invocation ownership only; durable
  installation/transaction identity, restart/orphan handling and startup order
  remain open. This follow-up is local, after submitted `96bcf1f5`.

- **Previous delivery checkpoint (2026-10-20):** `5aad911e` was submitted, with CI acceptance
  pending; Objects passes, Quality fails because the identity-vector header
  was generated only at build time, and Core/Runtime are still running/queued
  at the latest inspection. A local CMake fix generates the header during
  configuration and tracks its inputs for regeneration. Both affected sources
  pass pinned Clang analysis in a fresh, configure-only build directory, and
  their two CTests pass. `8fc3fcd4` is accepted and merged into main as `ff35ffe9`.
  Local follow-up through `c15a9231` integrates automatic sealed admission
  and checks create-only reservation collisions. Both native models pass
  recovery/restart, all maintained automatic rejection cases and both
  existing-content collision cases. A lost-RAM-write-acknowledgement prototype
  also passes on both models. Maintained failure integration at `2952a44b`
  now passes both models, including both existing-content collision cases;
  normal publication, warm restart, zero-write closed repeat and both
  unpublished cancellation cases now also pass on both models from the saved
  clean installed baseline. All 82 related Python tests, ARM analysis, formatting
  and strict docs pass. The follow-up through `a30ae484` was submitted as `96bcf1f5`;
  new CI is pending and will not be polled.
  Next: durable reservation
  ownership/lifetime and global startup order. No M8 acceptance gate is removed.
  The detailed evidence below includes earlier intermediate states.

- **Sealed installation identity batch:** the existing
  exact-library witness executes outside the library it verifies. An embedded
  admission command cannot embed its entire containing library as its own
  expected image. A packaging prototype now fills an owned `HPCROOT1` descriptor
  with the library span, descriptor coordinate and SHA-256 of canonical object
  nibbles, zeroing only the digest and native CRC fields before hashing, then
  rebuilding the native CRC. Tests cover descriptor alignment, odd object length,
  unchanged other fields, deterministic output and invalid inputs; all 76 related
  host tests and lint/format pass. Packaging now locates the optional descriptor
  through the owned admission ELF symbol and command layout, and seals it even
  when no layout sidecar is requested. Invalid symbol positions preserve all
  output files. A streaming C verifier checks the embedded descriptor, length,
  native CRC and canonical digest without heap allocation. Twenty generated
  reference vectors cover both nibble alignments, both length parities and SHA
  block/padding boundaries; every input-read failure and every content-nibble
  mutation with a repaired native CRC is rejected. CMake/CTest, ASan/UBSan,
  ARMv4T build/analysis and the related Python tests pass. ARM frame reports are
  192 bytes for the matcher and 168 for its hashing helper. A subsequent
  read-only installed-content matcher now resolves a unique live library and
  its catalog/bank switch before streaming the identity check. It reuses the
  complete six-bank allocation/tail scanner and shared catalog validation.
  All 20 vectors pass in all six banks; deleted/duplicate roots, wrong or
  ambiguous mappings, changed descriptors, read failure, dirty tails and a
  different valid library with the same ID reject without changing output or
  Flash. The new matcher, existing creation/authority regressions, ASan/UBSan
  and ARM analysis pass. A native prototype built from `791fcd34` now passes on
  HP49g+/HP50g: Stock installs the 14,896-byte sealed library, and a real XLIB
  call executes its internal admission command. The original content matches;
  changing only the library title and independently repairing the native CRC
  returns DIRTY. Both cases preserve exact Flash/catalog content, initialized
  execution code, its owning fresh CODE object, guards/mappings and normal
  native return, with zero Flash operations. The configuration is empty:
  this proves internal execution, not automatic startup admission.
  A local follow-up additionally validates allocation metadata and canonical
  or supported legacy envelope length; wrong-type and enlarged envelopes
  reject in host tests, with ARM analysis passing. Maintained native integration
  and admission against an interrupted transaction's restored view remain next.
  The subsequent local preflight now validates the bound transaction before
  checking installed content on the restored view, reads the exact native
  allocation extent and rejects any recovery allocation overlapping the target.
  Host tests accept ready/damaged, retiring and closed states, preserve later
  valid allocations after closure, and reject unpublished/corrupt records,
  wrong catalog mappings, read failures, a root supplied only by the journal
  and damaged current Flash after closure. Outputs and Flash remain unchanged
  on rejection. Host/sanitizer tests and ARM analysis pass; both native models
  have passed all five transaction-state probes (armed, damaged, retiring, closed
  and unpublished), including exact code/owner/guards/catalog/Flash checks.
  The current copied admission
  program occupies 26,914 bytes (26,832-byte ARM runtime extent), exceeding the
  earlier 20,898-byte low-memory gap; its containing library occupies 27,008 bytes.
  This is functional preflight evidence only: a smaller admission path and
  renewed physical-memory validation are required before startup integration.
  Streaming admission now avoids the sector-sized scratch buffer by validating
  the journal incrementally and reading the restored view directly from Flash.
  Differential host tests pass against the buffered path, including invalid
  closure order, a mismatched retiring target and a second pending journal.
  Record tests cover every status byte, byte corruption and read failure;
  focused CTest, ASan/UBSan and ARM static analysis pass. The native copied
  program is now 17,098 bytes (17,016-byte ARM runtime extent), with a
  17,192-byte installed library. Local commit `8311bb7d` passes armed,
  damaged and retiring native admission on both models. With only 20,860
  bytes free, both also pass damaged and closed admission, preserving all
  24,000 user real-array elements, the 32 KiB reservation, exact Flash/catalog,
  execution code/owning CODE, stack guards, mappings and native return.
  These are explicit internal XLIB calls, not automatic startup acceptance.
  All five normal-memory states now pass on both models. Record/authority
  regressions also pass. Maintained E2E integration adds an identity suite to
  the existing Runtime matrix, sharing its configuration build. It packages
  the sealed library from owned ELF coordinates, invokes its internal command,
  and checks transaction phases, changed content with repaired native CRC and
  low-memory preservation. ARM build/analysis, 78 related Python tests and
  lint pass. Native integration passes all five transaction states on both
  models. The new changed-content fixture initially validated ordinary native
  allocations after injecting pending journal records; it now validates the
  altered library before injection. The corrected content-mismatch case and
  both low-memory cases now pass on both models, including exact code/owner,
  guards, Flash/catalog, all user-array elements and reservation contents.
  The maintained copied admission program is 17,114 bytes; the measured free
  gap in the installation baseline is 20,860 bytes. The complete standalone
  clean-boot suite now passes on both models, including all five transaction
  states, changed content and both pressure cases; its measured gap is 20,868 bytes.
  A separate local prototype is testing sealed admission before automatic
  reservation release/recovery. Its initial 2 KiB startup stack corrupted the
  lower guard and adjacent code on both models. A 4 KiB variant matching the
  verified internal-command fixture now passes automatic recovery and repeated
  restart with preserved user data and exact Flash on both models. This remains
  a prototype; maintained automatic integration and negative cases remain open.
  Completion-marker libraries
  now reseal their owned identity after configuration changes; independent
  digest/native-CRC tests pass. These changes do not establish RAM ownership.
  The maintained identity batch was submitted as `5aad911e`; CI acceptance
  remains pending. A local automatic-admission follow-up binds the library
  number through an owned ELF placeholder before sealing, rather than baking
  a fixture ID into the admission code. The maintained automatic predicate now
  uses this bound number, sealed content and streaming transaction preflight
  with a 4 KiB stack. ARM build/analysis and 77 related Python tests pass; both
  models are running the complete automatic recovery and rejection fixture.
  Automatic low-memory recovery/restart and closed, unpublished, bad-CRC,
  bank-identity and dirty-tail rejection now pass on both models. An additional
  native startup case changes only the installed library title and repairs its
  native CRC; both models reject it without releasing RAM, writing Flash or
  requesting reset. The completion marker proves the callback ran, and all
  retained user data remain exact. Wrong-type, wrong-capacity and wrong-prefix
  reservation refusal also pass, completing the maintained automatic fixture
  on both models. A creation-side follow-up now checks that existing names
  are rejected even for byte-identical reservation content, without entering
  transaction publication; both content variants now pass on both native models.
  A local failure-path follow-up tracks native RAM write attempts during
  reservation creation and retains the invocation after a subsequent error,
  rather than returning an ordinary false predicate to Stock. A lost-write-
  acknowledgement prototype passes on both native models, preserving code/owner,
  guards and IRQ ownership without Flash publication. Maintained failure E2E
  integration and the normal publication/restart/cancellation regression now pass
  on both models. This retains failure ownership and does not implement rollback or restart claims.
  This content check does not establish installation provenance, fresh
  transaction ownership or RAM release permission.
- **Current cancellation batch (2026-10-20):** implementation and maintained
  tests through `8a717ec4` follow the previous submission `c8740410`.
  This batch passed Quality, Core, Runtime and Objects at `8fc3fcd4` and was
  merged into main through PR #2 (`ff35ffe9`). Main CI is not awaited.
  `747fa4ae`
  records the publication power-cut evidence below.
  Internal unpublished-transaction cancellation now passes host fault injection
  for four initial states: every program operation with five partial-write masks,
  lost acknowledgements, silent writes, sampled read failures and repeated calls.
  The oracle requires journal invalidation before authority writes and exact
  preservation of all bytes outside the two envelopes; armed and ordinarily
  retired records are rejected. ASan/UBSan and ARM static analysis pass.
  The same fault matrix now passes with separate banks and both adjacent record
  orders. Gaps, erased interiors of deleted allocations and a changed installed
  root are rejected without writes. Native HP49g+/HP50g research passes cancellation
  from both saved publication-cut states: eight writes after the journal-body cut
  and five after the authority-body cut, followed by zero-write repeated calls.
  Exact whole-Flash changes are limited to the two envelopes; catalog, initialized
  code/owning CODE object, guards/mappings, native return and the complete 32 KiB
  reservation pass. Native cuts during journal and authority cancellation also
  pass on both models: an independent NOR command oracle verifies the torn image,
  restart preserves it and the reservation, and retry completes in six/one writes
  respectively with zero-write repeated calls. These remain fresh-fixture claims.
  Maintained cancellation E2E now passes both generated interrupted states on
  both models in the existing installation suite, with the same exact Flash,
  execution, reservation and repeat checks. The seed records derive from the
  runtime-captured publication fixture rather than saved ROM snapshots. Fresh
  ARM packaging, 75 related host tests, lint/format and strict docs pass.
  Cancellation-time power-cut experiments remain separate research evidence.
  This is not public service delivery.
  This does not establish the still-required durable installation claim, RAM
  ownership, startup ordering or reclamation. Port 2 remains in M8.0; all other
  stage requirements and the accepted M8.1 status remain as listed below.
- **Publication power-cut research (local):** both native models now have
  independently verified cuts at program operations 2 (journal body) and 1,939
  (authority body), after 16 busy reads with completed-bit mask `0x5555`.
  An independent command sequence matches the complete torn Flash image. Warm
  restart preserves that image and the complete 32 KiB reservation. Subsequent
  USB execution of the maintained installed-transaction predicate rejects all
  four interrupted states as DIRTY, with valid code/owner/guards/mappings,
  unchanged Flash/catalog/reservation and zero writes. The fixture's installation
  has an empty configuration: these results prove rejection and native re-entry,
  not automatic recovery before other startup consumers or durable ownership.
- **Current implementation batch:** `85b57056` adds maintained native publication
  and completed-transaction restart tests to the existing installation suite,
  alongside initial planning (`2c7cb1c1`) and journal retry (`5d31e43a`). On both
  models, planning and create-only 32 KiB reservation creation precede journal
  and authority publication; each experiences one observed partial-bit program,
  then continuation, retirement and closure succeed. Each initial run performs
  1,957 real Flash operations. Restart preserves Flash and reservation; a repeated
  closed call performs zero operations. Exact Flash/catalog, initialized code and
  owning CODE object, stack guards/mappings and diagnostic results pass. The host
  oracle was corrected to expect the observed empty post-reset stack and rechecked
  against both saved native runs. Fresh ARM builds, 75 focused host tests, analysis
  and pinned lint/format pass. Publication uses a no-op desired target and retries
  within one invocation; mid-publication power-loss recovery and durable resource
  ownership remain open. CI acceptance is pending; no milestone is closed.
- **Previous submitted batch:** `b7b82d18` (implementation/tests through `0c263a97`)
  includes installed-transaction
  preflight, owned descriptor layout generation, one-shot fixture lifetime fixes
  and maintained native transaction E2E. Both models pass the full recovery suite
  and all five installed-transaction states locally; 75 focused host tests, ARM
  analysis, pinned formatting/lint and strict documentation pass. CI acceptance
  remains pending; `62f7ece4` remains the accepted baseline.

- **Journal publication retry (`5d31e43a`, included in current batch):** sector capsules now have an
  explicit continuation operation, sharing the existing authority-record
  publication validator. It checks the entire caller-owned original capsule,
  NOR-compatible partial body and erased end marker before writing; verifies the
  complete body before READY; and rejects every retiring/retired status. Fault
  injection covers every initial/resumed read and program cut, partial halfword
  bits, lost acknowledgements, silent programming failures and repeated calls.
  Target/unrelated bytes remain unchanged; authority regressions, ASan/UBSan,
  ARM analysis and pinned formatting pass. This requires the original trusted
  transaction identity/image and precedes any adjacent following allocation.
  Maintained publication and completed-restart evidence is recorded above;
  mid-publication power-loss recovery and durable RAM ownership remain open.
- **Initial transaction planning (`2c7cb1c1`, included in current batch):** the installed-layout
  planner validates the expected library/catalog on intact Flash before either
  transaction record is published. It derives the owned journal/authority pair,
  requires untouched bytes, validates their exact prospective allocation
  boundaries and the desired target image, and keeps the recovery library outside
  that target sector. Separate banks and either contiguous order work; tail holes,
  erased interiors of deleted allocations, occupied slots and malformed plans
  fail without output or Flash changes. Host tests, ASan/UBSan, ARM analysis and
  pinned formatting pass. Both native models also pass planning followed by
  create-only 32 KiB reservation creation and repeated-create rejection, with
  complete code/owner/guard/mapping/Flash/catalog checks. Durable reservation
  ownership and publication/interruption integration remain open.
- **Reservation creation prototype (local):** both native models pass exact
  installed-library/catalog matching before transaction publication, followed by
  creation of a 32 KiB HOME string through the existing native create-only API.
  Repeating the request returns EXISTS even for identical content; all 32 KiB
  remain unchanged. Complete initialized-code/owning-CODE checks, stack guards,
  restored mappings, Stock return, catalog preservation and zero Flash writes
  pass. This establishes reuse of the existing allocator, not durable reservation
  ownership, interruption handling or permission to mutate the target bank.
- **Previously submitted implementation:** `aaf23c02` includes the read-only native
  installation matcher, maintained two-model installation E2E (`66cdf5e5`) and
  unified recovery/admission ELF binding. Both native installation flows and full
  guarded recovery regressions pass locally. Quality, Runtime and Objects pass
  remotely, but Core's two full recovery jobs fail at retirement re-entry; both
  installation jobs pass. This batch is not fully CI-accepted.
- **Recovery fixture lifetime correction (`e7604b41`, submitted):** completed one-shot programs
  were retained in HOME across the full recovery chain. The failed HP50g state
  had only 1,334 bytes between TEMP and the data stack after restart; re-entering
  its menu reproduces screen corruption and USB timeout. One-shot witnesses now
  release their own HOME binding after RCL; the final reusable retirement
  program remains installed. Independent snapshot checks find the obsolete
  bindings absent, the reusable program retained and 203,636 bytes of free gap
  on both models. Both fresh native flows now pass the complete maintained
  recovery suite, including retirement/re-entry, automatic STARTUP, configuration
  recovery, transaction rejection and all reservation rejection cases.
- **Installed transaction preflight (`6953972e`, submitted):** a new read-only composition derives
  the pair from two matching HPCBIND1 descriptors at owned packaging coordinates,
  checks the bound transaction, and matches the complete installed library and
  catalog on the restored view. Its allocation must remain outside the target
  sector; a library supplied only by the journal is rejected. Closed transactions
  use actual Flash. Authority tests and ARM analysis pass; host execution against
  fourteen native snapshots accepts pending/closed/repeated states and rejects
  unpublished, corrupt, wrong-bank and dirty-tail states without output changes.
  Packaging `b44d8aec` generates descriptor coordinates through ELF symbols and
  command links; both fresh-copy and reusable layouts pass host tests, and actual
  ELF packaging preserves the tested library bytes. A native prototype passes
  armed/intact-target, closed and unpublished cases on both models, including
  complete initialized code and its owning CODE object, reconstructed expected
  library, stack guards, restored mappings, result and unchanged Flash/catalog.
  The damaged-target native prototype also passes on both models. Maintained
  installation E2E now includes armed, damaged-target, retiring, closed and
  unpublished transactions in the existing suite (`0c263a97`); both models pass
  all five states with unchanged Flash/catalog and zero Flash operations.
  Root publication, lifetime, RAM ownership, startup ordering and
  reclamation remain open.
- **CI accepted:** `62f7ece4`, including compact Flash recovery packaging and
  maintained low-memory/repeated-restart tests. All four triggered workflows pass;
  the preceding `9d015660` batch also passes all six triggered workflows.
- **Installation integration (`66cdf5e5`, included in submission):** native installation matching now has a
  maintained E2E suite, generated library fixture and shared ARM predicate entry.
  HP49g+ and HP50g both pass absent-library rejection and fresh Stock installation
  matching, with exact initialized code and enclosing CODE-object checks, stack
  guards, restored mappings, unchanged Flash/catalog and zero Flash operations.
  The Runtime workflow reuses its existing setup for separate recovery and
  installation matrix jobs. Default-build Clang analysis and workflow validation
  pass. Full low-memory recovery regression after the shared-entry extraction
  passes on HP50g/HP49g+ in 636/637 seconds: 20,898-byte initial gap, all 24,000
  user-array elements, 1,923 busy Flash operations, repeated restart, five
  transaction-admission rejection cases and three wrong-reservation cases.
  This integration has no CI acceptance yet.
- **Bound packaging (`aaf23c02`, included in submission):** the guarded-library CLI accepts an
  admission ELF and applies the same explicit journal/authority coordinates to
  both it and the recovery ELF. Both relocation/layout/binding checks finish
  before outputs are written; an unchecked prepacked predicate cannot be combined
  with an explicit recovery binding. The maintained reservation E2E uses this
  path. Actual ELF packaging reproduces both tested libraries on both models
  byte-for-byte. All 75 focused host tests pass, including different compiled
  defaults, shared rebinding and unchanged outputs after invalid admission input.
  Matching coordinates still do not establish allocation or RAM ownership.
- **Current scope:** M8.1 remains the only accepted complete M8 stage. M8.0
  installation identity, reservation/transaction ownership, safe startup,
  authority reuse and reclamation remain open, together with the retained Port 1
  and execution-reference/library-registration requirements. Sound compatibility
  remains M8.4 work, not a prerequisite for Port 2.

#### Earlier batch evidence

The following records preserve the evidence at each implementation step. Work
through `aaf23c02` is submitted, regardless of an earlier local-only checkpoint;
only `62f7ece4` has accepted CI evidence recorded above. Later matcher commits
`9e679dd5`, `f7fa0f9a` and their maintained-suite integration are included in that submission.

- **Previously submitted:** `eb81d433` includes guarded recovery packaging and maintained
  two-model E2E, both verified locally; remote acceptance is pending and the new
  CI has not been queried. Reservation ownership and safe startup admission are
  still unproven.
- **Local follow-up:** the installation witness now requires one live native
  recovery allocation with the complete packaged library, a matching catalog
  address/bank and no overlap with the recovery target sector. Deleted/interior
  copies, duplicate identities and changed payload/catalog data are rejected.
  Both models' saved guarded/observed installations pass; 64 focused host tests
  pass. This strengthens installation evidence without establishing transaction
  ownership, a production installer or startup ordering.
- **Locally verified, now included in `235d8ccf`:** the recovery witness accepts a versioned ELF
  binding descriptor, allowing the packager to set journal/authority addresses
  without recompiling ARM code. Descriptor/storage/overlap checks, validation of
  compiled defaults and 68 focused host tests pass. Fresh HP49g+/HP50g runs move
  the journal from `0x160105` to `0x180105` using the same ELF; only its packaged
  address fields differ. Both complete low-memory recovery, unchanged restart,
  all 24,000 user-array elements and all three reservation-rejection cases, with
  1,923 busy Flash operations each. This parameter path does not itself authorize
  those addresses or establish reservation ownership.
- **Startup admission research:** a small ARM predicate returns both real-valued
  outcomes correctly through the native library configuration on HP49g+/HP50g,
  preserving the empty stack, retained user string and complete Flash image.
  A follow-up using the existing authority/journal validators fits in an 11,946-byte
  configuration program. Both models pass armed, retiring, closing, closed,
  unpublished and corrupt-record decisions from a 20,935-byte free gap, preserving
  a 32 KiB string, all 24,000 user-array elements, the full catalog and Flash.
  Post-predicate ordinary arithmetic/variable-store controls also pass. The local
  maintained integration now uses the internal real-valued return helper, guarded
  stack and ROM-constant checks before RAM release. Its package is 15,722 bytes;
  71 focused host tests and the mutation/object suites pass. Initial native flows
  repaired Flash but lacked execution evidence after reset skipped the closed
  transaction. Local commit `a8f6e889` adds an opt-in pre-reset SRAM/CPU checkpoint
  and verifier support; the pinned emulator builds and the checkpoint unit test
  passes. Fresh native
  runs from both saved fault states pass pre-reset code/stack/TLS verification
  and final recovery checks, with 1,923 Flash operations each. The 15,722-byte
  maintained predicate passes complete fresh HP49g+/HP50g flows in approximately
  611 seconds/model: low-memory repair, repeated restart, matching-reservation
  rejection for closed/unpublished/corrupt records and all three wrong-object
  cases, preserving Flash, catalog and 24,000 user-array elements.
  Both original execution snapshots pass live TLS-cell checks, and 14 injected
  corruptions across CPU state, code, guards and TLS are rejected. The previous
  TLS check only validated recorded diagnostics and could miss later corruption;
  the verifier now checks the actual cells and exact errno address as well.
  Local follow-up `204b9572` extracts exact bound-allocation membership checks
  for use before reservation release. Authority/replay host suites and ARM Clang
  analysis pass, including nested records, wrong bank identity, dirty tail and
  failed reads. Connecting this check increases the ordinary reusable predicate
  to 25,002 bytes, above the low-memory fixture's gap. Its Flash-only fresh-copy
  form is 15,658 bytes. Integration commit `727b7464` passes the complete native
  flow on both models (609/612 seconds), including low-memory repair, repeated
  restart, closed/unpublished/corrupt-transaction rejection and all three
  wrong-object cases. Maintained packaging applies that lifetime only when embedding the hidden command in
  the immutable library. Separate native probes on both models also pass the
  bank-identity/dirty-tail rejection cases with zero Flash operations, no reset
  request and intact reservation, user array and catalog. A fresh ARM rebuild
  produces identical binaries; maintained ELF packaging produces byte-identical
  guarded and observed libraries to those used by the native runs.
  This follow-up batch is submitted as `235d8ccf`; no new CI acceptance or
  milestone completion is claimed. Structural membership does not establish installed
  identity, transaction freshness or RAM ownership; full recovery preflight and
  the remaining installation gates are retained.
- **Local installation follow-up:** a read-only native library matcher now
  reuses the existing full-port scanner. It validates the expected record,
  requires a unique live library ID and exact allocation header/length and
  library object, and publishes the physical slot only after every bank and
  erased tail passes. Deleted/nested copies, duplicates, changed valid libraries
  and later-bank damage are rejected without writes or output changes. Native
  STO's unused high padding nibble may be zero while the SDK encoder uses F;
  this non-object padding is excluded from identity, while the entire library
  and CRC remain checked. The extended creation/mutation suite and ARM Clang
  analysis pass. The host-compiled C matcher agrees with the independent
  package/catalog oracle on all four saved guarded/observed installations from
  HP49g+/HP50g. Those comparisons use saved-state evidence. A local combined installation
  check now also resolves the unique native catalog entry to that exact library
  header and compares its bank switch with the supplied ROM table contents.
  Tests cover all six user banks, absent/duplicate catalog entries, wrong headers,
  wrong or ambiguous bank switches and failed reads without output changes or
  writes. The host-compiled combined check passes all four saved installations
  and rejects eight changed header/switch cases; the extended mutation suite
  and ARM analysis pass. A private ARM probe now passes on both models for an
  absent library and a fresh Stock-installed library after reset. Independent
  checks verify the real-valued result, matched physical allocation, complete
  initialized code, stack guards, restored mappings, unchanged catalog and Flash,
  and zero Flash operations. Maintained E2E integration now passes both models
  locally as recorded above; CI acceptance remains pending.
  The read-only identity check is not a persistent installation trust root.
  Persistent installation lifetime, reservation/transaction ownership and recovery
  startup integration remain open; these follow-ups are not pushed.

#### Next M8.0 acceptance

Establish the trusted installation/root and bind the reservation to the current
transaction. The maintained guarded-reservation E2E is accepted at `62f7ece4`;
subsequent installation and recovery follow-ups retain the separate delivery
checkpoints above. Preserve the execution-reference
requirements while their scope clarification is pending. M8.1 remains the only
accepted complete M8 stage.

The next installation slice must connect the existing recovery pieces rather
than treat fixture addresses as installed state. Its acceptance requires:

1. Recover the exact authority/journal pair from the installed recovery identity,
   with allocation/lifetime checks; a scanned signature or patched address alone
   is not authorization.
2. Bind the reserved RAM's identity and capacity to that installation and active
   transaction before destructive writes, preserving unrelated HOME objects.
3. Reject stale, conflicting or partially published installation information
   without releasing unrelated RAM or mutating the target bank; accept retry only
   for the same owned transaction.
4. Exercise installation, interruption and recovery on both native Stock models,
   including complete Flash/catalog and user-data checks. Keep global startup
   ordering, safe authority reuse and reclamation as explicit remaining gates
   until their own acceptance is demonstrated.

The installation and recovery admission phases must remain distinct. Initial
reservation creation precedes authority publication, so it uses exact installed
library/catalog matching on intact Flash rather than requiring an already armed
transaction. A new reservation must use create-only semantics: even an existing
object with identical contents is a conflict, not evidence that this invocation
created it. Bind the resulting resource to durable installation/transaction
identity before authorizing target writes. Interrupted creation/publication must
have explicit retry and orphan handling; matching a fixed name, size and prefix
alone cannot establish that ownership. Preserve the existing recovery-time
checks and the RAM-loss recovery requirement.

If the original desired capsule is unavailable after power loss, compatible
partial bytes cannot reconstruct it. The unpublished-transaction path therefore
also needs cancellation under a trusted one-use installation claim: invalidate
the journal before disposing of the authority, preserve the target and unrelated
allocations, and make interruption of cancellation retryable without rearming a
journal. An already armed transaction must follow its recovery lifecycle instead.
Reservation release still requires proven ownership and completed cleanup.

The entries below preserve earlier batch evidence and check times; their older
pending-CI descriptions do not supersede these delivery checkpoints.

The accepted `d94580b4` batch was followed by `9d015660`, whose Quality, Core,
Objects, Storage, Font Gallery and Runtime workflows all passed. The accepted
baseline `62f7ece4` additionally passes Quality, Core, Objects and Runtime,
including both Stock recovery jobs. These batches add conditional SETUP dispatch
evidence, native bank-switch execution roots, explicit execution-address repair
planning, merged catalog repairs, direct-jump return-root discovery and compact
recovery packaging. They do not close library registration or the other retained
M8.0 gates.

Previous submitted batch `4b34833d`: Core, Objects, Storage and Font Gallery passed.
Quality failed because the code-relocation fixture header was unavailable after
configuration; Runtime failed at PWM4 lease admission in all six Stock System
Services jobs. Local fix `6668fc13` generates the fixture at configuration time
and tests occupied-timer rejection with EBUSY and zero register writes, retaining
actual IRQ delivery on an idle timer. Clean-config Clang analysis, all six Stock
System Services targets and HP50g Direct pass, including native return and
independent input pixels. The corrected batch is CI-accepted at `d94580b4`. This does not establish
a Stock asynchronous-audio backend. Earlier HP39 reject-return failures did not
recur in the submitted batch.

The follow-up batch includes the register-transfer and instruction-walk work
through `5dfeca33`, the CI fixture fixes in `6668fc13`, and independent catalog
switch-function relocation in `4a497fd5`. Evidence is recorded below; the four
triggered CI workflows pass at `d94580b4`, without closing an entire M8 stage.

| Stage | Current state | Remaining acceptance |
| --- | --- | --- |
| M8.0 | Native port reads and substantial mutation support exist. Port 2 backup create/replace/remove and library creation have native evidence. | Complete library registration/attachment and execution-reference safety; Port 1 replacement/removal; trusted Port 2 recovery installation and authority lifecycle, safe startup ordering, reclamation and retained per-port gates. |
| M8.1 | Installed source SDK and six-model packaging accepted. | Preserve relocation/build/package regressions as later features are integrated. |
| M8.2 | Declaration, layout, input, effects and canvas implemented; integrated UI CI accepted. | Public application/API review and remaining smallest-target resource/update-cost criteria. |
| M8.3 | Components, themes and renderer implemented; all-six Direct/Stock local memory-report and low-memory checks pass. | Whole-application physical-memory admission and remaining resource/CI acceptance; touched-stack watermarks alone are insufficient. |
| M8.4 | Multiple graphics, keyboard, console and system adapter groups exist. Fixed-array line-input aliases pass all-six Direct/Stock locally. | Complete the retained historical API/application ledger, asynchronous KOS sound and decimal-format policy, plus remaining adapter groups and CI acceptance. |
| M8.5 | Initial C++ UI/object wrappers accepted; additional owners verified locally. | Broader ownership/migration guidance, examples and pending integration acceptance. |
| M8.6 | Integrated examples and relocated packaging have substantial local evidence. | Complete downstream scenarios, resource/error/return and release acceptance; retain the independent-launcher evaluation without presuming retirement. |

The next implementation track is M8.0's existing native-port work: establish
complete execution-reference coverage for safe movement/registration, then
finish the trusted recovery installation/authority and startup requirements
needed for reclamation. Those requirements must be satisfied together before
public reclamation is enabled. Asynchronous sound is an M8.4 obligation, not a
dependency of Port 2. Its verified timer/IRQ foundations remain available for
later continuation. UI/resource and other adapter work may proceed independently
when the native-port track has a concrete external wait; this does not silently
replace that track or close its remaining requirements.

Port 2 remains part of M8.0. Native backup writes and library creation are
partial delivery, not acceptance of the complete service. The remaining work
is tracked as the following dependent acceptance steps:

1. Establish the retained relocation contract and safe public library
   registration/attachment; preserve Port 1 replacement/removal requirements.
2. Install and identify a trusted recovery root independently of test-fixture
   addresses. Validate package layout and live resource admission, not merely
   whether encoded lengths fit their fields.
3. Establish authority publication, closure and reuse across interrupted writes
   and erases. A surviving old journal must never regain replay authority.
4. Establish recovery ordering before any startup consumer can access an
   interrupted target bank; a low library ID alone is insufficient evidence.
5. Integrate reclamation and verify interruption/restart, persistence and normal
   Stock return before enabling the public operation.

While the execution-safety scope question remains unanswered, installation
resource admission and recovery lifecycle work may proceed within M8.0 without
relaxing relocation checks. Sound compatibility remains a separate M8.4 task.

Recovery packaging now rejects complete programs larger than the supported
256 KiB Stock main-RAM window, counting both initialized and runtime copies plus
RPL framing. Boundary tests cover both target models and small/maximum initialized
images. This is a necessary physical bound, not proof of available installation
memory, Flash-bank fit or trusted installation; those acceptance steps remain open.

Local retained-RAM fault experiments now expose a concrete recovery resource gap:
on HP49g+ and HP50g, a verified 26,000-element real array leaves 37,823 bytes between
the RPL stacks, and the existing 46,698-byte bound recovery package does not repair
the injected erased sector during the observed startup. Flash and both armed
transaction statuses remain unchanged. Wider-memory controls recover. Installation
must guarantee recovery-time storage before destructive mutation; a fresh-state
success or installation-time free-memory check alone is insufficient. This is local
negative evidence, not a completed installer or a relaxation of any startup gate.

A local compact Flash-configuration prototype now passes that same low-memory
fault on both models, reducing the package from 46,698 to 29,682 bytes. Independent
verification preserves all 26,000 real elements and checks the complete Flash
transition, catalog, image, stack/TLS and guards. Both models also pass retained-RAM,
RAM-loss, callback-body and repeated-warmstart checks. Maintained packaging now
provides explicit `--flash-copy` configuration libraries and a companion program
for verification, while ordinary reusable RAM programs retain their immutable
source. Host tests cover fresh/reusable lifetimes, all alignments, CLI restrictions,
distinct outputs and every real-array metadata/data nibble. The maintained native
configuration sequence now includes low-memory recovery and repeated startup;
the expanded local run passes on both HP49g+ and HP50g, including native catalog,
complete Flash, code/stack/TLS/guard and full real-array checks. All 49 focused
host tests, a fresh ARM build, pinned Python lint/format and strict docs pass.
This batch is CI-accepted at `62f7ece4`. Trusted
installation, recovery-memory guarantees and all other M8.0 gates remain open.

Local guarded reservation release now passes on both models: one Flash library's
configuration checks a controlled 32 KiB string's type, capacity and expected
header before releasing it and invoking its hidden recovery command. Real
literals and `SAME` avoid the failing experimental integer/numeric-comparison
path during startup. With only 20,898 bytes initially free, recovery and the
subsequent unchanged restart preserve all 24,000 user real-array elements and
pass complete Flash/catalog/image/stack/TLS checks. Normal startup without a
reservation also passes. An HP50g control without releasable storage remains
unrecovered at comparable pressure.

The internal packager now generates this guarded library directly, with six
hidden commands and no calculator-side compiler. Its generated package passes
the two-model positive sequence above. Instrumented rejection checks preserve
wrong-type, wrong-capacity and wrong-header objects with unchanged Flash on both
models; a completion marker is cleared before each case and recreated after the
unchanged guard command bodies run. All 54 focused recovery host tests, pinned
Python checks and the real ELF CLI packaging path pass locally. The maintained
bound-configuration E2E flow now includes these scenarios in the existing Stock
recovery jobs and preserves generated library packages as evidence. Both models
pass the maintained flow locally with 64 synthetic busy reads per operation:
1,923 recovery operations and 123,072 status reads, with no active operation at
return. Each added model run takes approximately 489 seconds. Stock's fixture
installation retains its existing non-injected setup; SDK recovery and all
subsequent checks use the configured busy model. Local validation is complete;
remote acceptance of this follow-up is pending.

This is a controlled resource mechanism, not a production installer or a proven
general explanation of the earlier startup failures. Admission must bind the
reservation to the trusted installation
root/current transaction and establish safe global startup ordering before
destructive work. A fixed fixture name or matching header alone is not ownership
proof. All retained installation, authority-lifecycle and reclamation gates remain.

The recovery verifier now rejects ARM abort/undefined processor modes even when
the saved PC lies inside ROM. Both captured failed-startup states are rejected;
all 22 previously passing configuration-state snapshots across HP49g+/HP50g
still pass this processor-state check. The 34 focused verifier tests and pinned
Python checks pass. This strengthens one return check, not full startup proof.

Catalog-growth integration requires unified preflight and repair ordering.
Native root scanning and the separate catalog/reference planners can
describe the same bank-switch field; their repairs must not be applied twice.
All geometry, mapping, reference and instruction guards must be checked before
the first write. Acceptance requires overlapping-root deduplication, conflicting
repair rejection, unchanged memory on preflight failure, and native continuation
after movement. ARM-held root cells retain their separate identity. These are
integration requirements, not evidence that complete opaque/live-code coverage
has already been established; that coverage remains required before public
registration can be enabled.

Catalog growth now snapshots native repairs and merges additional main-RAM
object/code edits before writing. Duplicate repairs coalesce by field or ARM
cell identity; conflicting repairs, partial overlaps, split fields and changed
guards reject before mutation. Geometry is published after repairs and movement.
Focused sanitizer tests cover all five edit widths, duplicate native/caller
discovery and sixteen rejection cases with unchanged memory/ARM roots. Existing
object, port and reference regressions pass, as do ARM analysis and six-launcher
builds. Controlled HP49g+/HP50g Stock probes require actual duplicate discoveries,
merge them with native repairs, grow the catalog and reach the independent RPL
continuation marker. Both preserve the sorted unique catalog and unchanged Flash.
Their opaque-code coverage remains a fixture limitation. This is local work,
not CI acceptance or complete execution-reference coverage; the next task remains
classification of unresolved live execution references before public registration.

Catalog commit failures now return a distinct `INCOMPLETE` result, preventing
confusion with preflight rejection. Host ASan/UBSan failure injection covers the
first write, movement, every geometry-publication field and a read failure after
writing starts. It also verifies allocation failure leaves memory/ARM roots
unchanged and that temporary plan storage is released on success, preflight
rejection and incomplete commit. ARM static analysis passes. These checks do not
provide rollback, replay or missing execution-reference classification.

Stage return-root discovery now follows direct GOTO/GOLONG/GOVLNG paths to
unconditional returns, bounded to 64 instructions and eight consumed cells.
Conditional/call/register-mediated paths still stop discovery. Sanitizers cover
signed relative ranges, mixed jump/return chains, all stack cursors, cycles,
instruction faults and boundaries. Catalog-movement integration verifies relative
offsets, absolute operand repair and preservation of unconsumed cells. All six
launchers build; controlled HP49g+/HP50g catalog-growth/native-return regressions
pass with unchanged Flash. A read-only snapshot check finds one pure
jump-to-return path among 47 direct-jump sites per model, in SETUP CODE; that
establishes an existing code pattern, not its liveness in every caller. ARM
analysis passes. This local expansion does not close full execution coverage.

Historical-source review distinguishes enumerated native roots from arbitrary
addresses embedded in machine code: HPGCC3's GC helpers repair the former, and
HP49 assembly documentation warns against collecting while executing from a
movable code region, describing a SysRPL restart boundary instead. Clarification
is pending on whether M8 acceptance follows that native relocation contract while
retaining broader hidden-address analysis as future work. No scope reduction or
preflight relaxation has been applied; all current gates remain open as above.

Before each work session, read this roadmap and issue #1, reconcile their current
state with implementation and verification evidence, and identify the selected
stage, dependency and completion condition. After verified progress, commits,
CI acceptance or a discovered discrepancy, update both records. Keep one current
board summary and separate historical snapshots; distinguish local, submitted and
accepted evidence. Preserve every retained goal and approved boundary, and seek
confirmation before reducing scope. A demonstrated completion status needs no
additional approval.

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

The current backlog is summarized above and in the retained checklist after the
history. In particular, Port 2 backup writes and library creation already exist;
early statements that all Port 2 writes are unfinished describe earlier states.

The HP49g+/HP50g stage now follows `RTN`, `RTNSC` and `RTNCC`, including direct
unconditional jumps to those returns, retaining up to eight distinct consumed
machine-stack cells as execution roots within 64 instructions. Each next cell
requires reaching another proven return; unreadable or unsupported instructions
stop discovery.
Host sanitizer tests cover all cursor positions, one to eight returns, opcode
boundaries, equal-valued unconsumed cells and failed preflight. Catalog-growth
integration independently consumes the relocated instruction/stack chain to its
marker. All six launchers build; the existing HP49g+/HP50g native catalog-growth
and post-call-marker regressions pass with unchanged Flash. Controlled native
fixtures also pass a three-return chain on HP50g and an eight-return chain on
HP49g+: a test-only stage inserts additional frames targeting the existing
return instruction before discovery, then the original ROM consumes the
relocated chain and reaches the independent RPL marker. This is synthesized
native-frame evidence, not proof of arbitrary nested callers. An HP50g control
using the old single-root selector rejects the missing roots before movement,
preserves the catalog and returns normally. Other instruction paths, opaque
contents and complete execution-reference coverage remain open; public
reclamation is not enabled.

Ownership tracing of the captured HP49g+/HP50g registration states identifies
ten remaining opaque intervals per model: four temporary library-command CODE
copies, three HOME CODE objects, one short HOME binary object, a launcher
descriptor and a system workspace. Native catalog and bank-table resolution
maps all four cached programs to library 275 commands 2, 3 and 4 (command 3
appears twice); complete cached-program matching and CODE-body comparison pass
on both captures. This establishes provenance, not relocation safety: source
identity does not classify embedded addresses, live continuations or workspace
contents. The next step is to establish those semantics and the remaining
execution paths before relaxing mutation preflight. These are local snapshot
results, not CI acceptance or a completed port-service gate. The three HOME
CODE objects belong to the stored `SETUP` program; the short binary object is
`UserKeys.CRC` in the hidden directory. `SETUP` includes relative calls between
separate CODE objects, so validating each CODE interval in isolation would reject
valid native code. Relocation analysis must preserve the enclosing program's
relationships as well as classify live references. Instruction-boundary checks
use Saturn+ decoding: older Saturn-only output misdecodes extended operations
and cannot establish these call paths.

An internal relative-branch inspector and relocation planner now cover proven
GOC/GONC, GOTO/GOSUB, GOLONG/GOSUBL, absolute GOVLNG/GOSBVL and GOYES predicate boundaries, including
cross-CODE targets.
They preserve operand widths and qualified mappings, rejecting split
instructions, unencodable distances and branch-to-return conversions. Exhaustive
displacement tests pass with ASan/UBSan, ARM compilation/analysis and pinned
formatting. Host checks against each HP49g+/HP50g snapshot match 211 native
branch sites; 408 proposed rewrites per model independently decode to the
intended target, while 14 invalid transitions are rejected. Shared reference
edits now carry explicit 1–5-nibble widths; the existing builder, validation and
retryable RAM replay handle mixed widths without overwriting adjacent bytes.
All 25 width pairs pass read/write-failure injection and retry tests, alongside
overlap checks and branch-plan/guard/replay integration. These are snapshot and
host checks, not native execution or CI acceptance. Complete instruction and
live-reference coverage plus production integration remain open; opaque
preflight is not bypassed.

Controlled HP49g+/HP50g Stock programs now execute 38 relative-branch cases per
model: the host C planner and shared replay generate repaired operands; each
repaired case reaches its success marker and its unrepaired control reaches a
distinct failure marker. Both models reach an independent final RPL marker with
unchanged Flash. This validates emitted instruction behavior, not in-calculator
relocation planning or complete library movement. Absolute-transfer host tests
cover qualified target movement, and 51 native ROM-call sites per snapshot
retain their targets when only their containing code moves. Production
integration and the remaining live-reference/opaque coverage are still open.

The same 38-case relative-branch matrix also passes with planning and replay
performed by the actual ARM application on both Stock models. The application
locates its stored test program, validates complete instruction guards, merges
19 operand repairs and applies the shared RAM replay before returning. Native
execution distinguishes repaired cases from unrepaired controls and reaches
the final RPL marker; Flash stays unchanged. All six launchers compile. These
controlled target-placement probes establish ARM-to-Stock execution handoff,
not arbitrary library movement or complete reference classification. The maintained
`code-relocation` Runtime suite now reproduces this probe through the standard
Stock installation flow and a required native-result verifier. Fresh local runs
pass on HP49g+ and HP50g, including all 38 cases, the final RPL marker and unchanged
Flash. CI integration is implemented; remote CI acceptance remains pending.

The maintained suite additionally passes 38 source-movement cases per model
(76 total). ARM repairs the original operand, copies the instruction 32 nibbles
forward, verifies the copy and clears its old bytes. Stock enters the copied
instruction and reaches the fixed success target; unrepaired controls reach a
distinct failure target. Both HP49g+ and HP50g pass all cases, the final RPL marker
and unchanged Flash locally. This establishes actual instruction movement within
controlled CODE fixtures; it does not establish whole-library movement, live
register/indirect-reference classification or remote CI acceptance.

Full five-nibble D0/D1 immediate data addresses now share qualified relocation
planning with absolute transfers. Partial loads and computed/register literals
remain unsupported without data-flow evidence. Exhaustive 20-bit target tests,
mapping separation, negative shifts, split-instruction refusal, read faults and
malformed inputs pass ASan/UBSan; ARM analysis and compilation pass. Both native
snapshots match 15 complete data-address operands. The maintained suite adds four
data-movement cases: ARM moves the data and poisons its old location; repaired
D0/D1 loads read the moved value while unrepaired controls read the poison.
HP49g+ and HP50g each pass all 80 cases, the final RPL marker and unchanged Flash
locally. Production integration, complete live/indirect-reference coverage and
remote acceptance remain open.

An internal register-transfer resolver now distinguishes `PC=A` / `PC=C` from
`PC=(A)` / `PC=(C)`. It consumes only the selected register's low 20 bits and
uses a separately supplied mapping reader for the indirect pointer cell; it
does not infer register provenance from an earlier snapshot. All 65,536
four-nibble encodings, separate code/data mappings, malformed reads, faults and
address boundaries pass host tests, with ARM compilation/analysis. Local native
watchpoint runs on HP49g+ and HP50g trace the normal post-ARMSYS return through
GETPTR and the subsequent RPL dispatch. A is loaded from the restored runstream
before `PC=(A)`; the observed pointer cell is the ROM SEMI entry, and the host
resolver agrees with the next observed native address. The 46 preceding
register accesses contain no R0–R4 use. Both runs preserve normal application
and Stock return checks. This is bounded normal-path evidence and host
resolution of native observations, not complete exceptional-path or register
liveness coverage; production opaque checks remain unchanged.

The maintained Stock suite now exercises those four register-transfer forms
through the actual ARM resolver and shared qualified reference planning/replay.
Eight new cases use explicit register-source cells and, for PC=(A/C), a separate
guarded pointer cell. Repaired and unrepaired controls reach distinct native
markers. Both HP49g+ and HP50g pass all 88 cases, the final RPL marker and unchanged
Flash locally. These controlled sources do not prove arbitrary live-register
provenance. Separately, error-return watchpoints are now gated on entry into the
specific ARM application, avoiding a matching numeric error code during earlier
Stock allocation. Both models copy the injected error code, restore RPL pointers
through GETPTR and reach the outer RPL error-handler marker after catalog growth,
with unchanged Flash. The first 400 observed register accesses contain no R0–R4
use; later consumers and other exceptional paths remain unclassified. Remote
acceptance and complete production reference coverage are still pending.

The internal instruction framer now walks an independently established
instruction-only interval, including variable-length literals and combined
predicate/GOYES operands. It supports documented Saturn and selected Saturn+
encodings, rejecting unknown extensions rather than treating `80B` as a short
classic BUSCC. Truncation, invalid nibbles, read faults, maximum-length literals,
callback failure and 20-bit bounds pass focused ASan/UBSan tests; ARM compilation
and analysis pass. In the HP50g native snapshot, all 1,229 boundaries across three
distinct cached library commands and the three SETUP CODE bodies match independent
disassembly. This is local framing evidence, not a proof of instruction semantics,
inline-data ownership, complete references or safe arbitrary program movement.
Opcode references are the [Saturn instruction reference](https://literature.hpcalc.org/community/intro-saturn-asm.pdf)
and [Rechlin's Saturn+ opcode observations](https://www.hpcalc.org/hp49/docs/programming/newopcodes.zip).

The maintained Stock relocation suite now walks each generator-established
instruction-only prefix on ARM and verifies the proposed repair site's boundary
before planning. HP49g+ and HP50g both pass all 88 cases, the final RPL marker and
unchanged Flash. A separate host walk of each model's native snapshot covers all
seven known CODE objects (including the duplicated command cache): 1,616
instructions and 346 direct branches. All 283 internal targets land on decoded
boundaries; the other 63 target the lower address region outside these RAM CODE
bodies. Qualified planning leaves all 346 operands unchanged for the observed
16-nibble catalog expansion. Six register-mediated transfer sites remain explicit
data-flow obligations; this result does not resolve live-register provenance,
other embedded data references or complete relocation safety.

Following the register-mediated calls also identifies library bank-switch fields
as executable targets, rather than inert catalog metadata. The internal catalog
planner now resolves payload and switch-function mappings separately and plans
both address fields. Equal numeric addresses in different mappings remain
independent; the zero/no-switch sentinel stays zero, and moving a nonzero function
to zero is rejected. Focused ASan/UBSan, ARM compilation and analysis pass for
independent mapping combinations and positive/negative moves. The two observed
Stock catalogs each contain 62 entries whose nonzero switch targets are below
the moving RAM region; that observation does not justify assuming all future
switch functions are immovable. Full register provenance and registration
integration remain open.

Post-submission source analysis bounds SETUP's observed `PC=A` dispatch block:
under HEX mode and entry at the block start, the destination is
`0x26bf1 + 7 * B.S`. Symbolic checks on both snapshots cover all 256 selector/P
combinations, keep unrelated initial register nibbles unknown, reject any unknown
input consumption and verify P restoration. All 16 targets are below moving RAM.
This is a conditional block-level result, not native execution of every selector
or proof that every possible entry reaches the block under those conditions.

The switch-function distinction now reaches native root enumeration and the
catalog geometry primitive. Nonzero catalog switch pointers have their own
execution-root provenance; RAM function pointers follow a move while banked
payload addresses remain in their selected mapping. ACPTR traversal also emits
the switch field as an unresolved execution reference, including compact and
nested array bodies. Focused sanitizer tests cover catalog movement, preflight
read failure without writes, root dependencies, zero sentinels, linked aliases
and callback failure. On both before/after native snapshots, all 56 nonzero
switch-function roots match their catalog fields; existing display-root
relocation checks still pass. Both HP49g+ and HP50g pass the controlled native
catalog-growth regression, post-call RPL marker and unchanged Flash; all six
launchers build. These fixtures retain their explicit bounded-probe scope:
complete execution coverage and public registration remain open.

An explicit execution-address planner now connects classified EXECUTION fields
to qualified relocation without weakening the ordinary planner's rejection.
Catalog switch fields and the maintained ARM register-transfer fixtures use it.
Mapping isolation, positive/negative shifts, split fields, invalid event metadata,
zero-sentinel rejection and unchanged outputs on errors pass focused sanitizers
and ARM analysis. Both HP49g+ and HP50g pass all 88 native relocation/control
cases, the post-call RPL marker and unchanged Flash. This closes address-field
planning for these classified cases, not code-body or live-register coverage.

<details>
<summary>Native-port implementation history and bounded verification evidence</summary>

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

The next native HP50g probe calls compiled freestanding C using an object-owned
4 KiB stack and a 536-byte workspace, with the original ARM context restored
before returning to Saturn. Independent state checks verify all 128 computed
workspace values and their checksum, the C local-array and stack addresses,
eight-byte stack alignment, both stack guards, unchanged code and object-owned
placement. Stock then pushes the expected integer 124275; library 275 remains
absent and Flash unchanged. This validates a C-call boundary for the probe;
actual journal discovery/replay, sufficient recovery stack depth, cache reuse
and cross-model behavior are still unaccepted.

The independent HP50g entry now also executes the actual ARM-compiled sector
discovery/preflight modules against native Flash after the damaged-bank restart.
It discovers the retained journal at `0x160105`, validates the restored whole-port
overlay for target sector `0x140000`, and copies the exact durable journal into
its owned workspace. A separate fixture with a corrupted journal CRC returns
DIRTY without publishing output coordinates. Both fixtures preserve all Flash
bytes, stack guards and code, and return to Stock with integer 124275 while
library 275 is absent. These are calculator-side read-only recovery checks;
Flash command ownership, erase/program replay through this entry, repeated
interruption recovery and production packaging are not yet accepted. The 4 KiB
probe stack passes these fixtures but still needs worst-case nested-object
depth acceptance before use as a recovery stack contract.

The independent entry now uses the existing supervisor IRQ bridge and Flash
mapping exchange instead of relying on direct CPSR writes. Native HP50g
observations confirm user-mode entry (`0x80000010`), masked execution
(`0x800000d0`) and restored caller control (`0x80000010`). Whole-port preflight
still passes while the two Flash sections are uncached and writable; both L1
descriptors and the Stock SWI vector are restored exactly, guards/code remain
intact and Flash unchanged. Actual writes remain gated on the recovery runtime:
linking the shared Timer0/RTC clock also requires its libc TLS/errno environment,
which the minimal freestanding probe does not yet initialize.

A subsequent native HP50g probe includes object-owned static TLS and the actual
shared Timer0/RTC clock modules. Its packaged extent now includes initialized
TLS bytes and zero-filled TLS storage; an error-path test exposed and corrected
an earlier probe packaging omission before any Flash writes were attempted.
Native checks verify the TLS initial value and mutation, EINVAL from a null
time-read output, the errno address inside the owned image, and a clock advance
of at least one millisecond while IRQ/FIQ remain masked. Whole-port preflight,
exact mapping/vector/control restoration, guards and Stock return still pass,
with library 275 absent and Flash unchanged. This supplies the timing dependency
for the next independent SST command/replay experiment; it does not establish
general SDK TLS loading, actual Flash replay or production recovery acceptance.

Calculator-side sector replay now passes in the isolated native HP50g fixture.
The independent entry uses the existing SST identification/command engine and
sector-replay module: one erase and 2,048 halfword programs restore the journal's
target sector exactly, with no other Flash changes and the journal retained.
An immediate second replay performs no erase/program operations. TLS/clock,
owned stack guards/code, mapping/vector/control restoration and the native
return integer all pass. A subsequent Stock ON/F3 rebuilds both library 124/275
bindings at their relocated addresses while preserving the recovered Flash and
journal. The first command probe exposed non-relocated GOT callback addresses;
local position-independent callbacks and a packaging relocation check correct
that probe defect. Interrupted calculator-side replay, startup access during
partially restored headers, journal retirement after reference acceptance,
worst-case stack/cache behavior and production packaging remain open. This is
an actual native command-path result, not automatic recovery or public Port 2
reclamation acceptance.

Native interruption acceptance now covers cuts after 128 and 132 completed SST
program callbacks, including an erased bank identity and a partially restored
first allocation header. Each interrupted replay reports DIRTY and stays in
owned ARM execution with guards intact; exact Flash comparison confirms only
the expected programmed prefix and an unchanged journal. Emulator hardware
reset preserves that partial image. Stock USB then accepts a separately named
recovery program, which discovers the journal, restores the target sector and
returns normally without library 275; repeated replay performs no writes. The
first retry accidentally selected the old fault-injection program because Stock
renamed a same-name upload; using a distinct recovery variable fixes the fixture.
These two cases establish command-boundary interruption/re-entry, not power loss
while the Flash chip is busy, all partial-header patterns, automatic startup
recovery or a completed public reclamation service.

A repeated-interruption chain now passes: after the 132-program cut and reset,
another native replay stops after 2,047 programs; a second hardware reset and
independent retry recover the exact image. Before final recovery, Stock retains
library 124/275 IDs with abnormal `0x1b08` bindings. After recovery and ON/F3,
both return to their expected relocated bindings without further Flash changes.
The final transfer first required native cleanup of the two retained fault-probe
programs after Stock rejected a third large probe for insufficient RAM. Production
packaging therefore still needs a bounded memory contract. The abnormal catalog
state led to delaying restored bank-identity publication until sector-body
verification, implemented and accepted below. Existing x50ng Flash operations complete immediately; native
command-boundary cuts do not establish busy-chip power-loss behavior.

Sector replay now leaves the first sector's bank-identity word erased until
every other word has been independently verified, then publishes and verifies
the identity last. Ordinary sectors retain their existing behavior. Exhaustive
read/program/erase-cut retries, silent programming failure, non-header-sector
coverage, ASan/UBSan, ARM analysis and formatting pass. Native HP50g interruption
before the final identity write leaves the bank hidden after hardware reset,
without the earlier abnormal library bindings. Independent retry then restores
only the target sector, preserves the journal and returns normally; repeated
replay performs no writes. This change does not protect a failed/partial erase
or other sectors, and does not establish automatic startup recovery.

The next independent prototype separates initialized bytes from zero-filled
runtime storage instead of embedding two copies of blank stack/workspace.
Its Stock package decreases from 61,788 to 49,442 bytes; the initialized ARM
copy is 18,384 bytes and the owned execution image is 30,960 bytes, retaining
the 4 KiB stack and existing workspace capacity. Entry clears BSS, initializes
guards and uses eight-byte image alignment. Native HP50g recovery and re-entry
of the same stored program pass TLS/workspace reset, bounds/guards, exact Flash
restoration and no writes on repeated recovery. This is still private packaging
research: a transfer attempt in the earlier two-probe/abnormal-library state
timed out without changing Flash. Reduced package size does not yet establish
a sufficient-memory check, peak installation budget or acceptance of that state.

Thumb compilation with position-independent ARM/Thumb interworking veneers
reduces the private package further to 39,986 bytes (13,656 initialized bytes,
26,232 owned runtime bytes), without reducing stack/workspace capacity. Native
HP50g recovery, TLS/clock, guards/alignment, exact Flash changes, no-op replay
and Stock return pass; residual GOT/absolute-address relocations are rejected.
Crowded-state uploads still fail: the damaged fixture times out, while the
restored-Flash control reports Invalid Server Cmd. A tiny transfer followed by
Stock MEM succeeds in the damaged fixture and reports only 11,658.5 bytes free,
independently establishing insufficient space for the recovery object. Flash
remains unchanged. An installation-space preflight and measured peak budget
are still required; the specific transport error paths remain unexplained.

A private pre-transfer check now queries Stock MEM over Kermit and drops its
result before deciding whether to upload. The crowded HP50g fixture reports
23,387 free nibbles against 79,956 serialized-object nibbles: upload is refused,
the server exits normally, the original empty stack is preserved and full Flash
is unchanged. The normal damaged-bank fixture reports 269,321 free nibbles and
passes upload plus independently checked native recovery, retained journal,
no-op replay and Stock return without Library 275. This is a necessary object
size check only, not a sufficient peak-memory budget or production installer;
transfer/installation overhead, error handling and other models remain open.

The independent CODE envelope is now maintained in `tools/package-recovery.py`
with host coverage for object lengths, initialized payloads, every destination
nibble alignment, reserved storage and invalid size/model rejection. Core CI
includes its Python tests and watches the tool. It accepts a trusted PIC ARM
binary plus explicit runtime size; it does not yet build the recovery kernel or
install it. The HP50g package removes the prototype's test-only return integer
and is 39,978 bytes. Native Stock recovery passes with the original empty stack,
exact target-sector restoration, retained journal, no-op replay and restored
hardware state. Other model entries and complete installer integration remain
pending; the raw binary caller still owns relocation validation.

The standalone `tests/e2e/port-recovery` CMake witness now builds the independent
entry, TLS/guarded stack and actual shared C sector replay from repository
sources. Core CI builds and packages it, checking relocation kinds and image/BSS
bounds; the existing Quality discovery also covers its ARM C source. The local
build reproduces the previously native-validated 39,978-byte package exactly.
Formatting and ARM analysis under the repository's MMIO policy pass. This closes
the witness's dependency on temporary build sources, not production recovery
installation or CI execution of the damaged-bank scenario.

The recovery saved-state verifier is now maintained beside the witness. It
accepts explicit artifacts and before/after state, independently checks journal
CRC/geometry and complete Flash differences, and checks native stack/catalog,
owned memory/guards, TLS, mapping/IRQ restoration, timing, repeat writes and
return to Stock ROM. It accepts the native HP50g recovery result; host corruption
tests and five saved-state negative controls reject damaged code/guards,
unexpected repeat writes, mapping drift and unfinished return. CI scenario
preparation and execution remain pending.

The fault-state preparation script now starts from a clean HP50g Stock boot,
validates native bank identities and an empty separate journal bank, and creates
a new state with the saved sector journal and erased target header sector.
Host tests cover source/output preservation, exact changes and invalid/nonempty
bank rejection. A fresh boot from standard firmware, offline fault preparation,
hardware reset and USB upload of the repository-built recovery witness pass the
maintained verifier; reset leaves the damaged Flash unchanged. The scenario no
longer requires historical research snapshots. CI orchestration and busy-chip
power-loss coverage remain open.

The maintained runner now completes fresh boot, offline fault preparation,
hardware reset, USB upload and independent saved-state verification locally
from a Linux source export. Core's Stock recovery / HP50g job is wired to the
shared portable emulator setup and Kermit watchdog, with retained phase evidence.
All 100 host targets, changed-file formatting, Python lint and strict docs pass
locally. Remote acceptance is pending. User-facing installation, memory-budget
acceptance, other models and busy-chip power-loss coverage remain open.

HP49g+ now independently passes the same fresh-boot recovery scenario using its
own bootloader, first in the controlled comparison and then through the maintained
runner's model option. The runner selects bootloader/LCD metadata from existing
target profiles and verifies the bootloader in saved Flash. HP49g+/HP50g share
the validated package format; unsupported families remain rejected. The Core
recovery matrix now includes both models. These changes are locally accepted;
their remote acceptance and the remaining installation/power-loss work are open.

Post-recovery hardware restart also passes on both models: all 61 original
native catalog entries, including bindings/switches/flags, are restored, while
complete Flash and the retained journal remain unchanged. The maintained runner
now requires this phase before reporting success. Host negative controls reject
missing/changed catalog entries, Flash changes, nonempty stack and unfinished
return, while permitting catalog reordering.

The maintained witness now has a test-only stop immediately before the final
bank-identity program command. Both models pass interruption verification,
hardware reset with the partial bank still hidden, upload of the normal witness
under a separate name, successful retry and final catalog reconstruction. Both
programs remain stored throughout the test. The oracle checks the exact partial
Flash image, retained journal, initialized code/guards, replay diagnostics and
owned PC/SP/IRQ state. Twelve verifier host tests and the interrupted ARM build's
static analysis pass. Core builds normal/interrupted packages and runs this
scenario for both models; remote acceptance is pending. This covers a command
boundary, not power loss during an erase/program pulse.

The recovery matrix additionally enables deterministic Flash busy intervals:
each program/erase requires 64 status reads before changing the array. Saved
operation/read counts must prove the busy path ran and completed. Unit tests
cover status toggling, deferred writes/erases, cancellation and bounds. This
checks native polling without claiming physical pulse timing or partial-cell
power-loss coverage; that coverage remains open.

Both HP49g+ and HP50g now pass selected native busy-operation faults: a partially
programmed word, followed by a partially erased sector on the next retry.
Each cut persists Flash and bypasses normal emulator teardown. Independent
whole-Flash comparisons verify the selected bit transitions and retained journal;
Stock restart keeps the partial bank hidden. Final recovery and another restart
restore all 61 catalog entries. Core includes both cuts before the existing
identity-publication interruption gate. This accepts these explicit fault
outcomes locally, not all physical power-loss patterns or automatic startup
recovery; remote acceptance remains pending.

Recovery now retains the inspected journal and a separate reload in the existing
workspace. Shared replay rejects any mismatch before target mutation, including
a different CRC-valid record, and rejects overlapping buffers. Both models pass
the full native fault sequence with independent checks of both RAM copies.
The checked ELF packager is shared outside E2E and regenerates missing raw-image
outputs. These changes are locally accepted; transaction freshness and complete
native reference repair remain separate requirements.

The independent recovery witness now also exercises journal retirement. Local
HP49g+/HP50g runs interrupt the status write with one selected completed bit,
restart Stock with the original 61-entry catalog, then finish retirement and
verify a repeated call performs no writes. Whole-Flash comparisons require only
the authorized status-byte change, preserving the neighboring byte and restored
sector; owned image/stack/TLS, return and final catalog checks pass. Twenty-one host
verifier tests cover invalid statuses, unfinished targets, collateral changes
and mismatched fault evidence. The maintained runner and Runtime workflow include
the retirement build and cut/resume stage. Broader physical fault patterns,
transaction freshness, reference authority and automatic startup integration
remain required separately.

An automatic-entry slice now passes locally on HP49g+/HP50g: install the owned
retirement witness as `STARTUP` without evaluating it, verify unchanged damaged
Flash, then warmstart into replay/retirement. A second warmstart reports no
pending journal and zero erase/program commands, preserving complete Flash,
the empty stack and all 61 clean catalog entries. Both runs use 64-read Flash
busy intervals and the independent image/stack/TLS/mapping verifier. The maintained
runner includes this separate fresh-fixture branch whenever the retirement build
is supplied. Production preservation of user startup programs, ordering against
library configuration, durable ownership/freshness and complete reference repair
remain open; this is not a general installed startup-recovery service.

The complete HP50g CI-equivalent recovery chain also passes locally, including
both configured busy-operation power cuts, interrupted bank-identity publication,
journal retirement and the two automatic warmstarts. A separate native Stock
2.15 ordering probe on HP49g+ and HP50g observes library configuration before HOME `STARTUP`.
Consequently the production entry must address recovery before other library
code can access damaged Flash, as well as survival after loss of HOME RAM.
On HP50g, two Port 2 probe libraries (1701 and 1702) configure in ascending ID
order with either installation order, before `STARTUP`. Ordering across arbitrary
libraries/ports and an available earliest recovery callback remain unverified.
Additional HP50g cross-port probes place Flash library 769 before RAM library
1702 but after RAM library 768. With a real HOME-appending configuration callback,
the earlier library runs twice across fault recovery and its internal restart;
the later library runs once. Both finish with exact recovery and all 63 catalog
entries. Selecting the lowest documented user ID is therefore insufficient to
guarantee safe production startup ordering.
HP49g+ and HP50g Stock dependency probes make that ordering failure observable: a RAM
library 768 configuration appends a marker, recalls the real Port 2 EQLIB library
226 and then appends a completion marker. After erasing the journaled bank-header
sector, recovery library 769 is never reached: the dependent configuration
repeatedly enters without completing (54 entries in each observation), Flash
remains byte-for-byte unchanged and both journal and authority remain armed.
With research-only recovery ID 4, the same dependency completes after
exact sector repair; all 63 catalog entries, owned recovery CODE state and both
closed transaction records pass verification. Both clean baselines complete the
dependency once. This establishes the need for recovery before dependent
configuration, not a production allocation of ID 4: factory IDs 0, 1 and 2 also
have configuration callbacks, and their safety before recovery remains unproven.
The public packager continues to restrict recovery IDs to 769..1791.
Inspection of the HP50g Stock callback bodies narrows the earlier-callback
question: library 1 recalls hidden `UserKeys.CRC` and `UserKeys`, checks the
object checksum and restores key assignments; library 2 attaches itself to HOME.
The primitive names are cross-checked against the authors' [entry database](https://www.hpcalc.org/details/5146).
Assigning the EQLIB object to a user key copies that object into RAM in the
observed Stock fixture, so its successful recovery does not establish safety
for banked references in user-key data. Library 0 and reachable banked data
still need validation. ID 4 also has a historical configuration-suppressor use,
described by its [author](https://files.mpoli.fi/pub/unpacked/software/hp48/hp48cojh.zip/_horn8.zip/cnflib.doc);
an installer must not assume that this ID is available on an existing calculator.
A further HP50g native fixture stores the actual user-key list in a Flash backup
and uses `StoHiddenVar` to retain an ACPTR in hidden `UserKeys`. Stock's `OCRC`
calculates the checksum of this pointer object, rather than the original list;
updating `UserKeys.CRC` with that result makes the clean warmstart preserve both.
Erasing the referenced backup's bank-header sector then still permits recovery
library 4 to restore the exact sector, retire/close the transaction and rebuild
all 62 catalog entries. The key reference/CRC and owned recovery image/stack/TLS
also pass. This accepts that explicit banked-key path; it does not prove every
earlier callback or arbitrary user-key representation safe.
The library 0 callback checks HOME's oldest, null-named directory entry. Its
existing-directory path reads RAM directory geometry; the missing-directory
path allocates a hidden directory and can call `GARBAGECOL` when `ROOM` reports
less than 20 nibbles. The inspected HP49g+/HP50g callback code agrees, and clean,
warm-repaired and cold-repaired saved states all have the expected RAM directory.
Those final snapshots establish the existing-directory precondition, not that
cold initialization never enters the allocation path. Installation and mutation
preflight must account for this distinction before claiming early-callback safety.
The maintained configuration fixture now also locates library 229's configuration
program from its catalog/header and erases that body's sector while preserving
the bank and library headers. Offline HP49g+/HP50g runs restore the exact sector,
close both transaction records and reconstruct all 62 baseline catalog entries.
The full maintained sequence additionally checks owned recovery state and an
unchanged later warmstart; that expanded sequence passes locally on both models.
This case does not prove that every earlier callback executed safely: Stock may
skip damaged library contents, and the earlier dependent-library counterexample
still requires a production startup design.
An explicit stale-journal host regression also demonstrates why structural
preflight is insufficient: a later valid backup inside the target sector is
hidden by the old image overlay. Public write preparation rejects ready and
partially retired sector journals without any Flash mutation. Durable
transaction authority remains required before production startup replay.

An internal one-use transaction-binding codec (`HPCAUTH1`) now binds a known
physical authority allocation to one validated sector journal's address, target
and status-independent CRC. It distinguishes unpublished, armed, closing and
closed states, rejects changed/moved journals and overlapping allocations, and
supports inspecting journal retirement without granting replay permission.
Host tests cover every status byte and single-bit corruption of the binding.
Caller-owned publication now verifies the durable journal and writes/verifies
the binding body before arming it. Bound replay checks that entire armed record
before the journal and target; closure requires a fully retired journal and an
exact target image before clearing the binding. Host tests cut every publication
and closure read/write, reject inactive bindings without erase/program commands,
and exercise publication → replay → journal retirement → closure. Later target
changes survive a replay attempt against that closed transaction. Resumption
of the same reserved publication verifies NOR-compatible partial body bytes
before completing them, requires exact bodies once status publication starts,
and never rearms a closing/closed binding. Host tests also interrupt resumption
and retry again. Trusted installed-root discovery, startup exclusion and
reference repair remain required; this is not production startup integration.
The bound read-only preflight now accepts an explicitly trusted authority/journal
pair, validates its phases and all restored bank chains, and proves both records
occupy exact native allocation boundaries. Matching records nested in deleted
payloads are rejected. Closed transactions validate current Flash without an
old-image overlay, preserving later valid backups; corrupt current objects still
fail preflight. Armed/retiring transactions and target-equal closure remain
distinct from clean, closed transactions. The bound Flash-configuration fixture
now passes locally on HP49g+ and HP50g: exact sector restoration, journal
retirement and authority closure, automatic complete catalog rebuilding, HOME
preservation, owned relocated CODE verification and a later unchanged warmstart.
The fixture's compiled journal/authority pair is explicit test input, not a
trusted production installation root. CI now selects this configuration mode
and uploads its generated library package alongside the standalone program.
The bound configuration runner also covers loss of all SRAM before the damaged
Flash boot. It declines Stock's memory-recovery prompt, then requires exact
sector restoration, retired journal and closed authority, the original complete
Flash library catalog, owned relocated CODE/stack/TLS and an unchanged later
warmstart. HOME preservation is checked only in the retained-RAM scenario.
Separate HP49g+ and HP50g cold baselines reconstruct all 62 Flash catalog entries;
both the standard library 1701 and research-only library 4 pass bound cold repair
and catalog reconstruction locally. This tests simulated RAM loss with an intact
installed recovery library, not physical battery loss or safe ordering against
arbitrary earlier library callbacks.
The recovery envelope retains an immutable initialized image and a separate
aligned runtime reservation so saved HOME programs can be executed repeatedly.
An in-place alignment experiment passed fresh Flash configuration starts but
overwrote its own source in HOME; that layout is not retained. Memory admission
and reducing the recovery footprint remain open requirements.
Installation must also remain independent of package size: Stock could
place a smaller library in the bank whose header the test erases. The configuration fixture
now occupies that bank's remaining tail with a native tombstone before Stock
installation and verifies that installation leaves the protected bank unchanged.
Recovery availability consequently no longer depends on package size forcing
Stock to choose a different bank. The tombstone is test setup, not a production
installer or space-reservation protocol. With the immutable-source envelope,
the complete maintained runner passes locally on HP49g+ and HP50g: both busy
power-cut masks, interrupted repair, journal retirement, saved-program reuse,
automatic STARTUP, bound Flash configuration, SRAM loss and unchanged later
warmstarts. All four build variants and their 16 Clang analysis configurations
pass. Remote acceptance of this follow-up remains pending.
An old binding
must not be erased/reused while its journal can survive, because a partial erase
can resurrect an armed status; root replacement needs its own protocol.

HP50g Flash-resident configuration prototypes now restore the injected header
sector and retire its journal automatically, both through a hidden library
command and with the recovery program directly in the configuration entry.
Full Flash comparison and 64-read busy-operation evidence pass. The first
repaired startup still exposes 59 of 62 catalog entries; another warmstart
restores all exact bindings without Flash changes. A post-startup invocation
also passes the owned RAM image/stack/TLS checks. The startup image survives as
a CODE object moved by native GC, including odd-nibble moves: the verifier now
explicitly supports reconstruction while checking original execution alignment,
RAM bounds, stack/TLS and the complete immutable image/guards and envelope.
Both configuration variants pass these checks. Clearing all 512 KiB SRAM before
HP50g reset also passes exact recovery, these ownership checks and Stock return
after cold initialization finishes. Production startup integration, installation
and transaction freshness remain open; this is not physical battery-loss testing.
HP49g+ independently passes direct configuration-entry replay/retirement and
the complete relocated-image checks, also retaining 59 entries on that first
repaired startup. Its RAM-loss case has not yet been accepted.

Both models pass the return-only Flash-configuration branch: native
library installation, clean catalog baseline, injected fault, automatic repair
and retirement, relocated CODE verification, then exact catalog rebuilding on
the second warmstart. The configuration mode now requests Stock reinitialization
after successful retirement, so the maintained runner requires all original
catalog bindings before a second host reset. Both models pass this maintained
automatic path, preserve the HOME string and end with a verified no-op invocation. The workflow
builds this mode with `HPCLANG_RECOVERY_RESTART_AFTER_REPAIR` and supplies it with
`--configuration-build`; ordinary retirement still returns to its caller.
A host-generated configuration-only library now replaces calculator-side
library compilation in this fixture. The packager validates the CODE envelope,
native length and CRC; its explicit user-library ID does not reserve a number
or guarantee priority. The original program artifact remains available for
independent owned-image verification.
A persistent HOME string also checks preservation of user RAM. Synthetic busy
reads apply to the recovery stages; Stock constructs
the fixture with the normal emulator Flash model because its installation
driver does not pass the synthetic read-count timing model. Production
installation/freshness and safe ordering before dependent libraries remain open.

Both models also accept a later invocation of the already installed recovery
program: discovery reports no pending journal, all erase/program counters stay
zero, the complete Flash image stays unchanged, and a subsequent reset retains
the same catalog. Reusing the installed program avoids the insufficient-memory
failure caused by accumulating another identical recovery package. The shared
Kermit transport now supports remote-command-only sessions; three host transport
tests cover this path, preserved upload ordering and malformed requests.

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

Catalog-plan assembly is now shared C code: it resolves each binding's physical
mapping and produces bounded address/switch edits plus count/ID/flag guards.
Errors invalidate both output counts. Host failure/alias/capacity cases,
sanitizers and ARM analysis pass. Against the saved native PACK capture it
reproduces all 124 edits and 125 guards; only library 275 changes address.
This completes the catalog portion of plan assembly, without native writes or
claiming coverage of the other root classes.

The native root enumerator now exposes read dependencies alongside source-tagged
roots, including structural table fields. Host checks match the complete backend
read trace and preserve callback failures without writes; sanitizers and ARM
analysis pass. On HP49g+/HP50g saved registration states it preserves 457/458
roots and records all 2,666/2,676 reads before/after registration. Bounded guard
assembly now coalesces repeated/adjacent observations without covering unread
cells, and invalidates partial output on conflicts or capacity exhaustion.
Host ordering/failure tests, sanitizers and ARM analysis pass. The same saved
states produce 527/529 guards (8,432/8,464 bytes), exactly covering all
2,631/2,641 distinct observed cells; changing the catalog count rejects validation.
These are sample storage measurements, not a fixed workspace guarantee.
ARM-side root ownership and complete object-body traversal remain required
before using this evidence for live relocation.

Nested INLINE/COMPACT enumeration now runs in shared C with caller-owned work
storage and a node budget. Host tests cover compact bodies, repeated array
aliases, workspace/budget exhaustion, read/callback failures and retained opaque
contents; sanitizers and ARM compilation/analysis pass. On both HP49g+/HP50g
saved registration states, HOME plus 73/75 TEMP blocks produce exactly the same
151/154 nodes and 288/289 edges as the independent immediate-edge scheduler.
Read guards exactly cover observed cells. The ten opaque edges and external
targets remain unresolved; these results do not close reference completeness.

A controlled HP50g comparison now separates native library registration from
ordinary application activity. Identical ARM payloads pass both lifecycle runs
and return normally in both cases. Only the registration case adds library 123
to the catalog and advances TEMP bottom by 16 nibbles. The 50- and 798-nibble
hex-string objects change in the control case as well; the 50-nibble object's
final contents are identical in both cases despite different native addresses.
Thus paired payload differences alone do not identify relocation fields. These
objects still require semantic classification; no raw-address scanning or
reference-free assumption is accepted from this experiment.

The 798-nibble object is now identified through Stock's `leeway` root. On
HP49g+/HP50g, independently copied contents remain unchanged across native
garbage collection. A deliberate address-shaped value also remains unchanged
while the referenced test object actually moves; the HP50g no-collection
control preserves the contents too. This establishes that numerical address
matches in this buffer must not become relocation edits. It does not classify
the other opaque objects or discharge the launcher's owned-pointer obligations.

The observed 50-nibble launcher descriptor has two ARM entry/base fields. Native
HP49g+/HP50g controls still complete UI execution and Stock return when both
fields are deliberately replaced with invalid addresses after application entry.
Both models also pass the catalog-growth diagnostic, descriptor movement, normal
return and relaunch/readback while retaining those invalid fields. This bounds
their liveness on the tested Launcher V3 path; other descriptor forms remain
unclassified. The cached-program machine-stack values still require a production
type/provenance and liveness contract. The host stack-restoration regression retains the
relocated object root without reading the consumed ARM fields again.

Native helper traces on both models identify the two saved conditional-branch
positions and the launch subroutine return address, with their actual producers
and circular-stack slots matching the suspended application snapshot. A separate
catalog-growth diagnostic on both models returns and relaunches successfully
while relocating only the subroutine return, leaving the two branch positions
unchanged. Thus the earlier assertion that all three must be repaired on normal
return was too strong. A stronger native continuation witness now passes on both
models with either root set: after registration, the original RPL caller executes
its post-call marker. Redirecting the saved post-ARMSYS continuation to the
validated launcher error exit produces the distinct outer error-handler marker;
the same catcher without fault injection produces only the success marker.
These checks also require the inserted catalog entry and unchanged Flash.
Native-cell watchpoints on both models observe the launch return being consumed,
while the old branch positions are overwritten without a reported intervening
read after application return. HP50g cursor tracing attributes that replacement
to ordinary modulo-eight pushes/pops, not a separate stack reset. A nonzero
slot and the circular cursor therefore do not establish a live execution root.
They cover normal return and that explicit launcher error path, not arbitrary
exceptional paths or every stack consumer. Production classification and complete
relocation coverage remain open.

A separate HP49g+/HP50g native collection control retains an object on the data
stack while saving its old numeric address with `RSTK=C`. Native collection
moves the owned object but leaves that machine-stack number unchanged. Thus
neither address-range scanning nor invoking the native collector supplies a
general execution-root classifier; live continuations need call-path evidence.

Shared edit assembly now merges identical observations across discovery paths
and rejects conflicting values, partial overlaps and exhausted storage without
publishing a partial count. Host tests, sanitizers and ARM compilation/analysis
pass. In both HP49g+/HP50g saved registration states, root/catalog/attachment
observations merge from 601/604 to 575/577 unique fields (26/27 duplicates).
Forward/reverse discovery order gives identical batches; native expected-value
preflight passes and a contradictory duplicate is rejected. This read-only
experiment uses unchanged values to verify assembly, not relocation semantics
or complete reference coverage.

ARM-side root scanning now coalesces repeated cell addresses before visiting or
relocating them. Distinct cells holding the same value still move independently.
The regression reproduces the previous double-shift bug; sanitizers also verify
that a later invalid root fails before RAM or root changes. Related host tests,
ARM compilation and static analysis pass. HP49g+/HP50g native catalog-growth
controls deliberately repeat the saved return-PC cell and still reach the RPL
post-call marker with the expected catalog entry and unchanged Flash. This fixes
duplicate application, without claiming complete caller classification.

A stopped-state HP50g integration experiment now combines shared root/nested
enumeration, qualified shift planning, edit merging and RAM replay. Its 738
unique fields include 44 changed values. After host-side relocation and catalog
insertion in an isolated snapshot, the unchanged ARM test application relaunches,
reads back its native library witness and returns normally, as does the unmodified
control. Independent state checks confirm exactly one added library-123 binding,
the expected 16-nibble TEMP-bottom shift and unchanged Flash. This establishes
the combined plan on that fixture, not calculator-side commit: stopped-state
host mutation bypasses active ARM-root ownership, and ten opaque objects remain
unclassified. Public reclamation remains gated on those retained obligations.

Calculator-side HP50g execution now isolates an additional launcher obligation.
Nested-field repair plus the existing catalog grow helper reaches the payload
PASS marker but faults during Stock return. A diagnostic variant additionally
validates and relocates three library-275 continuation values in the native
execution context; it passes initial return, relaunch and data readback.
Independent checks after both runs confirm the added catalog binding, expected
TEMP-bottom change and unchanged Flash. This caller-specific experiment is not
a general root ABI: its fixed offsets and command-body signatures remain outside
the product. A validated platform execution-root interface and broader caller/
model coverage are required before calculator-side relocation can be accepted.

ROM instruction and caller inspection confirms the eight-cell machine-stack
layout, 20-bit push values, modulo-eight cursor and clear-on-pop behavior. It
also confirms the numeric `RSTK=C` / `C=RSTK` path, so layout validation alone
cannot classify all cells as pointers. A diagnostic whole-stack variant passes
initial return, relaunch, library readback and independent catalog/TEMP/Flash
checks on both HP49g+ and HP50g for the tested launcher path. This broadens the
native integration evidence without accepting unconditional stack repair; caller
classification or independently owned entry/return roots remain required.

Read-only captures through the existing independent recovery entry also pass
on HP49g+/HP50g with library 275 absent: exact CPSR restoration, intact image/
stack guards, empty native data stack, Stock ROM return and unchanged Flash.
Both retain one machine-stack value in the proposed movable TEMP interval,
despite different ring cursor positions. Independent entry therefore does not
remove the need to analyze the outer Stock caller; replacing the historical
runner is not treated as a shortcut to complete relocation safety or as an
implicit change to the retained M8.6 evaluation scope.

The remaining independent-entry TEMP candidate belongs to a cached library
161 command 17 program. Catalog/bank/link resolution identifies its Flash source;
the shared cached-program matcher accepts all 52 program nibbles and rejects
incorrect identity or allocation extent on both captured model states. Host tests
also reject changed content, unreadable nibbles and invalid bounds. This replaces
command-offset assumptions with bounded cache identity validation, while leaving
the machine-stack value's semantic classification and live relocation gate open.

Instruction inspection distinguishes the retained caller cases: the independent
entry's preceding Stock handler saves the RPL cursor with `CD0EX` / `RSTK=C`;
the library-275 command-0 candidates lie on nested RPL boundaries, while the
command-4 candidate follows a machine-code `GOSUB`. These are different root
provenance paths. Cache matching alone must not collapse them into one generic
address-range rule, and the diagnostic command offsets remain outside product
code.

A calculator-side HP49g+/HP50g diagnostic now resolves each candidate's containing TEMP
allocation and native command source instead of using the three command-relative
offsets. It validates the user Flash bank mapping and complete cached programs,
then checks nested RPL boundaries or bounded machine-call return candidates.
The model-specific test passes initial return, relaunch and data readback;
independent checks confirm exact catalog/TEMP changes and unchanged Flash. Its
universal build hits `ENOSPC` in the preceding Port 0 creation test, before any
relocation, so application memory budget remains part of integration acceptance.
The diagnostic still does not prove arbitrary machine-stack values' types and
does not enable public reclamation. Both saved first-run states confirm five
declared native roots, zero changed nested fields and ten opaque obligations;
this smaller application therefore does not replace the earlier nested-field
relocation evidence. The current host suite passes all 101 tests.

Stage1 now recognizes an immediate execution root on HP49g+/HP50g: a saved
post-ARMSYS PC naming `RTN`, `RTNSC` or `RTNCC` proves that the circular
machine-stack top will be consumed as an address. Only that cell is added;
lower slots remain unclassified. Local original-system diagnostics on both
models use only stage1's three roots, grow the catalog and reach an independent
post-call RPL marker with unchanged Flash. Exhaustive opcode/cursor unit tests
pass with ASan/UBSan; all six launchers build, and HP39g+/HP48gII Direct UI
regressions pass return, independent pixels and memory checks. This closes one
root-discovery case, not complete execution coverage, registration or reclamation.

The Port development branch now integrates the standalone historical-system client
and system adapters. Both integrated HP40gs Stock clients pass normal return and
independent pixel checks, replacing the old combined fixture that failed with
`0x464100c0`. All 87 host tests, focused sanitizers/static analysis, formatting and
strict documentation checks pass. Public Port 1 replacement/removal remains open:
bank-qualified traversal reaches ROM machine-code entries and user-library code
whose execution references still require classification.

</details>

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

The UI allocator-pressure fixture now keeps the live view while an additional
gray16 renderer fails with `ENOMEM`, then releases the pressure blocks and
reconstructs the original renderer. Independent LCD checks require unchanged
pixels during pressure and after reconstruction. HP39g+ and HP50g Stock pass
locally with application-visible RW capped at 36 KiB; the shared launcher builder
and CMake API expose that page-aligned ceiling. The legacy Stock reservation is
not reduced, and stack peaks/whole-application admission remain open. This local
follow-up has not yet received CI acceptance.

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

- [x] Provide reusable text, action, selection, scrolling/list and editing
  components with composable containers; confirm the initial component set
  through real example screens rather than application-specific dialogs.
- [x] Implement a usable default surface/font renderer and explicit theme
  inputs. Keep widget appearance, font choices and rasterization outside the
  state/layout engine. Measurement and drawing must agree across backends.
- [x] Demonstrate the same application tree with a second renderer/theme,
  including focus and input behavior. Preserve the downstream 39gsX native
  appearance use case; a claim of faithful HP39 reproduction requires separate
  reverse-engineering and visual evidence.
- [x] Adapt the original sketch's illustrative native drawing/pointer boundary
  to actual monochrome/grayscale surfaces and keyboard-only operation. Keep
  pointer support optional and test clipped drawing ownership/publication;
  RGB565 and device scanning in the sketch do not imply new target hardware.
- [ ] Validate six-model geometry, native/fallback fonts, clipping, scrolling,
  partial redraw and custom canvas, including low-memory failure and clean
  Stock return. Record frame/update and memory budgets.

The first four items are covered by the reusable C/C++ interfaces, the installed
device-manager example, independent theme/font/canvas LCD checks in the accepted
six-model Runtime UI suite, and the current `ui`, `ui_cpp`, `ui_surface`,
`ui_demo` and `ui_surface_alloc` sanitizer tests. The final resource/low-memory
acceptance remains open for the bounded follow-up's CI and whole-app analysis;
this does not claim faithful HP39 native styling or complete M8.3.

The maintained 36 KiB UI artifact now passes all six Direct and all six Stock
targets locally, including exhausted-allocator rendering, reconstruction pixels,
native/fallback font geometry and normal return. SDK relocation also passes with
an external C++ application's RW limit, all six launcher/notebook builds and
three HP39 Aplet package families. CI acceptance and the remaining whole-app
resource analysis are still pending.

UI acceptance now records touched stack words at entry, before main returns and
after libc/runtime cleanup, using a test-only startup wrapper. A bounded memory
checkpoint saves the final measurements before launcher unmapping; Direct and
Stock artifacts include the versioned JSON. HP39g+ Direct reports 2,416/8,192
stack bytes and 18,892/22,920 heap bytes; HP50g Stock reports 2,424/8,192 and
20,076/22,920. Both retain the 36 KiB RW ceiling and pass independent UI pixels
and normal return. All six Direct and all six Stock targets now pass with the
same memory-report gate locally. A watermark
measures touched words, includes instrumentation overhead, and does not bound
untouched stack reservations or establish a minimum physical Stock allocation;
those distinctions remain part of whole-application resource analysis.

The startup resource review found that stage0 copied its C stage and moved SP
before stage1 could reject an undersized page pool. Stage0 now checks the linked
stage extent plus its reserved stack page before those writes or MMU changes,
including HP39's native-workspace exclusion and the coarse-table capacity.
The Direct harness first executes three rejected launches (zero pages, one page
and an excessive count), checking caller registers, interrupt state, mappings
and the complete untouched temporary pool before ordinary application execution.
HP39g+, HP48gII and HP50g pass those checks and the independent UI/memory/return
gates locally; the old launcher fails the same rejection sequence. HP50g Stock
also passes, as does HP39g+ Stock with its native-workspace exclusion. All six
launcher builds, relocated C/C++ SDK clients, model-specific notebooks and all
three HP39 Aplet package families pass. This closes an early startup overwrite
hazard, not the remaining
whole-application memory-admission or physical-reservation work.

The previous Runtime CI batch exposed a separate test-launcher defect: the
HP39gs/40gs ARMSYS probe clobbered `r2` before saving registers when skipping
its Stock-only path in Direct mode. The probe now preserves that register on
both its skip and replay edges. All three HP39 Direct Runtime targets locally
pass the three rejected launches, keyboard progression and normal return; the
downloaded failing CI launcher reproduces the old rejection failure on HP39gs.
HP39gs and HP40gs Stock Runtime also pass USB installation, the ARMSYS replay
probe, the complete keyboard sequence and return to the original Aplet UI.
Remote acceptance is still pending.

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
and normal return), and the updated relocated SDK acceptance pass. Runtime and
Quality CI are accepted on `c2fb66fe`. The original void signature is retained with errno and unchanged
output on failure; weekday/year-day are computed rather than copied from the
historically unreliable RTC weekday field.
The remaining historical system, timer and console interfaces remain open.

The clean console now supplies cursor/state queries, cursor movement, erase to
end of line and configurable tab stops. Tab expansion is bounded by the grid
size even for `UINT_MAX`; controls preserve the documented incremental UTF-8
behavior. Sanitizer tests and ARM analysis pass. Direct and Stock acceptance
passes locally on HP39g+, HP48gII and HP50g, including independent LCD checks
and normal return. These are prerequisites for the historical console adapter
below, whose default streams, byte encoding and HPG ownership need their own
integration acceptance.

The next local output adapter supplies `hpconio.h` registration, cursor/clear/tab
spellings and calculator-byte standard output on HPG's shared display lease.
It preserves surrounding graphics and scrolls them with text, honors explicit
stdio owners, and keeps HPG double-buffer publication explicit. Host sanitizer
tests, ARM static analysis and all six Direct LCD checks pass. HP39g+, HP48gII,
HP49g+ and HP50g Stock also pass. The installed C/C++ console example passes
SDK relocation, six-model launchers and three HP39 Aplet packaging families.
HP39gs/40gs Stock also pass; CI acceptance is pending. Historical input and numeric
formatting remain open, so this does not close the console group.

The follow-up `hpstring.h` group implements `ints/uints` using the verified
1.0/1.1/2.0 base, sign and borrowed-buffer contracts, including defined `INT_MIN`
handling. All bases and boundary values pass host sanitizers; ARM analysis and
HP39g+/HP50g Direct conversion checks pass. HP50g Stock and updated installed-SDK
relocation/packaging also pass locally; CI acceptance is pending.

The default character-input callback now uses model-native Alpha/Shift state
and the existing physical event stream, with shared HP-byte mapping for native
and HPG fallback fonts. Six-model host checks cover ordinary input, single/held
Alpha, classic locks and one-shot lowercase, Enter/backspace, cancellation and
poll failures. Font/HPG sanitizers and ARM analysis pass. All six Direct and
all six Stock physical-key checks pass locally, as does the updated SDK
relocation/packaging gate. Historical input aliases are covered below;
numeric-format policy remains open.

The bounded `hpclang_hpgcc_gets` editor now integrates the tested byte-line core
with standard streams and HPG rendering. Host sanitizers cover overflow/re-edit,
partial-text cancellation, restoring graphics after scroll undo, synchronized
double-buffer editing, and explicit stdio ownership without display acquisition.
The installed interactive example passes the SDK relocation/packaging gate.
HP39g+/HP50g Direct physical-key editing passes a full-buffer rejection/re-edit
scenario, with the scrolled result matching an independently drawn reference
frame pixel for pixel. Both models also pass Stock. The maintained independent
console verifier now checks edited/reference frames on all six Direct and all
six Stock targets, as recorded below.

`hpconio.h` now adapts historical `gets/_gets` calls on fixed `char` arrays to
the bounded editor in both C and C++. Pointer expressions and variable-length
arrays require the explicit-capacity entry point; they fail before consuming
input through the aliases. Host regressions check struct-member bounds,
overflow/re-edit, single argument evaluation and unknown-capacity rejection.
Both GCC ASan/UBSan and optimized Clang checks pass. The host stream fixture
also covers libc's optimized `getc/putc` paths. The maintained ARM console
witness uses `gets` and passes all six Direct and all six Stock targets with
normal return and independent console/editor pixel checks locally. The updated
installed-SDK C++ client also passes; formal CI for this batch remains pending.

`hpstdio.h` now provides blocking `readint/readdouble` over that bounded editor.
They retain historical numeric-prefix conversion and the 31-byte line capacity;
invalid conversion, integer overflow and input cancellation have defined results.
Host sanitizers cover boundaries, prefix conversion, floating range errors and
input failures. ARM build/analysis, HP39g+ Direct and HP50g Stock physical-key
numeric input, and installed-SDK relocation/packaging pass locally. The split
console suite below completes six-model Direct/Stock local numeric acceptance;
CI acceptance of this addition remains pending.
`readuint/readhex/readlonglong/readulonglong` now share a checked integer parser
matching the reviewed historical sign, whitespace and bare-hex rules. Boundary
and error cases pass host sanitizers; ARM analysis and HP39g+/HP50g Direct
physical-key acceptance pass locally; HP50g Stock also passes. Installed-SDK
acceptance compiles and packages a separate C++ client using all six numeric
entry points and the historical integer typedefs.
Decimal-format controls remain open.

Adding numeric conversion to the combined Display payload exposed HP39g+ Stock
memory exhaustion while opening the existing 16-level display, before console
tests ran. Historical console/input acceptance now has its own `hpgcc-console`
runtime suite and independent pixel verifier; Display retains its original
allocation-exhaustion, gray-level and cleanup checks. The split ARM payloads are
48,552 and 66,192 bytes, versus 75,616 bytes combined. Native acceptance of the
split console passes all six Direct and all six Stock targets, including
physical input, independent pixels and normal return. Split Display also passes
HP39g+ Stock with its original allocation/gray assertions. A terminated USB
transfer was diagnosed and repeated before this result. CI acceptance remains
pending; the failed combined payload is not accepted.

The next system-adapter batch adds owned `sys_playTone2` (Hz/milliseconds),
`sys_playTone` (Hz/hundredths of a second), and the 1.1/2.0 packed-BCD helpers.
Tone duration overflow fails before playback; the old clock-assumption error
is not reproduced. The compatibility guide records the different 1.0 register
return contract and numeric bug instead of claiming identical four-version
behavior. Unit tests, sanitizers, ARM static analysis and all six Direct targets
pass locally. The relocated SDK passes C/C++, six-model notebook/launcher builds
and all three HP39 Aplet package families with the updated tone/BCD example.
All six Stock targets also pass with independent pixels and normal return;
Runtime and Quality CI are accepted on `c2fb66fe`.

The follow-up adds `sys_slowOn/Off` as owned ECONOMY/HOST profile selection,
retaining slower host clocks and restoring the saved host configuration rather
than historical fixed register values. Host error propagation, sanitizers and
ARM analysis pass; native checks cover repeated selection, frequency bounds,
monotonic continuity and restoration. All six Direct and all six Stock targets
pass, as does the relocated SDK with the updated installed-package example,
six-model launchers and three HP39 Aplet families. Runtime and Quality CI are
accepted on `c2fb66fe`.
Separately, the emulator RTC divider now carries fractional microseconds across
ticks: all 128 settings over 4096 expirations each, sanitizers, a pinned-source
build and HP50g Direct/Stock regression pass. This fixes the test environment;
the historical RTC tick adapter acceptance is recorded below.

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
- [ ] Complete `kos_beep/kos_CheckBeepEnd` compatibility with owned asynchronous
  sound, completion queries and active-tone preemption policy. Verify flags
  against each original system; cover autonomous stop while application code is
  busy, overlapping requests, cancellation and normal/immediate-exit restoration.
  Keep Stock interrupt handlers out of the temporary application mapping; the
  existing blocking tone service alone does not satisfy this adapter group.
  The internal PWM4 one-shot lease passes host sanitizers and six-model Direct
  expiry/rearm/restoration probes with the interrupt masked. Banked IRQ stack
  exchange/restoration also passes six-model Direct probes with actual delivery
  during a busy loop; System Services E2E now maintains this check and passes all
  six Direct targets locally. Owned dispatch,
  absolute-deadline handling and autonomous audio shutdown remain unfinished.
- [x] Implement the RTC tick adapter group with configurable `(n + 1) / 128`
  second periods, query/restore semantics and coalesced pending indications.
  Preserve nonpositive wait behavior and account for a pending first tick;
  define ownership and exit restoration before claiming hardware equivalence.
  The internal RTC tick lease engine now passes host and sanitizer tests for
  selector validation, coalesced latch consumption, host configuration/pending
  preservation and unrelated interrupt isolation. The three public adapters
  and optional normal-exit/`_Exit` cleanup are now wired into the SDK; interface
  and exit-path sanitizer tests pass, and ARM applications/launchers build.
  Six-model Direct acceptance now includes a pre-Stock-restoration check of
  RTC control/divider and tick routing/mask. An HP50g `_Exit` variant passes,
  and deliberate missing cleanup is rejected. The relocated installed SDK
  also passes C/C++, six-model launcher/notebook and HP39 Aplet packaging with
  the adapters used by its timer example. All six Stock targets now pass the
  complete interface/timing payload, independent pixels and normal return.
  Host/ARM analysis passes across 265 sources and 777 configurations. Runtime
  and Quality CI are accepted on `413e5891`, including the HPGCC system suite
  in all three Direct families and all six Stock models.
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
