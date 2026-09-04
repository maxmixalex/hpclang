# hpclang

Modern Clang/clangd tooling for classic HP ARM graphing calculators, derived from the calculator-specific parts of HPGCC 1.1 through HPGCC 3.0.

The project deliberately separates four things that old HPGCC distributions bundled together:

1. the C/C++ compiler and language tooling;
2. the ARM ABI/startup/runtime contract;
3. ELF layout and calculator loader requirements;
4. calculator-specific packaging and runtime installation.

The goal is to keep only the minimum HP-specific compatibility layer and use current Clang/LLVM tooling everywhere else.

## Target profiles

| Profile | Historical baseline | Calculator family | Status |
| --- | --- | --- | --- |
| `hp39gs-hpgcc11` | HPGCC 1.1-p1 with 39G support | 39g+/39gs/40gs | **hardware-proven source-modern baseline exists in 39gsX** |
| `hp39gs-hpgcc11-rwpi` | HPGCC 1.1-p1 runtime + Clang RWPI | 39g+/39gs/40gs | host-proven legacy-interop experiment; hardware validation pending |
| `hp49g50g-hpgcc2` | HPGCC 2.0 / SP2 | 49g+/50g | official-tag archaeology in progress |
| `hp50g-hpgcc3` | HPGCC3 R004.revA | 50g + HPGCC3 ROMLIB | historical diff in progress |

39gs support is a first-class target. It must not be dropped merely because later HPGCC releases concentrated on the 49/50 family.

## Current design rule

Do **not** fork Clang unless a calculator requirement cannot be expressed through stock target flags, runtime objects, linker scripts, post-link tools, or small ABI shims.

Two distinct 39gs paths are intentionally retained:

### Hardware-proven source-modern baseline

The existing 39gsX path builds new ARMv4T code with stock Clang and links it with the historical HPGCC 1.1-p1 39G MMU startup objects using LLD. This remains the golden 39gs baseline.

### Experimental legacy-object interop

Stock LLVM already has ARM RWPI, a static-base-relative model for writable data. A host-verified experiment keeps historical HPGCC objects on their original `r10`/GOT model while new Clang code uses `r9`/RWPI:

```
old MMUloader establishes r10 = 0x09000400
                  |
                  v
_start bridge: mov r9, r10
                  |
                  v
new Clang code: -frwpi -mabi=apcs-gnu -mfloat-abi=soft
```

With one RW PT_LOAD rooted at `0x09000400`, LLD successfully resolves both the historical `R_ARM_GOT_BREL` relocations and new `R_ARM_SBREL32` relocations in one image.

This suggests normal hpclang development may require **no LLVM fork at all**. Hardware validation is still required before the RWPI experiment can replace any proven 39gs build path.

See:

- [HPGCC lineage](docs/lineage.md)
- [Porting model](docs/porting-model.md)
- [ABI migration findings](docs/abi.md)
- [39gs compatibility contract](docs/39gs.md)
- [Hybrid RWPI experiment](experiments/hybrid-rwpi/README.md)

## Repository status

This repository is currently in the archaeology/minimal-port phase. Historical calculator/runtime binaries and packaging assets are intentionally not vendored until provenance and redistribution terms have been audited.
