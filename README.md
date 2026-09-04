# hpclang

A clean LLVM-native SDK for the ARM-based HP graphing calculators.

hpclang is not an HPGCC compatibility layer. Historical HPGCC source is used
to understand the hardware, calculator OS and launch path, but new
applications use one hpclang ABI regardless of which historical SDK once
supported a calculator.

## Targets

The native ARM targets are:

- HP 39g+, 39gs and 40gs;
- HP 48gII;
- HP 49g+ and 50g.

Saturn-only calculators are outside the native runtime scope.

## One application ABI

Every supported target uses the same application-side contract:

- ARMv4T;
- AAPCS32;
- soft float;
- Thumb-1 C/C++ by default;
- ARM startup where useful;
- fixed virtual code and writable-memory addresses;
- no PIC/GOT static-base convention;
- no reserved r9/r10;
- no HPGCC object files;
- no GCC or libgcc.

The current v1 virtual layout is:

```
0x01220000  executable image
0x01100000  launcher boot page
0x01101000  .data / .bss / heap
             ...
             stack at top of mapped writable arena
```

The model-independent post-link image is an `HPC1` file. Calculator-specific
packaging is a separate layer.

## Architecture

```
              application source
                     |
             Clang / LLVM / LLD
                     |
                hpclang v1
        AAPCS32 + fixed virtual layout
                     |
                   HPC1
                     |
       +-------------+-------------+
       |             |             |
    HP39/40        48gII        49g+/50g
    platform       platform      platform
    backend        backend       backend
       |             |             |
       +------ calculator OS / ARMSYS ------+
```

The platform backend is responsible for finding physical memory, building the
MMU mappings, filling the boot page, entering ARM code and wrapping the HPC1
image in the calculator's native storage/package format. None of those details
change the application ABI.

## Current status

The unified core is already host-validated in CI:

- stock Clang integrated assembler;
- AAPCS32 Thumb-1 C code;
- ARM/Thumb interworking;
- fixed virtual RX/RW ELF layout through LLD;
- model-independent HPC1 packing;
- no unresolved target symbols for the core smoke image;
- no GCC, libgcc or HPGCC target binary.

A separate HP39 Launcher-V3 path remains in the repository only because it is
the current real-hardware bootstrap route. It is not the long-term hpclang ABI.

The next implementation layers are:

1. LLVM compiler-rt builtins;
2. common libc/runtime primitives;
3. the three calculator platform backends;
4. hardware validation on each model family;
5. graphics, keyboard, timers, filesystem and calculator-object APIs.

See [hpclang v1 ABI](docs/abi-v1.md), [target families](docs/targets.md), and the maintained [roadmap](docs/ROADMAP.md).
