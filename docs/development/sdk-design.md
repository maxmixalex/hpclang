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

### Keyboard

Separate raw physical-key state from higher-level key events.

Historical APIs often mix scanning, debounce, repeat, and translated key
semantics. hpclang should make those layers explicit so games can request raw
state while ordinary applications can consume events.

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

The next C++ validation work should cover:

- an explicit `-std=c++23` compile/link smoke;
- templates, concepts, lambdas, constexpr/consteval;
- `std::array`, `std::span`, `std::optional`, `std::string`, and
  `std::vector`;
- allocation/new/delete at runtime;
- global construction and destruction on the emulator;
- C++ wrappers over at least one real M7 service.

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

Keyboard is next and must keep raw physical state separate from translated
events. Display/surface follows and must be capable of implementing the stable
HPG drawing surface without making its historical framebuffer representation
the core ABI.

Together these services are sufficient for a small interactive program and can
be validated without first solving persistent storage or RPL object lifetime.

The first slice should end with one non-trivial C example and one C++ example
running through the normal E2E path.
