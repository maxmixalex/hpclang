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

Calculator-model policy must not leak into application code unless an actual user-visible hardware difference requires it. Platform differences remain below the HPC1 boundary; model identity is passed separately in the boot record.

## Platform families and model profiles

The platform family is the implementation unit for launch and low-level integration:

- `hp39` — HP 39g+, HP 39gs, HP 40gs;
- `hp48` — HP 48gII;
- `hp4950` — HP 49g+, HP 50g.

A model profile is narrower. It records real per-model facts such as model ID, firmware source, bootloader identity, LCD height, or stock transfer sequence. It must not create a runtime fork merely because a calculator has a different product name.

## Common platform services

Display, keyboard, timers, storage, and calculator-object/stack integration are planned behind common APIs. They should be added only after a real application needs them and after the affected families have a stable execution path.

The project does not copy the historical HPGCC API surface wholesale. Historical names or services are kept only when they describe an external calculator interface that hpclang must actually interoperate with.

## Non-ROM policy

The supported SDK must work through ordinary calculator software mechanisms. ROM replacement or ROM patching may be studied as an optional research or compatibility environment, but it is not part of the normal hpclang architecture.

## Historical execution models

Historical HPGCC launchers are specifications and test evidence. APCS-GNU, r9/r10 static-base conventions, ROPI/RWPI experiments, and Launcher V3 application ABIs are not supported hpclang application modes.

Those paths were useful during bring-up because they established that stock Clang/LLD can coexist with the calculator launch environment and because they exposed the underlying ARMSYS/MMU behavior. The production architecture deliberately moves those details behind the fixed-address HPC1 ABI instead of making applications reproduce historical compiler conventions.
