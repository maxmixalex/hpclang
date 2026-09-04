# hpclang

Modern Clang/clangd tooling and runtime for the ARM-based HP graphing calculator family, informed by HPGCC 1.x, 2.x and 3.x.

hpclang is not intended to become a patched copy of one historical HPGCC release. The end state is a self-contained LLVM-native SDK that can build calculator-ready programs from source without an HPGCC SDK, historical HPGCC target binaries, GCC or libgcc.

## Intended calculator families

| Family | Models | Historical launcher lineage | hpclang direction |
| --- | --- | --- | --- |
| HP39/40 ARM | 39g+, 39gs, 40gs | HP39 aplet + ARMToolbox39 / library 275 | native backend; 39gs is the first hardware-validation machine |
| HP48 ARM | 48gII | dedicated ARMToolbox48 launcher | native backend |
| HP49/50 ARM | 49g+, 50g | ARMToolbox Launcher V3; HPGCC 2.0 VC | native backend with optional VC profile |
| HP49/50 ROMLIB | 49g+, 50g where a compatible HPGCC3 environment exists | HPGCC3 CC3/EXT3 + ROMLIB | optional compatibility backend; never required |

Older Saturn-only machines such as the 39G/40G, 48G family and 49G are outside the native-ARM scope of hpclang.

## Common executable baseline

The historical sources reveal a useful common denominator: Launcher V3 **simple/MMU format** (marker `0B34C`) with the M/R memory-packer contract exists in the normal ARMToolbox, the dedicated 48gII launcher and ARMToolbox39. HPGCC 2.0 retains this format while adding virtual-code addressing as an extension.

That gives hpclang a portable native-ARM baseline:

```
C / C++ / assembly
        |
        v
stock Clang + LLVM tools
        |
        v
ROPI/RWPI ARMv4T image
        |
        v
Launcher V3 simple/MMU payload
        |
        +----------------------+----------------------+
        |                      |                      |
   HP39/40 adapter         HP48gII adapter       HP49/50 adapter
   aplet + lib275          RPL + Toolbox48       RPL + Toolbox
```

Family-specific launchers and package formats remain separate. The common C runtime and source API should not depend on one calculator's Saturn memory map.

## Optional compatibility profiles

Later HPGCC features remain valuable and should be preserved where the platform supports them:

- **HPGCC 2.0 VC:** fixed virtual code/data addresses, Launcher V3 v1.2 flags, ARM/Thumb interworking, no historical r10 single-PIC-base requirement.
- **HPGCC3:** CC3/EXT3 executable format, versioned ROMLIB ABI and a much richer runtime/library surface.

These are additive backends, not the definition of hpclang itself. No hpclang target will require a ROM replacement.

## Current status

The first clean bootstrap is the HP39/40 backend because real 39gs hardware and a hardware-proven historical launch path are already available. It currently proves:

- Clang integrated assembler;
- Clang ARMv4T ROPI/RWPI C code;
- LLD;
- no HPGCC target object;
- no GCC/libgcc;
- direct ELF-to-Launcher-V3 packing.

This is a validation foothold, not a model-specific architecture decision.

See:

- [Target families](docs/targets.md)
- [Runtime architecture](docs/runtime-architecture.md)
- [Baseline decision](docs/baseline.md)
- [Library lineage](docs/library-lineage.md)
- [HPGCC lineage](docs/lineage.md)
- [ABI migration findings](docs/abi.md)
