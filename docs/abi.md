# ABI migration findings

This document records which historical HPGCC compiler switches still need a
compiler change and which have direct modern Clang equivalents.

## Historical HPGCC 1.x/2.x code model

The 1.1 source tree builds normal HPGCC objects with:

    -mapcs-32
    -fPIC
    -mpic-register=r10
    -msingle-pic-base
    -msoft-float
    -mlittle-endian
    -fomit-frame-pointer

The MMU loader is deliberately different. Its own source says it must be built
without the normal PIC model and with r10 unavailable to the allocator:

    -mapcs-32
    -ffixed-r10
    -fno-inline

This distinction matters: preserving r10 for the loader and replacing the
single-PIC-base model for newly compiled code are separate compatibility
problems.

## What stock current Clang already covers

### APCS

Old GCC's `-mapcs-32` selects the 32-bit APCS calling convention. Current
Clang accepts:

    -mabi=apcs-gnu

This is the correct starting point for binary-interop probes against historical
HPGCC objects.

### Soft float

Use the current spelling:

    -mfloat-abi=soft

rather than relying on the historical `-msoft-float` spelling.

### ARMv4T / ARM state / little endian

Stock Clang already expresses these directly:

    --target=arm-none-eabi
    -march=armv4t
    -marm
    -mlittle-endian

### A standard single-static-base model: RWPI

Current LLVM already implements ARM RWPI:

    -frwpi

Writable globals use `R_ARM_SBREL32` relocations and are addressed relative
to r9/sb. On ARMv4T the generated access pattern is the same broad class of
operation HPGCC needs: load an offset and access data relative to a persistent
static-base register.

This gives hpclang a much smaller migration route than reproducing GCC's old
PIC backend.

## What the historical objects actually do

The original GCC 3.4.1 `MMUglobals.o` from the HPGCC 1.1 39G SDK was
inspected directly.

It uses:

- `R_ARM_GOT_BREL` relocations for globals;
- r10/sl as the GOT base;
- an extra GOT indirection before accessing each global.

A newly compiled Clang RWPI object uses:

- `R_ARM_SBREL32`;
- r9/sb as the static base;
- direct static-base-relative access to the writable symbol.

These are not binary-identical models, but LLD can resolve both relocation
families in one ELF.

## Dual static-base bridge

The preferred experiment is now:

    legacy HPGCC object:  r10 -> historical GOT/static area
    modern Clang object:  r9  -> same RAM image base

The original HPGCC MMU loader has already established r10 as 0x09000400 before
calling `_start`. A tiny assembly entry bridge can mirror it before any
Clang-generated C code executes:

    _start:
        mov r9, r10
        b hpclang_start

The real C entry is then compiled with:

    -frwpi -mabi=apcs-gnu -mfloat-abi=soft

This is compatible with the APCS register model: r9/sb/v6 and r10/sl/v7 are
persistent/callee-preserved in the relevant variants.

### Linker requirement

This only works if LLD sees the complete HPGCC RAM image as one RW PT_LOAD
starting at 0x09000400.

The RAM segment must contain, in historical runtime order:

    .got + .rodata + .data + .bss

If LLD splits .got and .data into different PT_LOAD segments, an SBREL
relocation for a modern data symbol is based on the later data segment rather
than 0x09000400. In that layout `mov r9, r10` would be wrong.

Adding explicit PHDRS to keep the HPGCC RAM image in one RW load segment fixes
this. In the host experiment a modern global at 0x09000420 correctly resolves
to SBREL offset 0x20 while the old GCC object continues to use r10 and its GOT.

## Full old-loader host experiment

The experiment has progressed beyond linking two ordinary objects.

The following link succeeds with no unresolved runtime symbols:

- original HPGCC 1.1 `MMUloader.o`;
- original HPGCC 1.1 `MMUglobals.o`;
- the r10-to-r9 entry bridge;
- a Clang ARMv4T/APCS/RWPI C entry.

The linked call path is:

    old _loader
      -> old c_loader / _init_globals
      -> bridge _start
      -> modern hpclang_start

The ELF has:

- one RX code PT_LOAD;
- one RW RAM PT_LOAD rooted at 0x09000400;
- old r10/GOT accesses and new r9/SBREL accesses in the same image.

See `experiments/hybrid-rwpi/`.

This is strong host-side evidence, but it is not yet a replacement for the
existing real-hardware 39gsX golden path.

## Remaining stock-Clang gap

### -ffixed-r10 on ARM

Current Clang/LLVM supports reserving r9 on ARM, exposed as
`-ffixed-r9` / `+reserve-r9`, but not r10.

Historical `MMUloader.c` explicitly asks for `-ffixed-r10`.

This gap matters only if hpclang wants source-identical recompilation of that
historical loader. It is no longer a prerequisite for ordinary new application
code if the dual-base RWPI model validates on hardware.

## Two compatibility levels

### source-modern

For new applications:

- current Clang ARMv4T codegen;
- `-frwpi` for writable static data when legacy-runtime interop is needed;
- tiny r10-to-r9 entry bridge;
- original calculator startup/packaging objects where still required;
- the existing 39gsX build remains the production/golden baseline until RWPI
  is hardware-tested.

### legacy-abi

For interoperation with GCC-built HPGCC libraries/objects:

- `-mabi=apcs-gnu`;
- `-mfloat-abi=soft`;
- keep r10 valid for old objects;
- keep r9 valid for new RWPI objects;
- ABI fixture tests across old/new calls and callbacks;
- preserve the one-RAM-segment linker invariant.

Do not claim full legacy ABI compatibility until those tests pass.

## Compiler patch policy

The priority has changed after the RWPI experiment.

A tiny ARM `-ffixed-r10` patch remains technically reasonable for rebuilding
the historical MMU loader from unmodified source, but **hpclang should not
require an LLVM fork for normal application development** unless hardware tests
disprove the dual-base approach.

Reimplementing GCC's old `-mpic-register=r10 -msingle-pic-base` backend is
currently rejected as unnecessary complexity. Stock LLVM RWPI plus a startup
bridge and linker-layout normalization is the preferred path.
