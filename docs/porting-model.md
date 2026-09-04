# Minimal-port model

The purpose of the HPGCC archaeology is to identify the smallest compatibility surface required by modern Clang/LLVM.

## Layer 0 — language tooling

Prefer stock, current Clang and clangd.

For the ARMv4T-era targets the common front-end baseline is approximately:

```
--target=arm-none-eabi
-march=armv4t
-marm
-ffreestanding
-fno-builtin
-fno-stack-protector
-fno-unwind-tables
-fno-asynchronous-unwind-tables
```

Exact optimization and section flags belong to the build profile rather than clangd semantics.

## Layer 1 — ABI and startup

Historical HPGCC mixes several assumptions:

- ARMv4T / ARM state;
- old APCS-era compiler options;
- a dedicated PIC base register (`r10/sl`) in 1.x/2.x code;
- special non-PIC MMU loader code;
- calculator-specific startup and state save/restore;
- in HPGCC3, a ROMLIB call ABI.

These are the highest-risk pieces for full source compatibility.

A key distinction:

- modern Clang has already produced **hardware-working 39gs application objects** in 39gsX;
- that does **not yet prove binary-compatible replacement of every old HPGCC library object or every `r10`-based global-access convention**.

Full legacy-library compatibility therefore needs ABI tests, not assumptions.

## Layer 2 — ELF layout

Keep calculator-required layout in linker scripts rather than compiler patches.

For the hardware-proven 39gs path, modern LLD needs only small normalization of the old HPGCC script:

- pass MMU startup objects explicitly instead of legacy `STARTUP()/INPUT()`;
- express `__data_start` using `LOADADDR(.got) + SIZEOF(.got)`;
- keep the end of copied data word-aligned;
- discard Clang-generated `.ARM.exidx*` / `.ARM.extab*` sections when using the historical `elf2hp` copying model.

These are linker/runtime compatibility changes, not reasons to fork Clang.

## Layer 3 — post-link conversion

The historical ELF-to-calculator conversion algorithm should be preserved or reimplemented with regression fixtures.

For HPGCC 1.x/2.x this includes the HP string conversion path (historically `elf2hp`).

For HPGCC3 it includes the CC3/EXT3 executable format and its ROMLIB-version metadata.

## Layer 4 — calculator shell/package

This layer is calculator-specific and must not be collapsed into a generic “HPGCC target.”

Examples:

- 39g+/39gs/40gs: HP39 aplet structure, directory files, ARMToolbox39/library 275.
- 49g+/50g HPGCC 1/2: RPL/ARM launcher conventions.
- 50g HPGCC3: custom ROM + ROMLIB + CC3 launcher.

## When a Clang patch is justified

A Clang/LLVM fork is a last resort.

Create a compiler patch only when a required historical behavior:

1. is observable on hardware or required by an existing binary ABI;
2. cannot be expressed by supported target/ABI/codegen flags;
3. cannot be isolated in startup code, a thunk, a runtime helper, or linker transformation;
4. has a regression test demonstrating the missing behavior.

The most likely area requiring investigation is the old HPGCC 1.x/2.x `r10` single-PIC-base convention. Until tested, hpclang must distinguish “modern source compilation works” from “drop-in ABI compatibility with all old HPGCC libraries.”
