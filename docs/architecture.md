# Architecture

hpclang is a clean LLVM-native SDK for the ARM-based HP graphing calculators.
It is not a selectable collection of historical HPGCC runtimes.

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

The normal build has no dependency on GCC, libgcc, historical HPGCC object
files or a replacement calculator ROM.

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
+-------------+-------------+
|             |             |
HP39/40      HP48gII       HP49g+/50g
platform     platform      platform
|             |             |
calculator OS / ARMSYS / hardware
```

The common runtime owns language/runtime concerns: startup, data/BSS, TLS,
constructors, compiler helpers, heap and libc primitives.

A platform backend owns calculator-specific work: native packaging, physical
memory discovery, Saturn-to-ARM transition, MMU setup, boot-info construction,
state restoration and low-level services.

## Common bootstrap

The common ARM bootstrap receives a small family-specific launch context,
installs the HPC1 virtual layout, enters the application, restores the original
mappings and returns to the calculator OS.

Calculator-model policy must not leak into application code. Family differences
are represented below the HPC1 boundary.

## Platform services

Display, keyboard, timers, storage and calculator-object/stack integration will
be added behind common APIs only after each family has a stable execution path.
The project does not copy the historical HPGCC API surface wholesale.

## Non-ROM policy

The supported SDK must work through ordinary calculator software mechanisms.
ROM replacement or ROM patching may be studied as an optional compatibility
environment, but it is not part of the normal hpclang architecture.

## Historical code

Historical HPGCC launchers are specifications and test evidence. Obsolete
APCS-GNU, r9/r10 static-base, ROPI/RWPI and Launcher-V3 application ABIs are
not supported hpclang application modes. Useful historical ideas are
reimplemented behind the current fixed-address HPC1 ABI.
