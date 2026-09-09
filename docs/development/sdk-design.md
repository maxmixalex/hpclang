# SDK API design

M7 adds the application-facing calculator services that turn the validated HPC1
execution environment into a practical SDK.

This document records design constraints before public service names are frozen.

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

The core API is not required to be:

- binary-compatible with HPGCC 1.x/2.x executables;
- source-identical to every historical HPGCC header;
- ABI-compatible with HPGCC3 ROMLIB;
- dependent on a replacement calculator ROM;
- POSIX-complete;
- object-oriented internally.

Compatibility is primarily a **source-porting** concern.

## Three API layers

### 1. Stable C service ABI

The platform/runtime boundary and the canonical public service implementation use
a C ABI.

This layer owns primitive values, explicit buffers, opaque handles, fixed-width
integers, and stable structs whose layout is intentionally controlled.

C++ standard-library types must never cross this boundary.

Reasons include:

- C is usable by C++, Rust, and other future front ends;
- it avoids coupling the SDK ABI to a particular libc++ revision;
- Arm Toolchain for Embedded uses libc++'s unstable ABI configuration;
- calculator-side services often map naturally to explicit low-level operations.

### 2. Modern C++ wrappers

`hpclang/*.hpp` may provide zero-cost or low-cost wrappers around the C layer:

- RAII ownership for resources that need release;
- scoped state restoration;
- `std::span` views over buffers;
- strongly typed coordinates, durations, and flags where useful;
- iterators/ranges where they do not add hidden allocation;
- overloads accepting `std::string_view` without storing libc++ objects in the
  platform ABI.

The C++ layer is a convenience surface, not a second platform implementation.

### 3. HPGCC source-compatibility layer

A separate compatibility surface should forward historical source patterns to
the new core API.

Compatibility headers must not become the canonical implementation API.

Each historical interface should be classified as one of:

| Class | Meaning |
| --- | --- |
| Direct | Same source-level behavior can be provided safely and cheaply. |
| Adapter | A wrapper can preserve common source usage but semantics or types need translation. |
| Migration | No compatibility wrapper; document the hpclang replacement. |
| Unsupported | The historical behavior conflicts with hpclang architecture or cannot be provided reliably. |

Binary compatibility with old HPGCC executables is not a goal.

## Historical API inventory

The official HPGCC 1.1/2.0 source trees contain public functionality in areas
including:

- screen/graphics;
- keyboard;
- file I/O;
- Saturn memory and stack integration;
- calculator OS/KOS calls;
- system services;
- stdio helpers;
- strings/math helpers;
- higher-level window/widgets in the HPGCC library tree.

HPGCC 2.0 is not a strict superset of 1.1. In particular, the HP39 and HP48
platform-specific paths present in 1.1 are not simply preserved as active 2.0
targets.

HPGCC3 must be inventoried separately. It uses a revisioned library environment
and includes more structured subsystems such as its filesystem. Its API may
provide useful design ideas, but its ROMLIB ABI is not the hpclang ABI.

M7.0 now mechanically inventories the public headers from complete HPGCC
1.0.3, 1.1, 2.0 SP2, and HPGCC3 distribution archives. The resulting
function-level compatibility matrix is recorded in
[HPGCC compatibility inventory](hpgcc-compat.md).

## Execution environment and hardware ownership

hpclang separates **service semantics** from **hardware ownership policy**.

The currently validated calculator path is stock-hosted: the HP operating
system launches HPC1 code and expects control back afterwards. In that
environment, platform code should borrow hardware state where practical,
avoid unconditionally replacing stock IRQ/timer configuration, and restore any
temporary state it changes. This reduces ROM-version coupling and makes clean
return a meaningful contract.

That rule must not be generalized into "hpclang never owns interrupts or
timers". A hpclang-controlled bootloader or operating system has a different
contract: it is responsible for initializing and owning the interrupt
controller, clocks, timers, display, keyboard interrupts, DMA, and other
hardware needed by the system.

The public service layer should therefore remain usable in both environments.
For example, keyboard events can be produced by synchronous GPIO polling in a
stock-hosted backend and by an IRQ-fed queue in a bare-metal backend without
changing the application-facing event representation. Likewise, monotonic time
can observe a host-configured timer in a borrowed environment and use an
environment-owned timer in a freestanding implementation.

Do not freeze a public execution-environment enum until code genuinely needs
one. For now this is an implementation/ownership boundary, not another target
axis in the application ABI.

Do not make downstream firmware, operating systems, or applications named
dependencies of hpclang. They are consumers of the compiler/SDK contract, not
inputs to it. A downstream project may motivate a generic capability, but it
must not create a project-specific target, backend name, ABI branch, or public
API in hpclang.

Public API and compatibility decisions are grounded in the supported calculator
hardware, validated stock behavior, the hpclang ABI, and the historical HPGCC
interfaces being replaced or adapted. Downstream projects may be useful
comparative evidence, but they do not define hpclang architecture.

## Core API design rules

### Ownership and allocation

Low-level calls should not allocate unless allocation is the purpose of the
operation.

Functions that write data should normally accept caller-provided buffers or
return explicit opaque owned objects.

Any returned ownership must have an obvious matching release operation.

### Errors

Do not use global implicit error state as the primary hpclang interface.

The C core should use explicit status values and output parameters where an
operation can fail for ordinary reasons. Compatibility wrappers may translate
those results into historical conventions.

### Coordinates and display geometry

Do not hard-code 131×64 or 131×80 assumptions into common API contracts.

Display size and pixel/plane capabilities should be queryable. Coordinates
should have one documented origin and clipping rule across platforms.

The first display abstraction is an opaque **surface**. The physical display is
a borrowed surface whose framebuffer address, stride, and bit packing remain
runtime-private. Applications can query geometry and color-level capability,
then use clipped clear/pixel/line/fill-rectangle primitives.

Drawing primitives currently take their intensity explicitly instead of
introducing a mutable core drawing-context object. Intensity uses 0 for white
and 255 for black; the current two-level physical surface thresholds
intermediate values internally. This also lines up cleanly with HPG's historic
256-value virtual color convention in monochrome mode without exposing HPG's
object layout.

Historical HPG stores the pixel target, clip, color, PAINT/XOR mode, font, and
fill pattern together in `hpg_t`. The compatibility adapter can retain that
legacy state and forward primitive operations to an hpclang surface. A
canonical draw-context type should be added only if non-HPG applications
actually need shared mutable drawing state.

Owned monochrome surfaces and representation-independent blitting now provide
the next surface layer. Fonts should build on that ownership/copy boundary.
Clipping state, grayscale/double buffering, and composite modes remain later
display work. None of them should require exposing the physical framebuffer
representation.

### Font/text and display research checkpoint

The historical HPG implementations sharpen the next boundary rather than
changing the existing surface design.

**Fonts and text.** HPG's built-in minifont and bigfont are fixed-advance
bitmap fonts with the same 6/4 and 8/6 height/advance metrics already selected
for hpclang. Historical HPG stores one byte per glyph row and exposes a
256-entry byte-indexed font namespace; custom fonts are lightweight descriptors
over caller-owned row data. Text drawing is fixed advance, and newline resets
to the original x coordinate and advances by one font height.

The merged font/text layer uses independently sourced public-domain
bitmaps and intentionally defines printable Basic Latin as the canonical v1
repertoire. The same independent X11 source also contains printable Latin-1
(0xA0-0xFF), but that does not reproduce HPG's calculator-specific/control
glyph range. Expanding the clean core merely to imitate HPG's 256-entry count
would therefore be misleading; HPG character-set parity remains adapter work.
Exact HPG bitmap data is not copied into hpclang.

Custom HPG font allocation does not force a mutable font subsystem into the
core. An HPG compatibility adapter can retain the historical caller-owned
bitmap descriptor and rasterize through the same explicit surface primitives.
The canonical API should only grow a custom-font constructor if a non-HPG
application demonstrates that need.

**Screen and graphics.** Historical `hpg_t` combines surface storage, color,
PAINT/XOR mode, font, fill pattern, and a mutable clip rectangle. hpclang should
continue to keep the core surface explicit and avoid making that legacy
graphics context its canonical state model.

The next safe, representation-independent primitives are circles and polygons.
The merged geometry layer adds those without mutable graphics state: circles
are clipped to visible rows/columns before rasterization, and polygons use
explicit `hpclang_point` arrays with even-odd fill semantics. This avoids
making huge off-screen circle radii proportional to runtime work and keeps the
surface representation opaque.

A deliberate clipping abstraction should follow only if application code needs
a clip region beyond the surface bounds. A clip rectangle should not become
hidden global state; a lightweight draw context/surface view or another
explicit mechanism is preferable.

Software-owned grayscale surfaces are also separable from physical LCD mode
ownership. Historical HPG allocates 1-, 2-, and 4-bpp off-screen images and
uses the same drawing/blit interface across them. hpclang now generalizes its
private owned-surface representation to 2, 4, or 16 intensity levels while
preserving one 0..255 public intensity contract.
Packing remains opaque, and the existing representation-independent blit
therefore works across level counts without a new image ABI.

Physical grayscale mode switching, hardware double buffering, and page flipping
are a different category. Historical HPG allocates physical framebuffers,
changes LCD mode, synchronizes the controller, and rewrites LCD address
registers during `hpg_flip()`; its runtime relies on surrounding CRT startup/
shutdown code to restore display hardware. A stock-hosted hpclang application
does not currently own that hardware. Physical mode changes must therefore
remain deferred until every platform has an explicit save/restore contract and
Stock E2E proves clean return. Software off-screen buffering does not have this
restriction.

The source audit makes the physical-buffer requirements concrete. The S3C2410
LCD controller scans a 160-pixel-wide line even though the application-visible
calculator area is 131 pixels wide. Historical `sys_lcdsetmode()` therefore
uses 20, 40, or 80 bytes per scan line for mono, 4-level, or 16-level mode and
programs the lower DMA bound using `height + 5` scan lines. Its HPG wrapper
also aligns the framebuffer to 16 bytes. A future hardware mode backend must
derive those controller dimensions from platform data rather than treating the
131-pixel surface stride as the DMA stride.

More importantly, the framebuffer must be physically contiguous. Historical
HPG obtains ordinary heap candidates and accepts one only when the virtual
range maps to one continuous physical memory class, then cleans cache before
handing the physical address to the LCD controller. HPC1 cannot assume that
about its normal heap: ARMToolbox stage1 explicitly assembles the writable
arena from an array of independently mapped 4 KiB physical pages. A physical
grayscale/page-flip API therefore needs a platform-owned contiguous/DMA buffer
allocator or reserved pool. It must not expose that requirement through the
ordinary `hpclang_surface` storage ABI.

The historical launchers show one feasible source for such memory, but also why
hpclang should not reserve it unconditionally. The normal ARMToolbox and its
HP48-specific variant use a single KOS allocation, page-align the usable
interior, and append consecutive 4 KiB descriptors to the temporary 0x09000000
page table. The normal path probes up to 120 KiB and intentionally leaves
16 KiB to the OS; the HP48 path probes up to 48 KiB and leaves 4 KiB. The HP39
Library 275 launcher has its own MMU path and an explicit `USESYSRAM` extension;
hpclang's current Launcher V3 envelope selects that MMU/system-RAM path.

A 16-level LCD plane needs 80 bytes per controller scan line. Two 64-row planes
therefore need a little over 11 KiB, while two 80-row planes need about
13.6 KiB before alignment/padding. Reserving four 4 KiB pages from every HPC1
process merely so page flipping might be used would be a disproportionate heap
tax on the smaller machines. The preferred future design is therefore a
platform resource request/broker that obtains or reserves contiguous LCD memory
only for applications that request physical display ownership, before ordinary
heap use can consume the relevant pages. Until that boundary exists, physical
gray4/gray16 and page flipping stay deferred; software 4/16-level surfaces are
the supported grayscale model.

Before any such backend is enabled, Direct E2E now gates LCD ownership
preservation. It snapshots stable S3C2410 LCD configuration before HPC1 and
compares it at the return sentinel, before harness restoration. Dynamic
interrupt pending/source registers are intentionally excluded. Future physical
mode switching must explicitly restore this state rather than relying on the
host harness to repair it.

Historical HPG's PAINT/XOR mode and repeating fill patterns are likewise
graphics-context policy rather than surface representation. The clean core
should not grow mutable `color/mode/pattern/font` fields to imitate
`hpg_t`. A future HPG adapter can hold those fields and dispatch explicit
surface operations. If non-HPG applications later need compositing, the
candidate core shape is an explicit operation/blend argument, not hidden
per-surface state.

Legacy `hpscreen.h` calls such as `getLCDPointer`, display-plane selection,
packed 4-bit sprite operations, and direct OR/XOR block drawing are
representation-specific compatibility/migration facilities. They must not leak
framebuffer addresses or packing into the core surface ABI.

### Keyboard

Separate raw physical-key state from higher-level key events.

The first stable layer is now `<hpclang/keyboard.h>`. A snapshot is exactly
eight bytes and uses an hpclang-specific raw encoding:

- bits 0–55 are the 8×7 S3C2410 matrix, packed as
  `column * 7 + row`;
- bit 56 is the dedicated ON input;
- bits 57–59 are raw GPF4/5/6 inputs whose key labels differ between HP39 and
  HP48/49/50.

Those matrix coordinates are physical scan positions, **not** portable key
meanings. HPGCC 1.1 itself documents its logical row/column table as HP49g+
specific, with HP39g+ different and HP48gII not verified. hpclang therefore
does not make the historical HPGCC `keymatrix` bit numbering its core ABI.

The raw reader is synchronous and does not install an IRQ, alter the clock
tree, or retain GPIO changes after it returns. It is suitable for games and
other callers that need instantaneous state.

The second layer is a synchronous, polled event service. It debounces the raw
snapshot and emits `DOWN`, `UP`, and `REPEAT` events. Repeat timing is
configurable, and each event carries physically held modifiers. Alpha and ON
are portable bits. `HPCLANG_KEYBOARD_MOD_SHIFT` is a normalized physical
Shift bit across all validated models; HP48/49/50 additionally expose
left/right Shift bits. `hpclang_keyboard_event_matches()` provides a
header-only key+modifier convenience check without adding another runtime ABI
layer.

The current **stock-hosted backend** deliberately does not install a keyboard
IRQ or background timer. Calling code drives progress by polling, so an
application launched from the HP OS does not steal interrupt/timer ownership
from the environment it must later return to.

This is a backend policy, not a permanent hpclang API or HPC1 restriction.
A freestanding execution environment that owns the machine may instead own the
interrupt controller and timers, feed the same event API from an IRQ-driven
queue, and implement delays/deadlines from environment-managed timers.
Application code should not have to change merely because the execution
environment owns more of the hardware.

The event layer now also carries a **physical key identity** in `event.key`.
The first Direct measurement established the common raw scan positions across
all five automated models. Complete key identities are then filled from the historical
keyboard tables instead of re-measuring every documented key: the stable
HPGCC 1.1/2.0/HPGCC3 matrix table for HP49/50, the HPGCC 1.1-39G HP39
`HEADER.H` keycodes plus the validated matrix layout for HP39gs/40gs, and the
same documented electrical matrix for HP48gII. Direct E2E injects every one of the 51 physical key positions on each
automated model and checks DOWN/UP, raw scan code, physical identity, and
dedicated-key modifier state. The final guest screen renders those positions
in physical keyboard geometry rather than electrical scan order so the same
result is recognizable in LCD evidence. This also preserves the HP49g+
CAT/EQW versus HP50g EVAL/quote primary-label
distinction at their shared physical positions. The one-off scan probe is not a
runtime input and is not retained as repository data after its mapping has been
incorporated into source and regression tests.

Common keys retain common enum values; keys whose primary label belongs to one
layout use `HP39_*`, `HP48_*`, or `HP4950_*` identities. Raw GPF4/5/6
remain physical input names because the HP39 labels differ from HP48/49/50.

HP39g+ deliberately returns `HPCLANG_KEY_UNKNOWN` until its real-machine
fixture exists.

The keyboard service deliberately stops at physical key identity plus modifier
state. It does not translate model-specific combinations such as HP39
`Shift+VARS` into application actions such as `CHARS`. That mapping belongs
to a higher UI/shortcut or compatibility layer. Sticky Shift/Alpha policy and
text translation therefore remain outside the raw/debounce event service.
HPGCC3 combined those policies with an asynchronous message queue; hpclang
keeps them above the physical/debounce layer. A future compatibility adapter
can synthesize HPGCC-style `KEYDN/PRESS/KEYUP`, repeat, and shift-plane
behavior from hpclang events without making those historical policies the core
API.

### Time

All new time APIs must name or encode their unit. Avoid historical interfaces
whose integer meaning depends on timer source or model clock.

A monotonic time base should be the foundation for delays, deadlines, and
event timing.

### Storage and stdio

Prefer making picolibc's standard C interfaces work where their semantics are
appropriate instead of inventing parallel replacements for `fopen`,
`fread`, and friends.

Calculator-specific volumes, ports, or object stores may need a lower service
layer beneath those standard interfaces.

### Saturn/RPL integration

Saturn addresses and RPL objects should not be exposed as ordinary C pointers.

Use explicit address/object types or opaque handles, with conversion/copy
operations whose ownership and lifetime are documented.

### Capabilities

A program should be able to query genuine model/platform capabilities such as
storage, transfer mechanism, display geometry, or optional stock-OS services.

Capability queries must describe real differences, not become a substitute for
clean platform implementations.

## Modern C++ profile

hpclang currently uses Arm Toolchain for Embedded 22.1.0, Clang/LLVM, libc++,
libc++abi, compiler-rt, and picolibc.

The repository already link-tests:

- a C++ `main()`;
- global/static C++ construction;
- `std::string`;
- libc++ linked into an ARMv4T HPC1 image.

Current build flags deliberately use:

```text
-fno-exceptions
-fno-rtti
```

Therefore current support should be described as a modern embedded C++ profile,
not unrestricted hosted C++.

Compiler/language features that do not require unavailable OS facilities are
expected to work, but the SDK must explicitly test the C++ standard level it
claims.

Current C++23 validation covers:

- an explicit `-std=c++23` compile/link smoke;
- templates, concepts, lambdas, constexpr/consteval;
- `std::array`, `std::span`, `std::optional`, `std::string`, and
  `std::vector`;
- heap-backed standard-library objects;
- global construction;
- header-only wrappers over the real monotonic-time and display-surface M7
  services.

The visible emulator payload now contains a C++23 translation unit that queries
the real display surface and exercises monotonic time/delay before returning to
the C validation program. Five-model runtime closure for that probe is tracked
separately from compile/link coverage.

Exceptions, RTTI, threading, locale-heavy facilities, iostream/file streams,
and `std::filesystem` are separate capabilities. They should not be claimed
until their runtime requirements are implemented and tested.

## First implementation slice

The historical compatibility matrix now exists, so M7.1 uses this vertical
slice:

```text
device/capability facts
+ monotonic time
+ keyboard
+ display/surface
```

The stable device-information API and stock-preserving monotonic microsecond
clock are already implemented. The time service does not reconfigure the stock
PLL, Timer0, RTC, or IRQ state; delays busy-wait on that clock and return rather
than deadlock if the stock clock service becomes unavailable.

Raw keyboard state, the polled debounce/repeat event service, and complete
physical-key identities for the five automated models are now implemented.
The initial common scan probe is retained as evidence, while documented
model-specific keys come from the HPGCC/HP39 tables. Direct E2E checks both a
common ENTER identity and one family-specific identity through the same public
event path. Text/Alpha/Shift-plane translation remains a separate higher layer.

The first opaque display surface and clipped clear/pixel-readback/pixel-write/
line/outline-rectangle/fill-rectangle primitives form the current primitive
set. The previously implemented subset passes the five-model Direct matrix;
new primitives close through the same gate before being marked validated. Rasterization is separated
from physical-display acquisition and has host tests for packed-pixel padding,
rectangle clipping, and extreme off-screen line clipping. The visible runtime
validation page draws through this API instead of accessing the LCD framebuffer
directly. Broader HPG compatibility still requires HPG-compatible image allocation and
blit adapters, fonts/text, clip state, and additional drawing primitives.

Together these services are sufficient for a small interactive program and can
be validated without first solving persistent storage or RPL object lifetime.

The first slice should end with one non-trivial C example and one C++ example
running through the normal E2E path.
