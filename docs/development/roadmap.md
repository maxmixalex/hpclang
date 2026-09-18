# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/zyf722/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Scope of the completed milestones

M0–M6 describe the **execution foundation**: compiler/runtime viability, the HPC1 ABI, calculator launch paths, stock-system return, packaging, fixtures, and automated E2E.

They do **not** mean that hpclang already reproduces the application-facing feature set of HPGCC 1.x, HPGCC 2.x, or HPGCC3. M7 now provides stable device facts, monotonic time, the complete five-model physical keyboard/event core, opaque 2/4/16-level software surfaces, geometry, native/generated font and text rendering, and hp4950 stream stdio through picolibc. Calculator-object/RPL storage and higher-level UI remain application-facing SDK work.

hpclang is also not intended to be binary-compatible with historical HPGCC executables or to reproduce HPGCC3's custom-ROM/ROMLIB environment.

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

Durable Direct and Stock launch evidence exists for HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. Control-flow return and visible stock-UI repaint remain deliberately separate gates. Runtime `35248256215 / #122` is fully green across all Direct and Stock families and closes HP48 payload PASS, stage teardown, control-flow restoration, settled Stock repaint, and the exact 52-frame keyboard progression. Post-cleanup Runtime #125 preserves the strict HP48 PASS but has an independent HP49g+ Stock timeout, so #122 remains the latest fully-green canonical Runtime evidence.

Blocked:

- [ ] HP 39g+ Direct runtime on a verified native stock flash fixture.
- [ ] HP 39g+ original-system launch on that fixture.

Direct runtime owns common-bootstrap execution and return correctness. Stock-launch gates own calculator-side package/menu/launcher integration.

## M7 — Application-facing calculator services

**In progress.**

M7 is where hpclang becomes a practical calculator SDK rather than only a validated native execution environment.

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

**Implementation and evidence closure complete on `m7-fonts-gallery`; integration to `main` remains a separate repository step.**

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
- [x] formal production `E2E / Font Gallery` run **35255868206 / #99** is fully green across Build, universal x50ng, all three Direct Gallery families, and Stock hp39gs/hp40gs/hp48gii/hp50g;
- [x] HP50g Stock validates live native minifont/main font plus generated control and reaches settled PASS;
- [x] HP48gII Stock validates distinct nonblank live native minifont/main font plus generated control and reaches HPC1 PASS, control-flow return, and settled Stock repaint; Gallery #102 also passes the post-cleanup strict HP48 gate;
- [x] HP48 and HP50 Stock summaries use the same compact three-panel structure and were visually checked from the #99 artifacts;
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
- [x] close the production Gallery matrix (`E2E / Font Gallery` #99) and retire the obsolete HP48 post-PASS exception;
- [ ] integrate the completed font slice to `main` after repository-level review.

### M7.2 — Storage and standard I/O

**hp4950 byte-stream stdio complete and merged to `main`.**

Storage is split by **semantics**, not by a misleading yes/no "filesystem" capability.

- [x] **Byte-stream filesystem:** HP49g+/HP50g use private KOS-backed `open`, `close`, `read`, `write`, `lseek`, and `unlink` hooks below picolibc. Public consumers use normal `FILE *` APIs. Stock validation covers read, create/truncate, write/flush, nonzero seek/ftell, reread, close/reopen, remove/ENOENT, and clean ENOSYS on non-stream families.
- [ ] **Calculator object store:** HP39gs/40gs stock storage contains named directory, Aplet/Zaplet, Note, Program, Variable, List, Matrix, Library and related object types, with `HP39DIR.000` as the root object-store file and `HP39DIR.CUR` as the current-directory pointer. This is not POSIX byte-stream storage and must not be forced through `FILE *`.
- [ ] **RPL/Saturn objects:** HP48/49/50 expose calculator-object namespaces whose typed object/stack integration remains separate from byte-stream stdio.

### M7.3 — Calculator objects and stack integration

**Planned.**

- [ ] design the HP39 typed object-store API around native object semantics;
- [ ] design HP48/49/50 Saturn/RPL object exchange without pretending objects are files;
- [ ] add stack integration where applicable;
- [ ] keep platform-specific representation below a coherent common capability/API boundary.

### Historical HPGCC parity is not complete

The old HPGCC SDKs included substantial calculator-facing functionality beyond merely entering C code: screen/graphics, keyboard, file I/O, Saturn/RPL integration, OS/syscall access, and higher-level helper libraries. hpclang has not yet reimplemented that breadth. M0–M7 service progress therefore must not be reported as "HPGCC feature complete".

## M8 — Practical SDK and porting layer

**Planned.**

HPG source compatibility is an explicit M8 goal, not an accidental side effect. The common 59-function `hpgraphics.h` surface is signature-stable across HPGCC 1.1, HPGCC 2.0 and HPGCC3, so hpclang should provide a broad source-compatible HPG adapter once its clean display/text primitives are stable. This does **not** make HPGCC binary compatibility a goal.

After the common services exist:

- establish stable application-facing headers;
- provide examples that exercise graphics, input, timers, storage, and calculator-object integration;
- validate non-trivial applications on each platform family;
- document source-porting guidance from common HPGCC 1.x/2.x APIs where useful;
- decide which compatibility wrappers are worth maintaining;
- establish a stable packaging/build UX from a fresh LLVM-only checkout;
- consider replacing the remaining ARMToolbox/library-275 stock-launch boundary with a minimal independent launcher if it materially improves packaging or portability;
- separate public application documentation from this internal Development reference.
