# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/zyf722/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Scope of the completed milestones

M0–M6 describe the **execution foundation**: compiler/runtime viability, the HPC1 ABI, calculator launch paths, stock-system return, packaging, fixtures, and automated E2E.

They do **not** mean that hpclang already reproduces the application-facing feature set of HPGCC 1.x, HPGCC 2.x, or HPGCC3. M7 now provides stable device facts, monotonic time, the complete five-model physical keyboard/event core, opaque 2/4/16-level software surfaces, geometry, native/generated font and text rendering, and hp4950 stream stdio through picolibc. Storage namespace/metadata, calculator-object/RPL integration, several system services, and higher-level compatibility remain active SDK work.

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

**Runtime and automated stock-launch validation complete for HP 39gs/40gs. HP 39g+ blocked on real hardware fixture.**

- [x] HP 39gs real-hardware canonical runtime PASS and clean return.
- [x] HP 39gs and HP 40gs official-ROM Direct E2E.
- [x] HP 39gs/40gs hardware-validation packages.
- [x] HP 39g+ model-1 hardware-validation package using the shared HPGCC39 APLET/ARMToolbox39 boundary.
- [x] Exercise stock serial-disk transfer of `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000` through the original HP39gs/40gs ROM.
- [x] Drive stock APLET → HPCLANG → ARMToolbox39 → HPC1 PASS → stock return on HP 39gs and HP 40gs.
- [x] Validate a read-only HP39 flash-dump mechanism byte-for-byte against emulated physical NOR.
- [ ] Acquire a real HP 39g+ and dump its complete stock flash.
- [ ] Add the verified HP 39g+ flash fixture to Direct runtime E2E.
- [ ] Automate the original-system HP 39g+ install/launch path available on real stock firmware.

The HP 39g+ items are **blocked**, not active research tasks. The retained NOR dumper validates a transient physical-flash mapping mechanism; it does not establish that HP39g+ shares HP39gs/40gs firmware identity or native-font offsets.

## M4 — HP48 platform / HP 48gII

**Complete.**

- [x] Add model 4 / 131×64 E2E target profile.
- [x] Record the genuine HP48gII hardware differences: 256 KiB physical RAM, 64-line LCD, and HP48gII firmware identity.
- [x] Add the x50ng HP48 preparation mode.
- [x] Establish and pin a genuine 2 MiB full-NOR fixture.
- [x] Boot the full-NOR image with 256 KiB RAM and 131×64 LCD.
- [x] Execute the same HPC1 runtime and validate clean return.
- [x] Exercise the supported ARMToolbox48 calculator-side launch path through stock Serial Kermit.

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

**Complete for five models; HP 39g+ blocked.**

Durable Direct and Stock launch evidence exists for HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. Control-flow return and visible stock-UI repaint remain deliberately separate gates. Runtime **35298949365 / #135** is the final M7.1 fully-green canonical Runtime evidence across all Direct and Stock families, including HP48 payload PASS, stage teardown, control-flow restoration, settled Stock repaint, and the exact 52-frame keyboard progression. Runtime #122 remains the first run that closed that full contract.

Blocked:

- [ ] HP 39g+ Direct runtime on a verified native stock flash fixture.
- [ ] HP 39g+ original-system launch on that fixture.

Direct runtime owns common-bootstrap execution and return correctness. Stock-launch gates own calculator-side package/menu/launcher integration.

## M7 — Application-facing calculator services

**M7.1 complete; later M7 slices remain in progress.**

M7 is where hpclang becomes a practical calculator SDK rather than only a validated native execution environment. Its completion criterion is **clean-core capability parity with the ordinary application-facing jobs of HPGCC**, not an arbitrary count of APIs.

M7 deliberately separates capability parity from historical spelling. A service belongs in M7 when a real application needs the capability even if the eventual HPGCC-compatible function name lives in M8. By the end of M7, the clean core should be sufficient to implement the broad HPGCC adapters without reaching back into raw KOS/Saturn/MMU internals.

Planned dependency order after M7.1:

1. **M7.2 — byte-stream filesystem and stdio parity:** complete namespace, metadata, directory, capacity, and standard-C file behavior on HP49g+/HP50g; unsupported families remain explicit.
2. **M7.3 — calculator objects, Saturn/RPL and stack:** typed object handles, copy/convert/ownership rules, named object stores, and stack exchange without treating calculator objects as files.
3. **M7.4 — remaining common system services:** RTC/wall clock, sound/beep and other broadly useful HPGCC-era system facilities that can be exposed without leaking raw hardware ownership.
4. **M7.5 — display/runtime capability gaps needed by real HPGCC-class applications:** resolve physical grayscale/page-flip/annunciator or equivalent application needs behind an ownership-safe broker where worthwhile; leave raw MMU/cache/IRQ controls as migration-only unless a genuine clean use case appears.
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

Device information, stock-preserving monotonic time, keyboard scanning/events, physical key identity, the C++23 wrappers, display geometry, opaque software surfaces, blitting, text rendering, and LCD ownership preservation are implemented and validated on the current five-model Direct matrix. HP39g+ key identity remains UNKNOWN until its blocked real-machine fixture exists.

The display clean-core boundary remains explicit: representation-independent circles/polygons and owned 2/4/16-level surfaces are supported; historical mutable clip/style state, PAINT/XOR, patterns, annunciator slots, XPM convenience loaders, raw framebuffer-pointer behavior, physical gray4/gray16, and page flipping remain compatibility/future work.

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

Direct Gallery intentionally does not fabricate a live Saturn `Read[]` map. Generated-font exhaustive rendering is valid there on all five models; HP39/40 native fonts are also valid through verified physical-ROM rasters; HP48/49/50 native-font evidence comes from Stock launch where the live Saturn mapping exists.

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
- Stock KOS is **not** a second file-I/O backend. It is retained only at the host-coordination boundary: insertion/write-protect sampling before takeover and syscall 19 (`ResetSD`) after FatFs is unmounted and the controller is released;
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

M7.2 is complete: a typical HPGCC HP49g+/HP50g program that uses files and directories can be ported without losing application functionality, while hpclang applications use standard C/POSIX semantics or cleaner typed queries rather than KOS internals. Final focused evidence is **E2E / Storage #53 (`35428824540`)**, fully green across Build, Direct hp39/hp48/hp4950 and Stock hp4950 on `c4b827fe`; **CI / Core runtime #254 (`35428824495`)** is also green on the same head. The representative HPGCC adapter flow includes HP-style/standard paths, read/write/seek/truncate, local cwd, LFN enumeration, metadata, volume size/free queries and cleanup. POSIX `ftruncate()` extension is explicitly zero-filled and regression-tested.

### M7.3 — Calculator objects, Saturn/RPL and stack integration

**Planned after the M7.2 filesystem namespace/metadata core is stable.**

- [ ] define opaque Saturn address/object references; never expose Saturn addresses as ordinary ARM pointers;
- [ ] design copy/borrow/ownership/error rules for calculator objects before freezing public handles;
- [ ] implement the HP39 typed object-store path for named Aplet/Zaplet, Note, Program, Variable, List, Matrix, Library and related native records;
- [ ] implement HP48/49/50 RPL object exchange against the stock object namespace without flattening objects into byte-stream files;
- [ ] add stack read/push/pop/replace operations where the platform meaning is real and safely restorable;
- [ ] support common HPGCC `saturn.h` / `satdir.h` application patterns through adapters above the clean object service;
- [ ] validate round trips against the original Stock environment, including malformed-object/error cases and return-state preservation.

Completion criterion: typical HPGCC programs that exchange RPL/calculator objects or use the stack can migrate without raw Saturn pointers or private OS calling conventions.

### M7.4 — Common system services

**Planned.**

Prioritize application capabilities that appear repeatedly in HPGCC programs and have a coherent stock-hosted implementation:

- [ ] wall-clock/RTC date-time query and conversion;
- [ ] tone/beep with explicit frequency/duration and safe host restoration;
- [ ] useful battery/power/contrast/status queries where semantics are stable across families;
- [ ] capability discovery for services that are genuinely model-specific;
- [ ] classify clock-rate control, raw interrupt/timer programming, cache/MMU manipulation and physical allocation as low-level Migration/Unsupported unless an ownership-safe application use case justifies a typed service.

### M7.5 — Remaining display/runtime parity needs

**Planned; driven by real application requirements, not by copying every HPGCC primitive.**

- [ ] determine whether physical gray4/gray16 and page flipping need an on-demand contiguous/DMA resource broker;
- [ ] cover annunciator/status-plane needs through a stock-safe abstraction if representative applications require them;
- [ ] retain clip/style/pattern/XPM compatibility state in the future HPG adapter unless independent hpclang applications demonstrate a cleaner core need;
- [ ] validate any temporary LCD ownership against the existing before/PASS/return state-restoration gates.

### M7 parity closure

Before calling M7 complete:

- [ ] every major ordinary HPGCC application capability is implemented in the clean core or explicitly classified as Adapter/Migration/Unsupported with a reason;
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
