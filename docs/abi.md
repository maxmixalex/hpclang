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

This distinction matters: preserving r10 for the loader and reproducing the
single-PIC-base model for normal legacy objects are two separate compatibility
problems.

## What stock current Clang already covers

### APCS

Old GCC's `-mapcs-32` selects the 32-bit APCS calling convention. Current
Clang accepts:

    -mabi=apcs-gnu

This should be used for binary-interop probes against historical HPGCC objects.
It is deliberately not forced on the existing 39gsX-derived source-modern
profile until that change is hardware/regression tested.

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

## What stock current Clang does not reproduce

### Historical r10 single-PIC-base codegen

Current Clang's ARM `-fPIC` codegen is PC-relative/GOT-based. It does not
implement GCC's old pair:

    -mpic-register=r10
    -msingle-pic-base

Therefore `-fPIC` is not a source-compatible spelling replacement for the
historical HPGCC global-data model.

This is the largest remaining question for arbitrary old HPGCC library binary
interoperability. It does not block the current 39gs source-modern path because
that path has already run on hardware while retaining the historical startup
objects.

### -ffixed-r10 on ARM

Current Clang/LLVM has target support for reserving r9 on ARM, exposed as
`-ffixed-r9` / `+reserve-r9`, but not r10.

This is the smallest concrete compiler gap found so far. HPGCC's historical
`MMUloader.c` explicitly requires `-ffixed-r10`.

An optional hpclang LLVM patch can therefore add only ARM r10 reservation,
without inventing a full custom HP target.

## Two compatibility levels

hpclang should expose these separately.

### source-modern

For new applications:

- current Clang ARMv4T codegen;
- modern ordinary PC-relative code/data accesses;
- calculator-specific historical startup objects and linker/package layers
  where still required;
- the 39gsX hardware-proven hybrid path is the baseline.

### legacy-abi

For interoperation with old GCC-built HPGCC libraries/objects:

- `-mabi=apcs-gnu`;
- `-mfloat-abi=soft`;
- ABI fixture tests across the old/new object boundary;
- investigate whether the old r10 single-PIC-base model needs a shim,
  recompilation of libraries, or additional LLVM support.

Do not claim full legacy ABI compatibility until those tests pass.

## Compiler patch policy

The experimental ARM r10 reservation patch is acceptable because it is:

1. required verbatim by historical HPGCC runtime source;
2. orthogonal to calculator packaging;
3. tiny and independently testable;
4. unnecessary for users who keep the known-good historical MMUloader object.

The much larger old GCC single-PIC-base model is **not** yet approved as an
LLVM patch. First try source recompilation, thunks and runtime/linker changes.
