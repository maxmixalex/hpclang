# Minimal-port model

The purpose of the HPGCC archaeology is to identify the smallest compatibility surface required by modern Clang/LLVM.

## Layer 0 — language tooling

Prefer stock, current Clang and clangd.

For the ARMv4T-era targets the common source-modern front-end baseline is approximately:

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

When binary interoperability with historical HPGCC objects is required, add the old ABI-compatible dimensions explicitly:

```
-mabi=apcs-gnu
-mfloat-abi=soft
```

## Layer 1 — ABI and startup

Historical HPGCC 1.x/2.x combines:

- ARMv4T / ARM state;
- APCS-era function ABI;
- a dedicated `r10/sl` PIC/GOT base;
- special non-PIC MMU loader code;
- calculator-specific startup and state save/restore.

HPGCC3 is different again and uses a ROMLIB/launcher ABI.

### The important new result

Current LLVM already implements ARM RWPI: writable data is addressed relative to a static-base register using `R_ARM_SBREL32`. ARM RWPI standardizes that base as `r9/sb`.

The original HPGCC 1.1 object `MMUglobals.o` was inspected directly and uses `R_ARM_GOT_BREL` with `r10/sl`.

These two code models can coexist:

```
legacy GCC objects        modern Clang objects
r10 + GOT_BREL            r9 + SBREL32
       \                    /
        \                  /
         same HPGCC RAM image
              0x09000400
```

The historical loader establishes r10 before calling `_start`. A tiny assembly bridge can execute `mov r9, r10` and tail-branch into Clang RWPI code.

This means reproducing GCC's old `-mpic-register=r10 -msingle-pic-base` backend is currently unnecessary.

## Layer 2 — ELF layout

Keep calculator-required layout in linker scripts rather than compiler patches.

For the hardware-proven 39gs source-modern path, modern LLD needs only small normalization of the old HPGCC script:

- pass MMU startup objects explicitly instead of legacy `STARTUP()/INPUT()`;
- express `__data_start` using `LOADADDR(.got) + SIZEOF(.got)`;
- keep the end of copied data word-aligned;
- discard Clang-generated `.ARM.exidx*` / `.ARM.extab*` sections when using the historical `elf2hp` copying model.

For the experimental RWPI interoperability path there is one additional invariant:

- `.got + initialized data + .bss` must belong to a **single RW PT_LOAD rooted at 0x09000400**.

LLD defines ARM SBREL relative to the writable load segment's static base. If it splits HPGCC's RAM image into several PT_LOAD segments, merely copying r10 to r9 is not sufficient.

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

## Compatibility levels

### source-modern

New application sources compiled with stock modern Clang while retaining only the historical calculator runtime/package pieces that are still needed.

The existing 39gsX route is already hardware-proven for this level.

### legacy-interop

New Clang objects and historical GCC-built HPGCC objects coexist in one image.

The RWPI/r9 + legacy-GOT/r10 experiment is host-proven at link/codegen level, but still requires real calculator testing and a broader ABI fixture matrix.

## When a Clang patch is justified

A Clang/LLVM fork is a last resort.

Create a compiler patch only when a required historical behavior:

1. is observable on hardware or required by an existing binary ABI;
2. cannot be expressed by supported target/ABI/codegen flags;
3. cannot be isolated in startup code, a thunk, a runtime helper, or linker transformation;
4. has a regression test demonstrating the missing behavior.

At present there is only one concrete small compiler gap: current ARM Clang supports `-ffixed-r9` but not `-ffixed-r10`. That matters for source-identical recompilation of historical `MMUloader.c`, but it is **not required for normal hpclang application code** if the dual-base RWPI route survives hardware validation.
