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

Off-screen owned surfaces, blitting, fonts, clipping state, grayscale/double
buffering, and composite modes remain later display work. None of them should
require exposing the physical framebuffer representation.

### Keyboard

Separate raw physical-key state from higher-level key events.

The first stable layer is now `<hpclang/keyboard.h>`. A snapshot is exactly
eight bytes and uses an hpclang-specific raw encoding:

- bits 0–55 are the 8×7 S3C2410 matrix, packed as
  `column * 7 + row`;
- bits 56–59 are the dedicated ON, Alpha, left-Shift, and right-Shift inputs.

Those matrix coordinates are physical scan positions, **not** portable key
meanings. HPGCC 1.1 itself documents its logical row/column table as HP49g+
specific, with HP39g+ different and HP48gII not verified. hpclang therefore
does not make the historical HPGCC `keymatrix` bit numbering its core ABI.

The raw reader is synchronous and does not install an IRQ, alter the clock
tree, or retain GPIO changes after it returns. It is suitable for games and
other callers that need instantaneous state.

The second layer is a synchronous, polled event service. It debounces the raw
snapshot and emits `DOWN`, `UP`, and `REPEAT` events. Repeat timing is
configurable, and each event carries the physically held ON/Alpha/left-Shift/
right-Shift modifiers.

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

It also does not implement sticky shift planes, Alpha lock, text translation,
or model-neutral key labels yet. HPGCC3 combined those policies with an
asynchronous message queue; hpclang keeps them above the physical/debounce
layer. A future compatibility adapter can synthesize HPGCC-style
`KEYDN/PRESS/KEYUP`, repeat, and shift-plane behavior from hpclang events
without making those historical policies the core API.

Model-neutral key identities will be added only after scan mappings have been
measured across `hp39`, `hp48`, and `hp4950`, rather than inferred from
the documented HP49g+ layout.

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

Raw keyboard state and the polled debounce/repeat event service are now
implemented and pass the five-model Direct matrix. Model-neutral key
translation remains deliberately pending until cross-family scan mappings are
measured.

The first opaque display surface and clipped clear/pixel/line/fill-rectangle
primitives also pass the five-model Direct matrix. Rasterization is separated
from physical-display acquisition and has host tests for packed-pixel padding,
rectangle clipping, and extreme off-screen line clipping. The visible runtime
validation page draws through this API instead of accessing the LCD framebuffer
directly. Broader HPG compatibility still requires owned images, blitting,
fonts, clip state and additional drawing primitives.

Together these services are sufficient for a small interactive program and can
be validated without first solving persistent storage or RPL object lifetime.

The first slice should end with one non-trivial C example and one C++ example
running through the normal E2E path.
