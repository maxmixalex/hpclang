# Architecture

hpclang is a clean LLVM-native SDK for the ARM-based HP graphing calculators. It is not a selectable collection of historical HPGCC runtimes.

## Supported build model

A normal application uses:

```text
Clang / LLVM / LLD
ARMv4T
AAPCS32
Thumb-1 C/C++
soft float
fixed virtual code and writable-memory addresses
HPC1 logical executable
```

The normal build has no dependency on GCC, libgcc, historical HPGCC object files, or a replacement calculator ROM.

## Layers

```text
application
    |
hpclang public C/C++ API
    |
common runtime + libc + compiler builtins
    |
hpclang v1 boot/service boundary
    |
+-----------+-----------+-----------+
|           |           |           |
hp39        hp48        hp4950
|           |           |
39g+        48gII       49g+
39gs                    50g
40gs
    |
calculator OS / ARMSYS / hardware
```

The common runtime owns language/runtime concerns: startup, initialized data and BSS, static TLS, constructors, compiler helpers, heap management, libc primitives, entry into `main()`, and cleanup on return.

A platform implementation owns calculator-specific work: native packaging, physical-memory discovery, Saturn-to-ARM transition, MMU setup, boot-info construction, state restoration, stock-system integration, and low-level services.

## Independent Stock launcher evaluation

An owned launcher could remove the user's separate library-275 installation
step. This is a concrete packaging benefit, especially for a first application;
the existing library is shared, so its size is not a per-application cost.
The application ABI and model-specific HPC1 builds would stay the same.

The current transfer baseline has three different boundaries:

| Family | Current external support | What independence would need to replace |
| --- | --- | --- |
| HP39g+/39gs/40gs | Aplet support library and external Aplet packaging assets/tool | ARM entry and allocation support, while preserving Aplet Start/Reset and custom-data persistence |
| HP48gII | ARMToolbox48 library 275 | Stock-to-ARM entry, page-pool acquisition and matching release |
| HP49g+/50g | ARMToolbox setup/library support | Stock-to-ARM entry, page-pool acquisition and matching release |

For scale, the 2026-11-04 maintained Notebook entry packages contain a
1,489-byte HP39gs `LIB275L.000`, a 2,404-byte HP48gII `L275_48`, and a
7,673-byte HP49g+ `SETUP.BIN`. These are measured transfer artifacts, including
their respective envelopes/installers, not comparable library bodies or
resident-memory measurements. The bounded prototype below is not an equivalent
Notebook package; these numbers do not establish a net size or RAM saving.

Replacing the outer entry is insufficient. `hpclang_armtoolbox_stage1` currently
consumes a preallocated page pool and coarse L2 table. Its return path also
depends on captured Stock state, protected stage0 storage, relocation roots and
main/Port1 allocation fences. An independent launcher must supply equivalent
contracts and retain cleanup on rejected startup, ordinary exit and application
failure. It would still depend on model-specific original-ROM entry and return
behavior; removing a third-party library does not remove that portability work.
The synthetic Direct path alone cannot qualify a Stock replacement.

The packaging benefit warrants retaining this option. A useful comparison is
the same Notebook application launched from a clean Stock state without
library 275, measuring transferred files, resident and peak installation RAM,
startup cost and settled return against the current packages. HP39 additionally
needs save/relaunch and copied-Aplet isolation checks. A prototype must preserve
the HPC1/application service contracts before replacing the current path.
The repository now includes a bounded HP49g+/HP50g prototype in
`tests/e2e/independent-hpc1`. It enters ARM from Stock RPL without library 275,
loads an embedded Thumb HPC1 image through the common bootstrap and returns to
Stock. The fixture documents model-specific native snapshot checks, including
unchanged Flash, restored MMU/control state and stack guards. Its isolated
build check runs in Core CI; build reproducibility alone does not establish
native execution. This bounded delivery is accepted through #43 / `38e9472a`.

The prototype supplies no native-service context and is not a general
application loader. HP39 Aplet integration, HP48gII entry and cross-family
ownership/coexistence remain evaluation work. The migration decision is to
retain the existing production launchers while evaluating the same Notebook
application and cleanup contracts described above. No production replacement
or retirement of the historical dependency is authorized by this prototype.

### Source-level family assessment

#### Migration decision after the full Notebook measurements (2026-11-19)

**Keep the installed SDK's current launchers.** Removing the separate support
installation would simplify first use. The HP50g fixture exercises a bounded
Notebook save and relaunch, but does not qualify a general replacement. Keep
the independent path experimental until the following gates pass. This
decision retains the migration work and does not close M8.6.

All page counts below use 4 KiB frames. “Minimum / editor” compares the
production packaging floor before heap with a budget derived from the observed
editor workload, including its heap reservation and full 8 KiB stack. It is
neither a worst-case requirement nor the cost of a different launcher.

| Family | Entry and page source | Stock services and exit | Next independent acceptance gate |
| --- | --- | --- | --- |
| HP39 (HP39g+ D1.55 measured) | Production Aplet: **25 / 27** pages. Its 37 frames include two native workspace pages. All 36 candidate RPL slots overlap these frames; the separate native block yields no aligned frame. Independent Aplet entry is unverified. | Production editor and ON return pass. Aplet data access depends on the action frame; no equivalent independent Start/Reset, data-slot or return contract is established. | From a clean Aplet install, enter without the support library; account for application and native workspace simultaneously. Verify save/relaunch, copied-Aplet isolation, Reset, rejected entry and settled return on all three HP39 profiles. |
| HP48 (HP48gII) | Production ARMToolbox48: **27 / 29** pages. A model-specific no-275 entry and RPL return pass locally. KOS supplied one aligned frame. HOME-backed storage moves on insertion, and Stock recomputes a lowered `AVMEM` before it can serve as a persistent fence. Neither approach qualified extra frames. SRAM mirrors add no frames; HP48gII has no Port 1 ERAM. | Production HOME save/retry, Port 0 and settled return have Stock evidence. Independent admission refused the 29-frame request and released the KOS block. The two rejected reservations preserved HOME/Port 0 and stack values after cleanup. | A separate model-4 HPC1 probe now maps 29 aligned pages within its executing program on the 256 KiB machine. The unmapped reservation occupies a single copy in the transfer object. Exercise HOME/Port 0 with live roots and ownership bounds, verify a successful full launch and qualify cleanup on partial assembly and entry failures. |
| HP49/50 | Both models run the model-specific Notebook without library 275 with all 20 code pages mapped read-only from Port 1. The normal independent entry uses nine aligned KOS pages, including its page tables. Both production packages require **32 / 35** pages for the minimum / observed editor workload. On HP49g+, the 79,168-byte image uses 20 code pages. The earlier observed editor used 13,660 bytes of heap reservation and touched 1,912 of 8,192 stack bytes. | Both original-ROM fixtures save to HOME, return status 1 with a 20 KiB RW budget, release and reacquire the KOS blocks, then reload the saved text on normal launch. A 35-page request also exercises partial allocation, release and simultaneous reacquisition of the same block sizes before Stock return. All four launches pass the GC and settled Stock checks. Stock recalls the distinct Port 0 backup written by the relaunched editor, with the Port 1 code record intact. The editor also exports to a FAT16 card. External readback checks the exported bytes and an existing file. | Exercise remaining native Port services under independent page ownership. |

**Promotion evidence.** For each candidate run, record the owner, physical
extent and lifetime of every frame; exclude aliases and Stock reservations.
Demonstrate disjoint storage for code, RW, stage, MMU and required native
workspace, with model-specific service fences and relocation roots. On rejection,
normal exit and application failure, verify restored mappings, stack, control
state and responsive Stock; verify page release or documented restoration
before reuse. Compare first-install files/bytes, shared vs per-app dependencies,
peak resident RAM and startup time against the same Notebook build. Require
six-model installed-SDK packaging and supported Stock workflows before proposing
retirement. A successful probe build or a zero-extra-frame observation alone
does not decide the outcome for a new layout.

The [roadmap measurements](roadmap.md#current-work-balanced-m8-delivery) name
the local per-frame reports and distinguish them from CI acceptance. The
`tests/e2e/independent-hpc1` and `tests/e2e/independent-pool` fixture READMEs
identify the independent execution and negative checks.

The current sources identify the following concrete replacement boundaries.
This is an implementation assessment, not additional native execution evidence.

| Family | Existing contract and consequence for an owned launcher |
| --- | --- |
| HP39g+/39gs/40gs | `runtime/core/hp39-aplet-frame.c` recognizes the Aplet `_Action` instruction sequence, including the library-275 `xPrRUN` ROMPTR, before exposing the custom-data slot. `tools/prepare-hp39-aplet.py` also installs Start/Reset views and restores the caller's program context around Reset. A new entry must provide an explicit equivalent Aplet data/return contract; replacing the ARM jump alone would leave persistence unavailable. Keep save/relaunch, copied-Aplet isolation and Reset in the comparison. |
| HP48gII | `platform/armtoolbox/stage0.S` consumes the launcher's pool count and L2 table, rejects insufficient space before changing mappings, and releases its optional KOS allocation on both rejection and return. The independent model-4 launcher maps all 29 pages and runs the installed SDK Notebook editor. A fresh Stock boot accepted keyboard input and returned on ON. The package stages the image inside its execution reservation. Native save, relocation during GC and persistent relaunch remain open. |
| HP49g+/HP50g | `tests/e2e/independent-hpc1/loader.c` supplies one code page and four RW pages from its fixed arena, but sets both `saturn_state` and `platform_cookie` to zero. It proves the common HPC1 bootstrap boundary, not the native object/port services used by Notebook. Supplying those services requires a captured Stock context and equivalent relocation/allocation boundaries before the application comparison is meaningful. |

Across families, `platform/armtoolbox/stage1.c` derives main/Port1 allocation
fences from the actual pool and publishes launcher and suspended-execution roots.
The replacement must preserve these supported service contracts, not copy a
fixture's fixed addresses into a general launcher. `stage0.S` restores the
borrowed mapping and original stack before restoring interrupt state and
releasing the optional KOS block; the independent path needs equivalent ordered
cleanup, including rejected startup.

Fresh unmodified Notebook builds on 2026-11-14 give the following sizing baseline
(ATFE 22.1.0, MinSizeRel, model-specific builds):

| Model | HPC1 code bytes | Static RW bytes | Minimum RW pages | Production minimum Stock pool pages | Production transfer bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| HP39g+ | 45,456 | 1,048 | 4 | 25 | 48,396 |
| HP48gII | 64,248 | 1,048 | 4 | 27 | 62,828 |
| HP50g | 79,152 | 4,320 | 5 | 32 | 74,620 |

Pages are 4 KiB. Minimum RW includes boot, static data and the 8 KiB stack;
it is not a measured heap budget. Transfer sizes include the production wrapper
and compressed payload, but exclude the separately installed support library
and HP39 Aplet envelope. These are build measurements, not execution or peak
Stock RAM measurements. Report: `/work/nov14-independent-notebook-budget/result.json`.

The bounded independent entry copies at most 65,536 bytes (`code_for`), and
its arena is serialized inside that copied blob. Even the HP39 image plus the
minimal 12 code + 4 RW + 1 MMU pages exceeds that copy limit, before entry code,
alignment and its separate stack. HP50 code alone exceeds it. Simply expanding
the fixture's one-code-page/four-RW-page constants cannot launch this application.
The first external-pool experiment is implemented in
`tests/e2e/independent-pool`. A fresh HP50g Stock run without library 275 obtains
8 and 16 aligned pages through KOS service 201 (heap `0x100`), writes/reads page
ends, and releases each block through service 101. Requests for 135,167 bytes
and 1 MiB fail normally; a subsequent small allocation reuses the first pointer.
Original interrupt/control state, SWI vector, code/stack guards, RPL return and
unchanged Flash pass the independent snapshot verifier. Ten corrupted results
are rejected. Report: `/work/nov14-independent-pool-live/verified.json`.
This is one baseline, not a universal KOS capacity bound. It does not provide the
32-page HP50g Notebook minimum, so the next step is an owned RPL-backed pool or
another demonstrated allocation source, with matching failure/return cleanup.
The follow-up `rpl-probe.c` now borrows and restores 28 main-RPL pages (112 KiB)
while Stock execution and interrupts remain paused. Each page is checked against
the saved native Read map, page-end contents are restored, and entry/return plus
ten corruption negatives pass. Its optional state argument leaves the default
entry package unchanged. This small-entry baseline does not establish combined
KOS/RPL availability with the full Notebook resident, or a lease across native
allocation/GC. Report: `/work/nov14-independent-rpl-pool-live/verified.json`. Then
connect the captured Stock context and service fences to a service-capable
HP49/50 Notebook prototype, followed by HP48gII application-sized ownership
and the HP39 Aplet data boundary. Compare identical application
sources and configuration; record package transfer cost, supported-flow memory
use, allocation failure and settled return. This does not require the deferred
whole-system worst-case memory proof. Until those comparisons exist, the
installation-step benefit is established but a net memory, size or startup-time
benefit is not. Production launchers remain in use and this evaluation remains
open.

## Common bootstrap

The common ARM bootstrap receives a small family-specific launch context, installs the HPC1 virtual layout, enters the application, restores the original mappings and privileged state, and returns to the calculator OS.

The code coarse table owns the full `0x01200000..0x012FFFFF` section while public HPC1 code begins at `0x01220000`. The bootstrap may therefore use the unused prefix before `HPCLANG_CODE_BASE` for narrowly scoped **private** small-page mappings without borrowing arbitrary stock L1 entries. These mappings are launcher/runtime implementation state, not application memory.

HP39gs/40gs native-font access uses this mechanism: only physical NOR pages `0x0007e000` and `0x0007f000` are mapped uncached into the first two private code-window pages. This replaced an experimental long-lived high-L1 section alias after runtime evidence showed that the stock HP39 LCD framebuffer can occupy bytes inside the physical L1-table region and overwrite an otherwise-empty descriptor word during drawing. The production design therefore publishes only hpclang-owned L1/L2 state for the font window.

Calculator-model policy must not leak into application code unless an actual user-visible hardware difference requires it. Platform differences remain below the HPC1 boundary; model identity is passed separately in the boot record. `platform_cookie` remains an opaque launcher/runtime token and does not expose the private code window as application ABI.

### Launcher storage and return lifetime

Stage1 distinguishes file bytes from its full in-memory extent. The packed image
preserves initialized-section alignment; stage0 zeros the trailing BSS and
reserves pages for the complete extent. In particular, the page-aligned HP39
portal staging buffer consumes runtime RAM without also occupying the
transferred calculator object as several kilobytes of zeros.

Classic-RPL named writes may move the USEROB object containing stage0. Before
exposing mutation, stage1 copies the position-independent stage0 code into a
protected unused upper half of the L2 workspace and validates its Stock identity mapping. Return uses
that protected copy, including the final removal of the temporary stage alias.
The hidden launcher object and saved post-ARMSYS Saturn PC are explicit private
relocation roots. They move with USEROB/TEMPOB references rather than retaining
addresses into relocated data. The private `RPL2` bounds prefix identifies
records containing these native roots. `RPL3` appends the Port 1 write fence:
the exclusive nibble offset before the earliest ERAM page in the entire
ARMToolbox allocation pool, including stage code, page tables, code, stack and
heap. Runtime never infers Port 1 capacity from currently unused heap bytes.
Invalid pool descriptors do not publish this extension. The current runtime
continues to accept `RPL2` for main-RAM services but requires `RPL3` for Port 1
creation. These records remain launcher-private; rebuild applications and
their packaged launcher together when updating the SDK.

Classic-RPL main-RAM mutation fences invert each native pool page through the
Stock Read map. A 4 KiB physical page must have exactly one complete consecutive
pair of 2 KiB main-RAM mappings. A missing half, duplicate alias or overlapping
partial mapping disables publication of mutation bounds; absence from main RAM
is distinct from an invalid partial mapping. This proof covers main-RAM aliases
only, not arbitrary banked aliases or future mapping changes.

For read-only range analysis, `hpclang_rpl_stock_visit_spans` walks a stable
Stock Read map and reports maximal physically contiguous runs. It validates
every touched page before callbacks, retains separate logical aliases and
uses absolute physical nibble coordinates. This permits whole-range overlap
checks without assuming that consecutive Saturn pages are physically adjacent.
It does not select a bank, classify pointer values or authorize relocation.
`hpclang_rpl_stock_spans_overlap` compares complete ranges under independently
qualified maps, including discontinuities and aliases. Both ranges must be
valid even when early pages overlap; adjacent or opposite-parity nibbles do
not overlap. The result describes physical aliasing, not reference liveness.
The profiled original-ROM switches map a full 128 KiB window even for the
96 KiB bank 0 allocation; its tail aliases bank 1 at different Saturn addresses.
Mapped extent is not allocation ownership, and bank IDs alone do not prove
disjointness. Reference planning requires normalized physical identity and
address offsets. Unused
bank 14/15 entries select unaligned placeholder pages, which these helpers
reject. The offline `bank_switch.py` check executes the original switches;
it does not establish arbitrary bank lifetimes or authorize movement.
For proven affine aliases, `hpclang_rpl_reference_plan_offset_shift` tests
membership in the moving range using independent field/target offsets, while
preserving native patch addresses and relative encodings. The caller must
establish that each translation and delta applies across all old/new ranges.
The planner rejects split fields and native encoding overflow; it does not
resolve discontinuous mappings or bypass executable-reference obligations.
`hpclang_rpl_catalog_plan_relocation_offsets` applies those translations to
catalog payloads and switch-function targets independently. Its edits/guards
keep original catalog addresses for the existing preflight/commit path; zero
switches stay zero and nonzero function targets cannot become zero.
`hpclang_rpl_catalog_plan_remove` validates the complete ordered catalog before
returning a removal slot, original count/end and native library binding. Missing
IDs and malformed or unreadable tables leave the output unchanged. This is a
read-only prerequisite for unloading: callers must still prove detachment and
reference liveness, revalidate under ownership, and repair references while
compacting the table. It neither unregisters nor deletes a library record.
The internal `hpclang_rpl_catalog_remove_repaired` commit revalidates that plan
and its binding, validates the complete HOME attachment prefix and rejects
still-attached IDs before any writes. A match never hides unreadable later
records. This check does not replace execution-liveness analysis. It then
contracts the catalog/metadata/TEMP/return-stack suffix by
16 nibbles, applies caller-proven repairs and publishes the count last. Roots
into the removed catalog cell prevent contraction. Supplied fields in that
cell are rejected; surviving native/register roots and embedded fields retain
their normal preflight checks. A commit read/write failure is `INCOMPLETE`:
ownership must be retained, with no retry or Stock return. This is not yet a
public unload service: detachment, execution-reference coverage and payload
deletion remain separate obligations.

`hpclang_rpl_library_attachment_resolve` checks the library ID and resolves its
hash/message offsets within a caller-selected stable mapping. It publishes both
coordinates together, distinguishing absent fields from present address zero.
These coordinates are not ready-to-publish HOME fields: banked libraries use
separately owned DOACPTR wrappers containing the target and switch identity.
Direct-mapped table pointers are retained even when the table itself is a
DOACPTR. Wrapper allocation/lifetime and transactional registration remain
caller obligations; the resolver does not dereference table bodies or run code.

`hpclang_rpl_mutation_visit_guarded_temp` records the allocation traversal's
geometry, sentinel and footer reads in both validation and enumeration passes.
These observations let the relocation plan guard the allocation boundaries that
selected its object graph. A malformed complete chain rejects before payload
callbacks; dependency/read/callback errors invalidate partial output. Payload
contents still need separate guarded traversal and execution provenance. The
native catalog/cache probe uses this traversal instead of an unguarded footer
walk; this does not authorize public registration.

Internal `hpclang_rpl_mutation_cache_acptr_insert/remove` stores 15-nibble
DOACPTR wrappers in the main-RAM metadata cache between the catalog end and
TEMPBOTTOM. Insertion requires the caller's expected TEMPBOTTOM and initializes
the prologue, target and switch before publishing geometry. Removal verifies all
three fields within the cache; the caller must independently prove the complete
object boundary and ownership. Surviving references into the removed extent
reject with BUSY. Shared movement uses the actual 15- or 16-nibble width for field
boundaries, root repairs, fences and accounting. Caller-proven embedded/register
repairs retain their original guards; preflight is write-free and commit failure
is INCOMPLETE. These primitives neither attach HOME fields nor execute switches.
Registration transactions, complete reference discovery, lifetime integration and
native execution acceptance remain open.

The internal `hpclang_rpl_mutation_home_attach` appends a HOME attachment using
two caller-proven native pointer fields. It validates the complete existing
prefix before rejecting a duplicate ID, checks count capacity and free-space
fences, then moves the lower stack/HOME prefix down by 13 nibbles. Existing and
new pointer targets in that prefix are repaired; directory objects remain fixed.
The new record is written before publishing the increased count. Zero pointer
fields are preserved. Preflight faults make no writes, while commit faults
return INCOMPLETE and require retaining ownership without retry or Stock return.
The caller must establish library identity, field semantics and relocation
safety; this primitive does not run library configuration or register a binding.

The internal `hpclang_rpl_mutation_home_detach` removes a matching HOME attachment
without unregistering or freeing the library. It validates the entire attachment
prefix and directory geometry, moves the stack/HOME prefix upward by 13 nibbles,
repairs known roots and publishes the reduced attachment count last. Following
directory objects retain their addresses; HOME and earlier attachment fields
move. Duplicate matching IDs, live roots into the removed record and unreadable
input reject before writes. Commit failures require retained ownership as above.
Caller-proven relocation safety and execution liveness remain necessary; this
internal primitive does not make public unloading complete.
`hpclang_rpl_reference_edits_check_aliases` resolves complete fields before
checking physical overlap, including different IDs/coordinates and repeated
pages within one map. The mapped RAM replay entry performs this check before
target I/O, then preserves the ordinary expected-or-replacement retry rules.
Resolvers must prove contiguity and stable mappings across the entire batch;
this adds no permission to write ROM or move objects with unresolved roots.

`hpclang_rpl_dispatch_reference_visit_guarded` records the native reads used
by the two caller-proven dispatch schemas. These include the dispatch word and
the scalar type operand or first embedded-entry nibble. Dependency callback
failures survive parser error conversion; partial output must be discarded.
Type identifiers remain scalars, while execution and embedded-entry events
retain their unresolved code obligations. The descriptor's identity and stable
mapping are caller preconditions; observing bytes does not authenticate code.
Host regressions check changed dependencies, every read/observer fault, invalid
nibbles, callback errors and untouched trailing payloads. Offline checks against
retained HP49g+/HP50g predicate fixtures cover 42 dispatch entries and 332
observer interruptions; they do not execute a new application or close full
execution-reference coverage.

The public port adapter treats `HPCLANG_RPL_MUTATION_INCOMPLETE` as terminal.
`hpclang_runtime_stop` masks IRQ/FIQ and retains the current mappings/ownership
without destructors, service cleanup or Stock return. Ordinary `exit`/`_Exit`
would release that ownership, so they are not valid commit-failure paths.
Port1 body preparation failures remain recoverable `EIO` while its zero
prologue still hides the record. Stable production RAM callbacks should not
fail after preflight; the terminal path preserves the contract if they do.
Port0 backup creation/replacement/removal and library creation observe Saturn
write attempts and ARM-root changes, which can precede the first Saturn write.
A subsequent failure returns `INCOMPLETE`; unchanged preflight failures retain
their original result. The root snapshot is allocated before mutation; allocation
failure returns `ENOMEM` without native changes. This boundary adds neither
rollback nor coverage of previously unsupported references.
Public Port1 replace/remove still require complete reference protection.

## Platform families and model profiles

The platform family is the implementation unit for launch and low-level integration:

- `hp39` — HP 39g+, HP 39gs, HP 40gs;
- `hp48` — HP 48gII;
- `hp4950` — HP 49g+, HP 50g.

A model profile is narrower. It records real per-model facts such as model ID, firmware source, bootloader identity, LCD height, or stock transfer sequence. It must not create a runtime fork merely because a calculator has a different product name.

HP39g+ D1.55 has its own font layout, established from the verified real-machine full-NOR fixture: pages `0x0006e000` / `0x0006f000`, versus `0x0007e000` / `0x0007f000` on HP39gs/40gs 2.22. Both use the same owned two-page window and font API. The complete tables match the reviewed atlas; [model-specific pixel validation](validation.md#hp39g-native-font-follow-up) does not infer equality from family membership.

## Common platform services

The application-facing clean core now includes device information, stock-preserving monotonic time, raw/debounced keyboard events with established per-model physical key identity across all six models, opaque display/software surfaces, clipped geometry and blit, calculator-native/generated fonts, UTF-8 text, and hp4950 byte-stream stdio through picolibc. HP39g+ model-1 identity is backed by the real D1.55 fixture and strict Direct key-transition evidence rather than sibling firmware inference.

These services keep platform representation and ownership below common APIs. Calculator-object/stack integration is established through the M7.3a-e owned-object/read/stack/adapter baseline: HP39 typed objects and HP48/49/50 Saturn/RPL objects are not flattened into byte-stream files or exposed as raw stock addresses. M7.3f adds public named-store put/remove and HP39 native-variable get/set with the same owned-copy boundary; its implementation and formal CI acceptance are complete.

The project does not copy the historical HPGCC API surface wholesale. Historical names or services become adapters only when they describe useful source-level behavior over an already coherent hpclang core.

## Downstream system boundary

hpclang is an SDK/runtime layer, not the owner of downstream operating-system policy. Projects such as **39gsX** may consume hpclang services, but hpclang must not depend on, special-case, or expose an ABI branch for a downstream system.

This means:

- generic application-facing services such as input, display, fonts, time, storage semantics and future renderer-agnostic UI primitives may belong in hpclang;
- machine-owning subsystems such as 39gsX OHCI/USB host or OTG policy, USB MSC transport, system VFS, file manager, native Aplet loader, alternative-system boot, kernel exception/MMU ownership and swap remain in 39gsX;
- a platform-private driver may exist inside hpclang when it is only the implementation needed to provide a clean hpclang service (for example HP49g+/HP50g SDI beneath the standard stream API). That does not turn the driver into a general public hardware framework;
- no `#ifdef HP39GSX`, 39gsX-specific boot mode, or reverse dependency belongs in hpclang.

The dependency direction is therefore **hpclang → reusable application contract; 39gsX → downstream consumer/system owner**, never a two-way runtime dependency.

## Non-ROM policy

The supported SDK must work through ordinary calculator software mechanisms. ROM replacement or ROM patching may be studied as an optional research or compatibility environment, but it is not part of the normal hpclang architecture.

Read-only access to calculator-owned stock assets through launcher-private mappings does not change this policy: hpclang does not replace or modify the calculator ROM, and the mappings remain below the application ABI.

## Historical execution models

Historical HPGCC launchers are specifications and test evidence. APCS-GNU, r9/r10 static-base conventions, ROPI/RWPI experiments, and Launcher V3 application ABIs are not supported hpclang application modes.

Those paths were useful during bring-up because they established that stock Clang/LLD can coexist with the calculator launch environment and because they exposed the underlying ARMSYS/MMU behavior. The production architecture deliberately moves those details behind the fixed-address HPC1 ABI instead of making applications reproduce historical compiler conventions.
