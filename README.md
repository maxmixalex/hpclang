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
| `hp39gs-hpgcc11` | HPGCC 1.1-p1 with 39G support | 39g+/39gs/40gs | **hardware-proven baseline exists in 39gsX** |
| `hp49g50g-hpgcc2` | HPGCC 2.0 / SP2 | 49g+/50g | historical diff in progress |
| `hp50g-hpgcc3` | HPGCC3 R004.revA | 50g + HPGCC3 ROMLIB | historical diff in progress |

39gs support is a first-class target. It must not be dropped merely because later HPGCC releases concentrated on the 49/50 family.

## Current design rule

Do **not** fork Clang unless a calculator requirement cannot be expressed through normal target flags, runtime objects, linker scripts, post-link tools, or small ABI shims.

The current 39gs hardware-proven path already builds modern ARMv4T objects with Clang and links them with HPGCC's original MMU startup objects using LLD. That is the starting point, not something to replace.

See:

- [HPGCC lineage](docs/lineage.md)
- [Porting model](docs/porting-model.md)
- [39gs compatibility contract](docs/39gs.md)

## Repository status

This repository is currently in the archaeology/minimal-port phase. Historical binaries and copyrighted calculator payloads are intentionally not vendored until their provenance and redistribution terms have been audited.
