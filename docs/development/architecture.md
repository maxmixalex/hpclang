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

## Platform families and model profiles

The platform family is the implementation unit for launch and low-level integration:

- `hp39` — HP 39g+, HP 39gs, HP 40gs;
- `hp48` — HP 48gII;
- `hp4950` — HP 49g+, HP 50g.

A model profile is narrower. It records real per-model facts such as model ID, firmware source, bootloader identity, LCD height, or stock transfer sequence. It must not create a runtime fork merely because a calculator has a different product name.

HP39g+ sharing the `hp39` platform does not establish that it shares HP39gs/40gs flash identity. Its native physical-font layout remains blocked on a trustworthy real-machine full-flash fixture.

## Common platform services

The application-facing clean core now includes device information, stock-preserving monotonic time, raw/debounced keyboard events with five-model physical key identity, opaque display/software surfaces, clipped geometry and blit, calculator-native/generated fonts, UTF-8 text, and hp4950 byte-stream stdio through picolibc.

These services keep platform representation and ownership below common APIs. Calculator-object/stack integration is the next major service boundary: HP39 typed objects and HP48/49/50 Saturn/RPL objects must not be flattened into byte-stream files or exposed as raw stock addresses.

The project does not copy the historical HPGCC API surface wholesale. Historical names or services become adapters only when they describe useful source-level behavior over an already coherent hpclang core.

## Non-ROM policy

The supported SDK must work through ordinary calculator software mechanisms. ROM replacement or ROM patching may be studied as an optional research or compatibility environment, but it is not part of the normal hpclang architecture.

Read-only access to calculator-owned stock assets through launcher-private mappings does not change this policy: hpclang does not replace or modify the calculator ROM, and the mappings remain below the application ABI.

## Historical execution models

Historical HPGCC launchers are specifications and test evidence. APCS-GNU, r9/r10 static-base conventions, ROPI/RWPI experiments, and Launcher V3 application ABIs are not supported hpclang application modes.

Those paths were useful during bring-up because they established that stock Clang/LLD can coexist with the calculator launch environment and because they exposed the underlying ARMSYS/MMU behavior. The production architecture deliberately moves those details behind the fixed-address HPC1 ABI instead of making applications reproduce historical compiler conventions.
