# HPGCC 1.1 hybrid RWPI experiment

This experiment tests whether unmodified historical HPGCC 1.1 MMU runtime
objects can coexist with newly compiled Clang code without reproducing GCC's
old `-mpic-register=r10 -msingle-pic-base` code generator.

## Model

Historical HPGCC objects keep their original convention:

- r10/sl = HPGCC GOT base at 0x09000400;
- globals use R_ARM_GOT_BREL relocations.

New Clang objects use the standard ARM RWPI model:

- r9/sb = read-write static base;
- writable globals use R_ARM_SBREL32 relocations;
- compile with `-frwpi -mabi=apcs-gnu -mfloat-abi=soft`.

The entry bridge mirrors the already-established HPGCC static base:

    _start:
        mov r9, r10
        b hpclang_start

Both r9 and r10 are callee-preserved in the relevant APCS model, so legacy
objects may continue using r10 while modern RWPI objects use r9.

## Important linker constraint

LLD computes ARM SBREL relative to the PT_LOAD segment containing the writable
symbol. Therefore the historical RAM image must be represented as a single
load segment beginning at 0x09000400.

The experiment linker script puts:

    .got + .rodata + .data + .bss

in one RW PT_LOAD. This also matches the old MMUloader's actual copy/zero model:
it copies GOT + the initialized data tail into RAM and zeros BSS immediately
afterward.

Without the explicit common RAM PHDR, LLD may split .got, .rodata and .data
into separate PT_LOAD segments. In that case SBREL for a new .data symbol is
relative to the later .data segment, not to 0x09000400, and merely copying
r10 into r9 is wrong.

## What has already been host-verified

Using the original GCC 3.4.1 objects from the HPGCC 1.1 39G SDK:

- MMUglobals.o uses R_ARM_GOT_BREL and r10.
- A Clang ARMv4T RWPI object uses R_ARM_SBREL32 and r9.
- LLD links both relocation families in the same ELF.
- The original MMUloader.o and MMUglobals.o link with the bridge and modern
  RWPI C entry without unresolved runtime symbols.
- With one RAM PT_LOAD at 0x09000400, a modern global located at 0x09000420
  receives SBREL offset 0x20.
- The resulting entry sequence is:
  old _loader -> bridge _start -> hpclang_start.

This is not yet a real-hardware proof. The existing 39gsX non-RWPI hybrid
build remains the production/golden 39gs baseline until this experiment is
tested on calculator hardware.

## Why this matters

If hardware validation succeeds, arbitrary new hpclang code can get a
single-static-base global model using stock LLVM. We would not need to add
ARM -ffixed-r10 support or resurrect GCC's historical single-PIC-base backend
for new code.

A tiny reserve-r10 LLVM patch would then only be useful for source-identical
recompilation of MMUloader.c, not for ordinary hpclang applications.
