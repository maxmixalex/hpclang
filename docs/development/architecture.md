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
`hpclang_rpl_reference_edits_check_aliases` resolves complete fields before
checking physical overlap, including different IDs/coordinates and repeated
pages within one map. The mapped RAM replay entry performs this check before
target I/O, then preserves the ordinary expected-or-replacement retry rules.
Resolvers must prove contiguity and stable mappings across the entire batch;
this adds no permission to write ROM or move objects with unresolved roots.

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
