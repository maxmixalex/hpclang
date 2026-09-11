# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/zyf722/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Scope of the completed milestones

M0–M6 describe the **execution foundation**: compiler/runtime viability, the
HPC1 ABI, calculator launch paths, stock-system return, packaging, fixtures,
and automated E2E.

They do **not** mean that hpclang already reproduces the application-facing
feature set of HPGCC 1.x, HPGCC 2.x, or HPGCC3. M7 now provides stable device
facts, monotonic time, the complete five-model physical keyboard/event core,
opaque 2/4/16-level software surfaces, geometry, a validated text renderer,
and hp4950 stream stdio through picolibc. The active font-package work is
closing reusable font selection/import semantics above that renderer;
calculator-object/RPL storage and higher-level UI remain application-facing SDK
work.

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

Passing:

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
implemented and validated on the current five-model emulator matrix. HP39g+ key
identity remains UNKNOWN until its blocked real-machine fixture exists.

The display clean-core boundary is intentionally explicit: representation-
independent circles/polygons and owned 2/4/16-level surfaces are supported;
historical mutable clip/style state, PAINT/XOR, patterns, annunciator slots,
XPM convenience loaders, and raw framebuffer-pointer behavior remain
compatibility/future-use work. Physical gray4/gray16 and page flipping remain
deferred until a platform resource boundary can supply contiguous LCD DMA
memory without permanently taxing every HPC1 heap.

#### Font package/gallery closure

The old provisional Basic-Latin built-in tables are no longer the architectural
endpoint. `m7-fonts-gallery` now contains the canonical source bundle and a
repository-maintenance generator that produces build-time-ready assembly assets.
Normal application builds consume the checked-in generated catalog and do not
run Python or parse source fonts.

Current evidence:

- [x] Git LFS materialization of the canonical source bundle in CI;
- [x] BDF, TTF, FON, and newRPL BMP+TXT source parsing;
- [x] complete batch generation of **308 faces / 1,779,042 glyph mappings / 3,797 assembly sources**;
- [x] large faces split into bounded assembly chunks;
- [x] individual glyph/bitmap ELF sections remain independently eligible for linker garbage collection;
- [x] generated catalog is checked into `m7-fonts-gallery`.

The import contract has **three distinct modes**:

- [ ] `STATIC_AUTO` — derive the retained character repertoire only from string literals explicitly marked with `UI_TEXT`. Ordinary C/C++ string literals, diagnostics, paths, format strings, metadata, and unrelated data must not be scanned implicitly.
- [ ] `STATIC_EXPLICIT` — retain exactly the caller-supplied charset/codepoints (plus any explicitly selected fallback required by the font contract). No automatic source-text discovery occurs.
- [ ] `FULL` — retain the selected face's complete mapped repertoire.

`STATIC_AUTO` and `STATIC_EXPLICIT` are two different ways to define a static
repertoire; they must not collapse back into one ambiguous `STATIC` mode.
`UI_TEXT` is explicit build metadata for the automatic mode, not a replacement
for normal string literals and not a runtime font type.

UI font choice is a separate semantic axis from import mode. The common UI roles
are:

- `mini` — compact/proportional titles, soft menus, and small UI text;
- `text` — normal body/editor text (the primary general UI text role);
- `edit-large` — the larger fixed-cell EditLine/cursor-underlay role.

These roles are aliases/policy. The underlying bitmap-font ABI continues to
support arbitrary metrics, arbitrary generated faces, and explicit custom font
selection; it is not restricted to three physical fonts.

M7.1 font closure requires:

- [ ] implement the three modes end to end in CMake/build tooling;
- [ ] implement the `UI_TEXT` annotation/metadata extraction used by `STATIC_AUTO`;
- [ ] add deterministic tests proving unmarked string literals do not affect `STATIC_AUTO` output;
- [ ] validate `STATIC_EXPLICIT` subset retention and `FULL` complete-face retention;
- [ ] expose/freeze `mini` / `text` / `edit-large` semantic selectors while preserving general face selection;
- [ ] update API, licensing, HPGCC-compatibility, validation, and user-facing font documentation;
- [ ] run the appropriate ARM/Core and calculator E2E gates, then merge the completed slice to `main`.

### M7.2 — Storage and standard I/O

**hp4950 byte-stream stdio complete and merged to `main`.**

Storage is split by **semantics**, not by a misleading yes/no "filesystem"
capability.

- [x] **Byte-stream filesystem:** HP49g+/HP50g use private KOS-backed `open`, `close`, `read`, `write`, `lseek`, and `unlink` hooks below picolibc. Public consumers use normal `FILE *` APIs. Stock validation covers read, create/truncate, write/flush, nonzero seek/ftell, reread, close/reopen, remove/ENOENT, and clean ENOSYS on non-stream families.
- [ ] **Calculator object store:** HP39gs/40gs are not storage-less. Their stock object-store includes named directory, Aplet/Zaplet, Note, Program, Variable, List, Matrix, Library and related object types, with `HP39DIR.000` as the root object-store file and `HP39DIR.CUR` as the current-directory pointer. This is not POSIX byte-stream storage and must not be forced through `FILE *`.
- [ ] **RPL/Saturn objects:** HP48/49/50 expose calculator-object namespaces whose typed object/stack integration remains separate from byte-stream stdio.

The public model therefore distinguishes stream storage from calculator
objects. Physical media (RAM/flash/SD) is backend detail unless an application
genuinely needs it.

### M7.3 — Calculator objects and stack integration

**Planned.**

- [ ] design the HP39 typed object-store API around native object semantics;
- [ ] design HP48/49/50 Saturn/RPL object exchange without pretending objects are files;
- [ ] add stack integration where applicable;
- [ ] keep platform-specific representation below a coherent common capability/API boundary.

### Historical HPGCC parity is not complete

The old HPGCC SDKs included or supported substantial calculator-facing
functionality beyond merely entering C code: screen/graphics, keyboard,
file I/O, Saturn/RPL integration, OS/syscall access, and higher-level helper
libraries. hpclang has not yet reimplemented that breadth.

Therefore M0–M6 completion must not be reported as "HPGCC 1.x/2.x compatible"
or "HPGCC feature complete".

## M8 — Practical SDK and porting layer

**Planned.**

HPG source compatibility is an explicit M8 goal, not an accidental side effect.
The common 59-function `hpgraphics.h` surface is signature-stable across
HPGCC 1.1, HPGCC 2.0 and HPGCC3, so hpclang should provide a broad
source-compatible HPG adapter once its clean display/text primitives are
stable. This does **not** make HPGCC binary compatibility a goal, and raw GGL,
historical framebuffer pointers, physical grayscale/page-flip ownership and
other representation-specific behavior remain separately gated.

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

HPGCC3 compatibility is also a separate question: HPGCC3 uses a revisioned
library environment tied to a custom calculator ROM. hpclang currently follows
the opposite non-ROM design and does not implement that ROMLIB ABI.

C++ exceptions/RTTI, broader POSIX/libc hooks, advanced graphics/UI libraries,
and additional compatibility APIs remain optional until a real application
requires them.

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

Only at this stage should hpclang be described as a broadly usable replacement
SDK rather than a completed execution foundation.

## Unified test model

Repository validation has two source-test layers:

- `tests/unit/` contains host-side `test_*.c` tests for pure, hardware-independent implementation logic;
- `tests/e2e/` is one calculator-side hpclang consumer project. The same source project is built into HPC1 and launched through each supported target and stock/direct harness.

Target profiles, stock UI sequences, and x50ng frontends are harness data, not
separate test applications. New public SDK functionality should be added to the
unified E2E application whenever it is calculator-testable, with a separate
unit test only where pure implementation logic benefits from isolation.

The CI implementation uses a family-level E2E pipeline: build one HPC1
application per test mode, then run one job per platform family (`hp39`, `hp48`,
or `hp4950`). Each family job prepares x50ng and runtime dependencies once and
executes its automated sibling targets in parallel. HP 39g+ is explicitly
excluded by target-profile metadata until its real stock fixture exists.
Behavioral expectations remain in E2E specs and harness code rather than
GitHub Actions YAML.
