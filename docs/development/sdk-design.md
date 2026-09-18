# SDK API design

M7 adds the application-facing calculator services that turn the validated HPC1 execution environment into a practical SDK. This document records durable design constraints; implementation status belongs primarily in [API reference](api-reference.md), [roadmap](roadmap.md), and [validation](validation.md).

## Goals

The application API should:

- work coherently across `hp39`, `hp48`, and `hp4950`;
- remain usable from C and modern C++;
- make model limitations discoverable rather than silently changing semantics;
- allow common HPGCC 1.x/2.x source to be ported with minimal mechanical work;
- avoid carrying historical compiler or binary-ABI constraints into new code;
- keep ownership, lifetime, units, and error handling explicit;
- remain small enough for machines with very limited RAM.

## Non-goals

The core API is not required to be binary-compatible with HPGCC executables, source-identical to every historical header, ABI-compatible with HPGCC3 ROMLIB, dependent on a replacement calculator ROM, POSIX-complete, or object-oriented internally.

Compatibility is primarily a **source-porting** concern.

## Three API layers

### 1. Stable C service ABI

The platform/runtime boundary and canonical public services use a C ABI. This layer owns primitive values, explicit buffers, opaque handles, fixed-width integers, and intentionally controlled structs. C++ standard-library types never cross this boundary.

### 2. Modern C++ wrappers

`hpclang/*.hpp` may provide zero-cost or low-cost wrappers around the C layer: RAII, `std::span`, strongly typed coordinates/durations, and `std::string_view` inputs where no C++ object becomes part of the platform ABI.

### 3. HPGCC source-compatibility layer

Historical names are implemented above the clean core, never the other way around. Each historical interface is classified as Direct, Adapter, Migration, or Unsupported. Binary compatibility is not a goal.

## Execution environment and hardware ownership

hpclang separates **service semantics** from **hardware ownership policy**.

The validated stock-hosted environment borrows the HP OS and must return control afterwards. Platform code should therefore preserve IRQ/timer/clock/display state where practical and restore temporary changes. A future hpclang-controlled bootloader or OS may legitimately own those peripherals while exposing the same application-facing services.

Do not freeze a public execution-environment enum until code genuinely needs one. Do not make downstream firmware, operating systems, or applications named hpclang targets or dependencies.

## Core API rules

### Ownership and allocation

Low-level calls should not allocate unless allocation is the operation. Writing functions normally accept caller-provided buffers or explicit owned handles. Every owned resource needs an obvious release path.

### Errors

The C core should use explicit status values/output parameters for normal failure. Historical wrappers may translate those results into legacy conventions.

### Coordinates and display geometry

Do not hard-code 131×64 or 131×80 into common contracts. The physical display is a borrowed opaque surface with queryable geometry and intensity capability. Framebuffer address, stride, and bit packing remain private.

Drawing primitives take explicit intensity rather than a hidden mutable global graphics context. Historical HPG's `hpg_t` color/mode/font/pattern/clip state belongs in the compatibility adapter unless a non-HPG application demonstrates a clean-core need.

Owned 2/4/16-level software surfaces and representation-independent blit are supported without exposing packing.

### Physical grayscale and page flipping

Physical LCD mode switching is separate from software surfaces. Historical HPG changes controller mode and framebuffer addresses and relies on runtime teardown to restore them. A stock-hosted hpclang app cannot assume that ownership.

The S3C2410 controller scans a 160-pixel-wide line even though the visible calculator surface is 131 pixels. Historical mono/gray4/gray16 physical storage therefore uses 20/40/80 bytes per scan line, and buffers must be physically contiguous and aligned. HPC1's ordinary heap is assembled from independently mapped pages, so it is not a safe DMA allocation contract.

A future physical gray/page-flip API therefore needs an on-demand platform resource broker or contiguous DMA pool. It must not permanently reserve scarce pages from every application merely to support an optional display mode. Until that boundary exists, software 4/16-level surfaces are the supported grayscale model.

Direct E2E snapshots stable LCD configuration before HPC1 and compares it before harness restoration, so future physical display ownership must explicitly restore stock state.

## Fonts and text

The font design has **two independent source classes** and a common rendering API.

### Calculator-native fonts

`hpclang_font_mini()`, HP48/49/50-only `hpclang_font_big()`, HP39gs/40gs-only `hpclang_font_editline()`, and the convenience `hpclang_font_default()` return borrowed descriptors over calculator-owned font assets. hpclang does not embed or redistribute HP ROM glyph bitmaps.

The implementation deliberately differs by platform family:

- **HP39/40:** native mini/main/EditLine rasters are fixed physical stock-ROM assets. They are read through the physical ARM flash view, not the current logical Saturn bank mapping. The mini table is proportional; main is 5×8/advance 6; EditLine is fixed 6×10/advance 6. HP39 does not expose the HP48-compatible `bigfont`.
- **HP48gII/49g+/50g:** native minifont/bigfont are stock Saturn objects resolved through the launch-provided logical `SAT_CPU->Read[]` map.

This difference is not an hpclang invention. Historical HPGCC's calculator-native helper path explicitly resolves the HP48-compatible minifont at Saturn `0x812c8` and bigfont object at `0x84d82` through `sat_map_s2a()`, whose implementation indexes `_saturn_cpu->Read[]`. The HPGCC HP39 Aplet/Library 275 support instead binds HP39 stock ROM text entries such as `Put3x5` and `Put5x7`.

The exact hpclang HP39 raster locations (`0x07E695` mini and `0x07C651` main in physical Saturn nibble space) come from independent HP39 stock-ROM reverse engineering and pixel/renderer validation. They must not be described as HPGCC constants.

This distinction is implementation-only. Applications receive the same opaque font interface and must not learn physical ROM addresses or `Read[]` internals.

### Generated custom fonts

Generated fonts use the checked-in bitmap-font catalog produced from the canonical source bundle. Normal application builds do not parse source font files or require Python/fontTools/Pillow.

The **canonical bundled asset set is bitmap-source only**. The generic TTF importer has been removed and is not part of the supported project surface. Candidate additions should prefer upstream BDF/PCF or another directly auditable bitmap source; any one-time deterministic derivation needed for a reviewed candidate belongs to source-package curation/provenance rather than normal SDK importing.

`hpclang_target_font()` supports exactly three repertoire modes:

- `STATIC_AUTO`: collect only code points from explicit `HPCLANG_UI_TEXT("...")` markers in the target's sources;
- `STATIC_EXPLICIT`: retain explicitly supplied code points plus the configured fallback;
- `FULL`: retain the selected face's complete mapped repertoire.

`HPCLANG_UI_TEXT` is an identity macro at runtime. It is build metadata, not a runtime font/string type. Ordinary literals, diagnostics, paths, format strings, comments, and fake marker text do not influence `STATIC_AUTO`.

Generated glyph/bitmap sections remain individually garbage-collectable. The wrapper ABI supports arbitrary metrics and faces; convenience aliases `@mini`, `@normal`, and `@large` are policy, not a restriction on the format.

### Text API and UI policy

The canonical renderer accepts code points and UTF-8. Invalid UTF-8 becomes U+FFFD and then the selected font's fallback when necessary. `hpclang_surface_draw_char()` remains a direct byte/U+0000..U+00FF compatibility entry point.

The core does not define semantic UI font roles. Historical native accessor names such as `mini`, `big`, and `editline` identify concrete ROM fonts only; a future UI toolkit may assign its own `text`, `caption`, `editor`, or other theme roles above them.

Font Gallery is deliberately a **separate HPC1 E2E application**. It exercises native fonts, generated aliases/arbitrary faces, metrics, representative repertoire, and both 64- and 80-line layouts without expanding the canonical runtime/keyboard/storage payload. Its specimen text is part of the visual test contract and now includes face-sensitive width, punctuation, accented Latin, digits/symbols, and supported CJK/Japanese/Korean samples rather than one weak generic phrase.

The Gallery evidence UI is also part of the test surface. Final M7.1 closure is `E2E / Font Gallery` **35298949457 / #112**. HP39gs/40gs Direct expose 245 generated faces + mini/main/EditLine for **248 entries / 496 captures**; HP48gII/49g+/50g retain 245 generated + minifont/bigfont for **247 / 494**. Stock hp39gs/hp40gs additionally verify the EditLine ROM fingerprint/pixels, while HP48gII/HP50g retain live-Saturn native evidence.

### HPG compatibility consequence

Historical HPG's bundled minifont/bigfont and caller-owned custom font descriptors are compatibility semantics, not the canonical hpclang asset model. An HPG adapter may retain the historical mutable font/context state and render through hpclang surfaces/fonts without exposing native ROM storage.

## Keyboard

The public keyboard core separates raw physical state from higher-level events.

A snapshot is eight bytes: bits 0–55 are the 8×7 matrix (`column * 7 + row`), bit 56 is ON, and bits 57–59 are raw GPF4/5/6 inputs whose labels vary by family. Matrix coordinates are physical scan positions, not portable meanings and not the historical HPGCC keymatrix numbering.

The stock-hosted backend synchronously polls GPIO and does not install a keyboard IRQ or background timer. The second layer debounces that state and emits DOWN/UP/REPEAT events with timestamp, modifiers, raw scan, and physical key identity. A freestanding backend may feed the same event contract from IRQs later.

All 51 physical positions pass Direct E2E on HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. HP39g+ stays UNKNOWN until its real fixture exists.

The service stops at physical key identity plus modifier state. Model/UI shortcuts and text/Alpha/Shift-plane translation belong above it.

## Time

All time APIs name their unit. Monotonic microseconds form the basis for delays, deadlines, and event timing. The stock backend observes existing timer/RTC state instead of taking hardware ownership.

## Storage and stdio

Storage is split by **semantics**, not by media.

### Byte-stream files

HP49g+/HP50g now provide private KOS-backed `open/close/read/write/lseek/unlink` hooks beneath picolibc. Public code uses ordinary `FILE *` APIs. Stock E2E covers read, create/truncate, write/flush, nonzero seek/ftell, reopen, append/create, remove and `ENOENT`.

HP39/40 and HP48 do not pretend to expose that SD byte-stream service; unsupported stream operations fail cleanly with `ENOSYS`.

Raw KOS module/function IDs remain runtime-private.

### Calculator objects

HP39gs/40gs have a real typed object store: directory, Aplet/Zaplet, Note, Program, Variable, List, Matrix, Library and related records, rooted at `HP39DIR.000` with `HP39DIR.CUR` tracking the current directory. These are not POSIX files and need a typed/named object service.

The HP39 ROM exposes System-RPL-side object operations such as `DoGetObFrSto`, `DoSendObToSto`, `OpenIO`, and `StdIOProc`. A future implementation should reach them through the typed Saturn/object bridge rather than make their addresses/calling conventions public ARM ABI.

HP48/49/50 RPL/Saturn objects belong to the same typed-object side.

## Saturn/RPL integration

Saturn addresses and RPL objects should never be ordinary C pointers. Use explicit address/object types or opaque handles with documented borrow/copy/ownership semantics. Historical `sat_*` APIs are adapters above this model.

## Capabilities

Capabilities describe genuine model/platform differences such as display geometry, stream storage, object services, or transfer mechanisms. They must not become a substitute for clean platform implementations.

## Declarative UI core (planned)

hpclang should include a small **renderer-agnostic declarative UI engine**, not a fixed calculator theme or widget skin. The intended application model is:

```text
View = f(Model)
event -> update(Model) -> declare View again -> reconcile by stable key
```

The clean core may own stable node identity, keyed reconciliation, deterministic row/column layout, focus and keyboard navigation, event/action routing, controlled-input state, explicit async state/effect hooks, dirty-region tracking, and a bounded custom-canvas node. Ordinary C functions remain sufficient for component composition; the implementation should remain compatible with fixed-capacity pools and predictable ARM920T memory/time budgets rather than requiring a desktop-style heap-retained object graph.

The core must **not** freeze visual policy. Style identifiers are renderer/theme inputs rather than pixel definitions. Widget chrome, fonts/colors, native calculator appearance, and final rasterization belong to a backend above the layout/logic engine. A downstream project such as 39gsX can therefore implement a renderer/theme matching the independently reverse-engineered HP39 native UI while another environment can render the same declarative tree differently.

A canvas/custom-paint escape hatch is part of the boundary: declarative layout owns the node and its bounds, while application/backend code performs specialized drawing only when the node revision changes or its region is damaged. Historical HPGCC `win*` compatibility may later target this engine where useful, but the legacy widget model does not define the canonical UI API.

Public C names, node storage limits, layout units, focus traversal rules, and renderer ABI remain deliberately unfrozen until a focused design/fixture slice begins.

## Modern C++ profile

hpclang uses Clang/LLVM, libc++, libc++abi, compiler-rt, and picolibc with `-fno-exceptions` and `-fno-rtti`. This is a modern embedded C++ profile, not unrestricted hosted C++.

Current C++23 validation includes templates/concepts/lambdas/constexpr, `std::array`, `std::span`, `std::optional`, `std::string`, `std::vector`, heap-backed objects, global construction, and wrappers over real time/display services.

Exceptions, RTTI, threading, locale-heavy facilities, iostream/file streams, and `std::filesystem` are separate capabilities and are not claimed without dedicated runtime support.

## Current dependency order

The clean-core order is now:

```text
device/capability facts
+ monotonic time
+ physical keyboard/events
+ display/software surfaces
+ native/generated fonts and UTF-8 text
+ hp4950 standard stream I/O
```

M7.1 font architecture, asset curation, native-ROM census, keyboard/runtime return evidence, and validation are complete: the final bitmap catalog is **245 faces / 1,639,485 glyph mappings / 3,472 assembly sources**, Font Assets #24 is clean, Runtime #135 is fully green, and Gallery #112 is fully green. The HP48 visible Stock-return repaint is closed as part of the canonical runtime gate. M7.2 now completes the HP49g+/HP50g byte-stream namespace/metadata/directory surface before M7.3 begins the typed calculator-object/Saturn-RPL bridge. Broad HPGCC adapters and the planned declarative UI engine remain downstream of stable clean-core services.
