# Retired experiments

Several bring-up experiments were removed from the working tree after the production HPC1 architecture made them unnecessary. Their conclusions remain relevant because they explain why hpclang does not reproduce historical GCC code-generation conventions.

Git history remains the source for the original files.

## Legacy Launcher V3 bootstrap

The removed `research/legacy-l3/` directory preserved a clean HP39 Launcher V3/MMU experiment.

Its purpose was to prove that stock Clang and LLD could replace the historical GCC target toolchain while still entering through the calculator's historical Launcher V3/MMU environment.

The experiment succeeded as architectural evidence, but it is not a supported hpclang execution mode. Production uses HPC1 plus the unified fixed-virtual hpclang v1 ABI, so no current target profile or CI workflow needs the old standalone Launcher V3 implementation.

## Hybrid HPGCC 1.1 / Clang RWPI experiment

The removed `experiments/hybrid-rwpi/` experiment tested whether unmodified historical HPGCC 1.1 MMU runtime objects could coexist with newly compiled Clang code without reproducing GCC's old `-mpic-register=r10 -msingle-pic-base` code generator.

Historical HPGCC objects use:

- r10/sl as the HPGCC GOT base at `0x09000400`;
- `R_ARM_GOT_BREL` relocations for globals.

The Clang RWPI experiment used:

- r9/sb as the read-write static base;
- `R_ARM_SBREL32` relocations;
- `-frwpi -mabi=apcs-gnu -mfloat-abi=soft`.

The bridge was:

```asm
_start:
    mov r9, r10
    b hpclang_start
```

Both r9 and r10 are callee-preserved in the relevant APCS model, so this allowed legacy objects to keep r10 while new Clang objects used r9.

### Linker constraint

LLD computes ARM SBREL relative to the PT_LOAD segment containing the writable symbol. Therefore the historical RAM image had to be represented as a single load segment beginning at `0x09000400`.

The experiment placed:

```text
.got + .rodata + .data + .bss
```

in one RW PT_LOAD.

Without that common RAM PHDR, LLD could split `.got`, `.rodata`, and `.data` into separate PT_LOAD segments. In that case an SBREL relocation for a new `.data` symbol would be relative to the later data segment rather than `0x09000400`, and merely copying r10 into r9 would be wrong.

### Host-verified results

Using original GCC 3.4.1 objects from the HPGCC 1.1 39G SDK:

- `MMUglobals.o` uses `R_ARM_GOT_BREL` and r10;
- a Clang ARMv4T RWPI object uses `R_ARM_SBREL32` and r9;
- LLD links both relocation families in one ELF;
- historical `MMUloader.o` and `MMUglobals.o` link with the bridge and a modern RWPI C entry without unresolved runtime symbols;
- with one RAM PT_LOAD at `0x09000400`, a modern global at `0x09000420` receives SBREL offset `0x20`;
- the resulting entry sequence is `old _loader → bridge _start → hpclang_start`.

This experiment was never the desired final architecture. Once the common fixed-address HPC1 runtime was established, keeping a hybrid historical runtime in the production path would add complexity without benefit.

## Modern Clang compatibility probe

The generated Clang probe established two useful facts.

First, contemporary Clang does **not** accept the old GCC option:

```text
-ffixed-r10
```

The observed diagnostic was:

```text
clang: error: unknown argument '-ffixed-r10'; did you mean '-ffixed-r19'?
```

Second, Clang can still emit ARM APCS-GNU objects and ordinary ARM PIC code, but its normal PIC code generation uses modern GOT-relative relocations rather than reproducing the historical HPGCC single-static-base convention.

This is one reason hpclang does not attempt to make modern Clang source-identical with the old HPGCC ABI.

## HPGCC 1.1 → 2.0 source comparison

The generated inventories and tag-comparison files were removed because they were bulky reproducible analysis output, not source.

The durable conclusion is important: HPGCC 2.0 is not a strict superset of HPGCC 1.1 for the calculators hpclang supports.

The 2.0 transition removes, among other active 1.1 paths:

- `sources/HP39-Aplet/` including its binaries, skeleton, `hp2aplet`, and library 275 source;
- active HP48-specific ARMToolbox source under `masd_source48`;
- the original MMU loader/linker path used by the earlier release.

Later source may contain attic copies or related binaries, but that does not make the 2.0 runtime a backwards-compatible family bundle.

This finding is why hpclang uses multiple historical HPGCC generations as evidence while maintaining one new LLVM-native runtime.

## Rejected compressed Font Gallery staging (`HPZ1`)

During Font Gallery bring-up, an experimental launcher-private staging envelope named `HPZ1` was used to test whether compressing a large standard HPC1 image could preserve a single exhaustive Gallery payload through the historical ARMToolbox/Launcher V3 object boundary.

The experiment was intentionally internal: `e2e.hpc` remained a normal HPC1 image, and `HPZ1` was never an application ABI or public package format. A small raw-LZ4 encoder compressed the Gallery body and stage1 expanded it directly into final code pages.

The experiment proved two useful points:

- the historical wrapped object-size limit can be bypassed by compressing the launcher payload;
- doing so does **not** solve the real stock-launch resource limit. HP50g reached the native launcher path but reported `Error: Insufficient Memory` before hpclang entered, even though the compressed transfer object fit the wrapper.

ELF accounting also showed why compiler flags are not a substitute for validation architecture here: after section GC and Gallery-only read-only ICF, about 96% of the roughly 795 KiB exhaustive image was font/catalog asset data rather than optimizable program code. `-Oz` or LTO could not plausibly make that one payload small enough for the stock path.

`HPZ1` was therefore removed. Production and validation use standard HPC1 only. Exhaustive Direct Gallery validates every generated face with a synthetic test resource window sized for the existing HPC1 code-page ceiling, while a compact standard-HPC1 Stock Gallery validates the real native UI → ARMToolbox → HPC1 launch path. This keeps exhaustive font coverage and stock-launch integration as separate claims without creating a new package format for a test workload.
