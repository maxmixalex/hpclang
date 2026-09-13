# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/zyf722/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Scope of the completed milestones

M0–M6 describe the **execution foundation**: compiler/runtime viability, the
HPC1 ABI, calculator launch paths, stock-system return, packaging, fixtures,
and automated E2E.

They do **not** mean that hpclang already reproduces the application-facing
feature set of HPGCC 1.x, HPGCC 2.x, or HPGCC3. M7 now provides stable device
facts, monotonic time, the complete five-model physical keyboard/event core,
opaque 2/4/16-level software surfaces, geometry, native/generated font and text
rendering, and hp4950 stream stdio through picolibc. Calculator-object/RPL
storage and higher-level UI remain application-facing SDK work.

hpclang is also not intended to be binary-compatible with historical HPGCC
executables or to reproduce HPGCC3's custom-ROM/ROMLIB environment.

## Principles

- one application ABI: HPC1 / hpclang v1;
- one common ARMv4T runtime;
- thin platform implementations: `hp39`, `hp48`, and `hp4950`;
- no required GCC, historical HPGCC runtime, or ROM replacement;
- historical code is research evidence, not a compatibility mode;
- emulator fixtures are generated or prepared from verified inputs rather than committed as opaque calculator-state snapshots;
- model-specific differences remain metadata or platform code unless they genuinely require a different implementation;
- stock-hosted backends preserve host hardware ownership where practical;
  freestanding implementations may own IRQs, timers, clocks and peripherals
  without changing the application-facing service APIs;
- downstream firmware, operating systems, and applications remain consumers:
  they must not become named hpclang targets, ABI branches, or dependencies.

## M0 — Unified application ABI

**Complete.**

HPC1, AAPCS32, Thumb-1/ARMv4T, soft float, and the fixed virtual layout are the single supported application contract.

Existing model IDs 1–6 are stable ABI values.

## M1 — Modern language/runtime stack

**Complete.**

LLVM compiler builtins, ARMv4T-safe integer and soft-float helpers, picolibc integration, malloc, static TLS, constructors, and normal C/C++ linkage are in the common runtime.

Historical GCC code generation, libgcc, r9/r10 static-base conventions, and legacy HPGCC runtime objects are not normal-build dependencies.

## M2 — Common ARM bootstrap

**Complete for the validated platforms.**

The common bootstrap installs the HPC1 mappings, enters the application, restores temporary state, and returns to the calculator OS.

The common code coarse table also has a launcher/runtime-private prefix before `HPCLANG_CODE_BASE`. Platform code may use that prefix for narrowly scoped small-page mappings without borrowing arbitrary stock L1 entries; this does not change the HPC1 application ABI. HP39gs/40gs native-font access is the first production user of that mechanism, mapping only two uncached stock-NOR pages below public code.

HP 39gs real hardware plus the current emulator gates validate the design. HP 39g+, HP 39gs, and HP 40gs share the `hp39` platform; HP 48gII uses `hp48`; HP 49g+ and HP 50g share `hp4950`.

## M3 — HP39 platform

**Runtime and automated stock-launch validation complete for HP 39gs/40gs. HP 39g+ blocked on real hardware fixture.**

- [x] HP 39gs real-hardware canonical runtime PASS and clean return.
- [x] HP 39gs official-ROM Direct E2E.
- [x] HP 40gs official-ROM Direct E2E.
- [x] HP 39gs/40gs hardware-validation packages.
- [x] HP 39g+ model-1 hardware-validation package using the shared HPGCC39 APLET/ARMToolbox39 boundary.
- [x] Exercise stock serial-disk transfer of `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000` through the original HP39gs/40gs ROM.
- [x] Drive stock APLET → HPCLANG → ARMToolbox39 → HPC1 PASS → stock return on HP 39gs and HP 40gs.
- [x] Validate a read-only HP39 flash-dump mechanism byte-for-byte against emulated physical NOR.
- [ ] Acquire a real HP 39g+ and dump its complete stock flash.
- [ ] Add the verified HP 39g+ flash fixture to Direct runtime E2E.
- [ ] Automate the original-system HP 39g+ install/launch path available on real stock firmware.

The HP 39g+ items are **blocked**, not active research tasks. Do not substitute a sibling or foreign bootloader and call it native HP39g+ validation.

The retained HP39g+ NOR dumper validates a transient physical-flash mapping mechanism; it does not establish that HP39g+ shares HP39gs/40gs firmware identity or native-font offsets.

The preferred fixture path uses the stock ROM's own transfer/install logic. Direct SRAM/flash patching is a fallback only. The obsolete Conn3x GUI itself is not part of the runtime regression contract.

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

Completion criterion:

> HP49g+ and HP50g use the same platform implementation and HPC1 ABI, with only genuine model differences below the common bootstrap.

## M6 — Automated emulator E2E

**Complete for five models; HP 39g+ blocked.**

Passing durable evidence exists for:

- HP 39gs Direct runtime;
- HP 40gs Direct runtime;
- HP 48gII Direct runtime on the verified full-NOR fixture;
- HP 49g+ Direct runtime with its native ARM bootloader;
- HP 50g Direct runtime with its native ARM bootloader;
- HP 39gs full stock serial-disk/APLET launch;
- HP 40gs full stock serial-disk/APLET launch;
- HP 48gII full stock Kermit/ARMToolbox48/`PrRUN` launch;
- HP 49g+ full stock ARMToolbox/`PrRUN` launch;
- HP 50g full stock ARMToolbox/`PrRUN` launch.

This is durable path evidence, not a claim that every current workflow attempt is green. Current Stock harness stabilization is tracked separately from runtime correctness: HP39 serial-transfer duration and host/calculator keyboard-clock synchronization, plus HP48 visible post-return repaint, are bounded harness concerns under active regression. Direct remains the blocking common runtime/keyboard correctness gate while those Stock orchestration gates are repaired.

Blocked:

- [ ] HP 39g+ Direct runtime on a verified native stock flash fixture.
- [ ] HP 39g+ original-system launch on that fixture.

Direct runtime owns common-bootstrap execution and return correctness. Stock-launch gates own calculator-side package/menu/launcher integration. Host transfer tools are included only when they can be exercised through a maintained deterministic transport.

## M7 — Application-facing calculator services

**In progress.**

M7 is where hpclang becomes a practical calculator SDK rather than only a
validated native execution environment.

### M7.0 — API inventory and compatibility design

- [x] inventory the public HPGCC 1.0.3/1.1 and 2.0 headers by functional area from complete distribution archives;
- [x] inventory the relevant HPGCC3 public interfaces separately;
- [x] classify major historical API families as direct source-compatible, compatibility-adapter, migration-only, or intentionally unsupported;
- [x] publish a progress-oriented API reference with per-service validation and HPGCC 1.0.3 / 1.1 / 2.0 SP2 / HPGCC3 comparison;
- [x] define the C ABI boundary and C++ wrapper policy;
- [x] establish common conventions through the implemented device/time/keyboard/display/storage slices;
- [ ] finish conventions only when remaining object/UI services expose genuinely new ownership, error, string/buffer, or capability-query requirements.

See [SDK API design](sdk-design.md).

### M7.1 — Common services, display, keyboard, and fonts

Device information, stock-preserving monotonic time, keyboard scanning/events,
physical key identity, the C++23 wrappers, display geometry, opaque software
surfaces, blitting, text rendering, and LCD ownership preservation are all
implemented and validated on the current five-model Direct matrix. HP39g+ key
identity remains UNKNOWN until its blocked real-machine fixture exists.

The display clean-core boundary is intentionally explicit: representation-
independent circles/polygons and owned 2/4/16-level surfaces are supported;
historical mutable clip/style state, PAINT/XOR, patterns, annunciator slots,
XPM convenience loaders, and raw framebuffer-pointer behavior remain
compatibility/future-use work. Physical gray4/gray16 and page flipping remain
deferred until a platform resource boundary can supply contiguous LCD DMA
memory without permanently taxing every HPC1 heap.

#### Font package/gallery closure

The font system has two intentionally separate sources:

- **calculator-native fonts** are borrowed stock assets exposed by `hpclang_font_default()`, `hpclang_font_mini()`, and `hpclang_font_big()`; no HP glyph bitmap is redistributed;
- **generated custom fonts** come from the canonical source bundle and checked-in generated catalog and are selected per target with `hpclang_target_font()`.

The native implementation is platform-specific below the common API. HP39/40 uses fixed physical stock-ROM raster tables, while HP48gII/49g+/50g resolves stock native font objects through the live logical Saturn `Read[]` mapping. Historical HPGCC source independently supports that family split: its HP48-compatible native helpers use `sat_map_s2a()` / `Read[]`, whereas the HP39 Aplet package binds HP39 stock `Put3x5` / `Put5x7` ROM entries. The exact HP39 physical raster bases are established by stock-ROM reverse engineering, not copied from an HPGCC constant.

HP39gs/40gs now expose those fixed rasters through two uncached 4 KiB mappings in hpclang's own bootstrap-private code-L2 prefix: physical NOR pages `0x00042000` and `0x00043000` appear at the private window beginning `0x01200000`; public HPC1 code still begins at `0x01220000`. This replaced a long-lived high-L1 section alias after diagnostics proved that the active LCD framebuffer itself overlaps the physical L1-table region and can overwrite the selected descriptor word during Gallery drawing. `platform_cookie` carries the private window base only as an opaque launcher/runtime token, so no application ABI was added.

Current evidence:

- [x] Git LFS materialization of the canonical source bundle in CI;
- [x] BDF, TTF, FON, and newRPL BMP+TXT source parsing;
- [x] complete batch generation of **275 faces / 1,772,082 glyph mappings**;
- [x] large faces split into bounded assembly chunks;
- [x] individual glyph/bitmap ELF sections remain independently eligible for linker garbage collection;
- [x] generated catalog is checked into `m7-fonts-gallery`;
- [x] HP39 mini/main physical-ROM reader validated on HP39gs/40gs Direct screenshots and HP40gs Stock screenshot;
- [x] long-lived HP39 high-L1 alias failure root cause proven: framebuffer bytes overlapped the active L1 descriptor storage; production code uses the owned two-page L2 window instead;
- [x] Core font-selection/window/MMU tests, ARM hardware-validation build, and HP39 package build pass after the mapping change;
- [x] targeted HP39 exhaustive Gallery traverses **277 entries / 554 captures** on both HP39gs and HP40gs with PASS and settled return;
- [x] exhaustive Direct Font Gallery traverses **277 entries (275 generated faces plus two native entries) and both SAMPLE/TEXT pages, 554 captures per target**, on HP39gs, HP40gs, HP48gII, HP49g+, and HP50g; all five reach PASS and settled return;
- [x] compact HP50g Stock Gallery validates live Saturn `Read[]` native mini/main fonts, a representative generated face, the original UI/ARMToolbox launch path, PASS, and settled stock return;
- [x] formal production `E2E / Font Gallery` run **34743437279 / #47 (attempt 2)** is fully green across Build, universal x50ng, `Gallery / hp39`, `Gallery / hp48`, `Gallery / hp4950`, and `Stock Gallery / hp50g`.

The import contract has **three distinct modes**:

- [x] `STATIC_AUTO` — derive the retained character repertoire only from string literals explicitly marked with `HPCLANG_UI_TEXT`. Ordinary C/C++ string literals, diagnostics, paths, format strings, metadata, comments, and fake marker text are not scanned implicitly.
- [x] `STATIC_EXPLICIT` — retain exactly the caller-supplied charset/codepoints plus the configured fallback. No automatic source-text discovery occurs.
- [x] `FULL` — retain the selected face's complete mapped repertoire.

`STATIC_AUTO` and `STATIC_EXPLICIT` are two different ways to define a static repertoire; they do not collapse into one ambiguous `STATIC` mode. `HPCLANG_UI_TEXT` is explicit build metadata and remains an ordinary string at runtime.

Generated-font convenience aliases are currently `@mini = newrpl_6a`, `@normal = newrpl_8c`, and `@large = newrpl_10a`. Arbitrary generated face IDs and metrics remain supported. These build aliases are separate from the calculator-native `hpclang_font_*()` functions.

There is deliberately **no public semantic font-role layer** in the font core. Application developers select calculator-native or generated fonts directly through the font APIs. Names such as `mini`, `text`, or `edit-large` may be useful to a future UI toolkit/theme layer, but they do not belong in the core font ABI.

Direct Gallery's synthetic launcher fixture intentionally does not fabricate a live Saturn `Read[]` map. Therefore generated-font exhaustive rendering is valid there on all five models, HP39/40 native fonts are also valid because they use verified physical ROM rasters, and HP48/49/50 native-font evidence must come from a real Stock Saturn mapping. HP50g Stock closes the hp4950 native path; compact HP48gII Stock Gallery now closes the 64-line live-native evidence item, including nonblank/distinct native probes and a visible stock return.

The experimental launcher-private `HPZ1`/LZ4 staging format is rejected. It could compress the exhaustive Gallery below the legacy wrapper's object-size limit, but it did not solve the real Stock memory constraint and it regressed hp4950 stage1 launch. Standard HPC1 remains the sole application payload contract; exhaustive Gallery and compact Stock Gallery instead test the concerns they actually own.

M7.1 font closure now requires:

- [x] implement the three repertoire modes end to end in CMake/build tooling;
- [x] implement `HPCLANG_UI_TEXT` extraction used by `STATIC_AUTO`;
- [x] prove ordinary/unmarked literals do not affect `STATIC_AUTO` output;
- [x] validate `STATIC_EXPLICIT` subset retention and `FULL` complete-face retention in Core CI;
- [x] separate calculator-native assets from generated custom/font-package assets;
- [x] validate the corrected HP39 physical-ROM native reader through the owned two-page L2 window;
- [x] build Font Gallery as a separate HPC1 E2E application and exhaustively render all 275 generated faces across the five automated models and both 64/80-line layouts;
- [x] keep the developer-facing font API direct; do not add public `mini` / `text` / `edit-large` semantic role selectors;
- [x] validate the compact standard-HPC1 Stock Gallery on HP50g with live native fonts and one generated control face;
- [x] close the formal production Gallery matrix after the HP39 mapping fix (`E2E / Font Gallery` #42);
- [ ] add compact HP48gII Stock Gallery evidence for the 64-line live Saturn native-font path;
- [ ] merge the completed font slice to `main` after the final HP48 Stock Gallery and documentation/evidence review.

### M7.2 — Storage and standard I/O

**hp4950 byte-stream stdio complete and merged to `main`.**

Storage is split by **semantics**, not by a misleading yes/no "filesystem"
capability.

- [x] **Byte-stream filesystem:** HP49g+/HP50g use private KOS-backed `open`, `close`, `read`, `write`, `lseek`, and `unlink` hooks below picolibc. Public consumers use normal `FILE *` APIs. Stock validation covers read, create/truncate, write/flush, nonzero seek/ftell, reread, close/reopen, remove/ENOENT, and clean ENOSYS on non-stream families.
- [ ] **Calculator object store:** HP39gs/40gs are not storage-less. Their stock object-store includes named directory, Aplet/Zaplet, Note, Program, Variable, List, Matrix, Library and related object types, with `HP39DIR.000` as the root object-store file and `HP39DIR.CUR` as the current-directory pointer. This is not POSIX byte-stream storage and must not be forced through `FILE *`.
- [ ] **RPL/Saturn objects:** HP48/49/50 expose calculator-object namespaces whose typed object/stack integration remains separate from byte-stream stdio.

The public model therefore distinguishes stream storage from calculator objects. Physical media (RAM/flash/SD) is backend detail unless an application genuinely needs it.

### M7.3 — Calculator objects and stack integration

**Planned.**

- [ ] design the HP39 typed object-store API around native object semantics;
- [ ] design HP48/49/50 Saturn/RPL object exchange without pretending objects are files;
- [ ] add stack integration where applicable;
- [ ] keep platform-specific representation below a coherent common capability/API boundary.

### Historical HPGCC parity is not complete

The old HPGCC SDKs included or supported substantial calculator-facing functionality beyond merely entering C code: screen/graphics, keyboard, file I/O, Saturn/RPL integration, OS/syscall access, and higher-level helper libraries. hpclang has not yet reimplemented that breadth.

Therefore M0–M6 completion must not be reported as "HPGCC 1.x/2.x compatible" or "HPGCC feature complete".

## M8 — Practical SDK and porting layer

**Planned.**

HPG source compatibility is an explicit M8 goal, not an accidental side effect. The common 59-function `hpgraphics.h` surface is signature-stable across HPGCC 1.1, HPGCC 2.0 and HPGCC3, so hpclang should provide a broad source-compatible HPG adapter once its clean display/text primitives are stable. This does **not** make HPGCC binary compatibility a goal, and raw GGL, historical framebuffer pointers, physical grayscale/page-flip ownership and other representation-specific behavior remain separately gated.

After the common services exist:

- establish stable application-facing headers;
- provide examples that exercise graphics, input, timers, storage, and calculator-object integration;
- validate one or more non-trivial applications on each platform family;
- document source-porting guidance from common HPGCC 1.x/2.x APIs where useful;
- decide which compatibility wrappers are worth maintaining;
- establish a stable packaging/build UX from a fresh LLVM-only checkout;
- replace the remaining historical ARMToolbox/library-275 stock-launch boundary with a minimal independently implemented launcher if doing so materially improves packaging or portability; this is not a prerequisite for HPC1 or the M7 service APIs;
- separate public application documentation from this internal Development reference.

HPGCC binary compatibility is not a goal.

HPGCC3 compatibility is also a separate question: HPGCC3 uses a revisioned library environment tied to a custom calculator ROM. hpclang currently follows the opposite non-ROM design and does not implement that ROMLIB ABI.

C++ exceptions/RTTI, broader POSIX/libc hooks, advanced graphics/UI libraries, and additional compatibility APIs remain optional until a real application requires them.

## M9 — SDK stabilization

**Planned.**

Completion requires:

- all three platform families to execute HPC1 through a supported path;
- representative emulator coverage plus primary real-hardware coverage;
- HP 39g+ either validated or explicitly documented as the remaining external hardware blocker;
- the M7 calculator-service baseline to be usable by real applications;
- stable public headers and packaging;
- representative applications passing on all supported platform families;
- coherent application-facing documentation and porting guidance.

Only at this stage should hpclang be described as a broadly usable replacement SDK rather than a completed execution foundation.

## Test model

Repository validation has two source-test layers:

- `tests/unit/` contains host-side `test_*.c` tests for pure, hardware-independent implementation logic;
- `tests/e2e/` contains **focused calculator-side HPC1 applications plus shared harness infrastructure**. It is not required to remain one monolithic application.

The canonical **runtime** E2E application owns ABI/runtime/device/time/display/keyboard/storage regression coverage and is launched through the supported Direct and Stock harnesses. Visual/demo-heavy or independently scoped features should use separate E2E applications when that keeps the runtime gate deterministic. Font Gallery is the first explicit example: it gets its own executable/target and evidence path rather than adding pages to the runtime payload.

Font Gallery intentionally separates exhaustive asset/rendering coverage from Stock integration coverage. The exhaustive Direct payload walks all generated faces and both pages; compact Stock payloads keep the normal standard-HPC1/ARMToolbox resource envelope and validate live calculator-native assets plus a representative generated face. A synthetic Direct fixture must not fabricate Saturn mapping state merely to make native-font screenshots look plausible. The production Gallery matrix is independently gated and its current formal closure is run `34698825657` / #42.

Target profiles, stock UI sequences, x50ng frontends, and common reporting remain shared harness data. A focused E2E app should reuse that infrastructure rather than create a parallel emulator framework.

The CI implementation remains family-oriented where practical: build an HPC1 application once, then run one job per platform family (`hp39`, `hp48`, or `hp4950`). Each family job prepares x50ng and runtime dependencies once and executes its automated sibling targets. HP 39g+ is explicitly excluded by target-profile metadata until its real stock fixture exists. Behavioral expectations remain in E2E specs/app code and harness code rather than GitHub Actions YAML.


### Curated font package evidence (2026-09-13)

The canonical package was curated after visual and semantic audit: six Dina OEM faces, 21 Powerline-specific faces, and six faces whose `Quick` specimen exceeded the 131-pixel LCD were removed at the source-package level and the generated catalog was rebuilt. The current catalog contains **275 generated faces / 1,772,082 mapped glyphs**. Semantic source-to-generated parity is **275/275**; exact duplicate, `quick-overflow`, `glyph-collapse`, `blank-heavy`, and Powerline signals are all zero. Six retained faces still have source/cmap-level printable-ASCII gaps and 13 have loose-spacing diagnostics; neither is treated as an automatic defect.

The TTF rasterizer now uses Pillow mask geometry rather than incorrectly assuming `getbbox()` dimensions match `getmask2()` storage, which eliminated the importer-created blank-heavy glyphs. Ark Pixel Gallery labels expose size, monospaced/proportional mode, and locale explicitly. The human review report contains 423 embedded specimens; Ark locale panels show only differing characters from the curated common-character list and never pad the panel with obscure CJK. Retained TTF families still require the final human curation decision before the font slice is merged.

The curated exhaustive Direct Gallery is **277 entries (275 generated + 2 native) × 2 pages = 554 captures per target** on HP39gs, HP40gs, HP48gII, HP49g+, and HP50g, all PASS. `E2E / Font Gallery` run **34743437279 / #47, attempt 2** is fully green, including `Stock / hp48` and `Stock / hp4950`. The first HP50g Stock attempt timed out before HPC1 PASS; the rerun reached the same ARMToolbox path, HPC1 PASS, nonblank native mini/main/generated control captures, and final PASS, so the first result is retained as a Stock-harness timing fluctuation rather than hidden.
