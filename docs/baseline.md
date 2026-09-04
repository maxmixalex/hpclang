# hpclang baseline

## Decision

hpclang does **not** select HPGCC 1.1, 2.0 or 3.0 wholesale as its baseline, and it is not a 39gs-specific SDK.

The project baseline is assembled from four independently useful layers:

1. **Launcher V3 simple/MMU ABI** as the common historical native-ARM launch contract.
2. **Current LLVM ROPI+RWPI** as the normal code/data relocation model for newly built applications.
3. **Per-family calculator adapters** for HP39/40, 48gII and 49g+/50g packaging, Saturn address conversion, memory discovery and OS entry points.
4. **Optional later-generation profiles** for HPGCC 2.0 virtual-code addressing and HPGCC3 ROMLIB compatibility.

The first clean implementation happens to be on a 39gs because that is the first available hardware-validation path. It must not dictate the common ABI.

## Historical branch topology

The HPGCC version number is not a complete calculator-support ordering.

The evidence now points to this topology:

```
                    common HPGCC 1.x ARM work
                      /        |         \
                     /         |          \
              HP39/40       HP48gII       HP49g+
              branch        branch        mainline
                |             |              |
          1.1-p1 39G      Toolbox48          |
                                              v
                                      HPGCC 2.0 VC / SP2
                                              |
                                              v
                                      HPGCC3 49g+/50g
```

The official `frozen_2.0_VC_release` no longer contains the historical HP39-Aplet or dedicated 48gII ARMToolbox paths, while its VC crt0 source is present.

Therefore “use HPGCC 2.0 as the baseline” would silently regress earlier model-specific support.

## Historical target families

### HP39/40 ARM family

Reference material:

- HPGCC 1.1-p1 39G support;
- `HP39-Aplet/`;
- ARMToolbox39 / library 275;
- HP39 aplet and directory packaging.

Contemporary HP 39gs/40gs documentation records that HPGCC support was added for the 39gs and 40gs and later discontinued after the 2.0 transition. This independently explains why the 2.0 mainline cannot be treated as the complete HP39/40 source baseline.

The 39G launcher intentionally removed secondary HPGCC function-entry dispatch and retains the Launcher V3 M/R memory-packer path.

### HP48gII

Reference material:

- the HPGCC 1.1 source explicitly contains HP48gII compatibility work;
- ARMToolbox contains a separate `masd_source48/` implementation and `L275_48.LIB`.

Its `PRLNK` retains Launcher V3 function-table parsing, so 48gII historically supports multi-entry payloads even though its memory mapping differs from 49/50.

The frozen 2.0 VC tag no longer contains those dedicated 48 paths at their 1.1 locations. 48gII support therefore remains a distinct lineage to carry forward.

### HP49g+ / HP50g

Reference material:

- ordinary ARMToolbox;
- HPGCC 2.0 / SP2;
- HPGCC3.

HPGCC 2.0 SP2 is catalogued for the 49/50 platform. ARMToolbox 3.1 adds virtual code addressing, removes the old stack bias and improves cache flushing.

HPGCC3 documentation describes the 49g+/50g family and states that both use the same 512 KiB physical RAM organization.

## Common executable contract

Launcher V3 v1.1 simple format is implemented by:

- ordinary ARMToolbox;
- dedicated ARMToolbox48;
- ARMToolbox39.

HPGCC 2.0 Launcher V3 v1.2 keeps that simple format for assembler/MMU programs and adds a second virtual-code format.

Therefore the common hpclang payload should initially target the simple/MMU contract:

- marker `0B34C`;
- primary entry point;
- optional secondary entry table when the family launcher supports it;
- Launcher V3 memory-requirement word;
- M memory-packer flag;
- optional R OS-RAM flag;
- target-side family adapter supplies the machine-specific launch implementation.

This is a better compatibility baseline than either the 1.1 r10/GOT object ABI or the 2.0 fixed virtual-code ABI.

## Common LLVM code model

New code uses stock LLVM:

```
--target=arm-none-eabi
-march=armv4t
-mabi=apcs-gnu
-mfloat-abi=soft
-fropi
-frwpi
```

ROPI removes the need to know the physical placement of code. RWPI gives writable data a standard ARM static base through r9.

A family runtime may choose an appropriate virtual writable-data window while preserving the source-level API and common runtime contract.

No new application should depend on:

- GCC `-mpic-register=r10`;
- GCC `-msingle-pic-base`;
- runtime GOT patching;
- a historical HPGCC object.

## HPGCC 2.0 features to preserve

HPGCC 2.0 is an important compatibility and optimization target for the 49/50 lineage, not a universal model-support baseline.

Confirmed improvements include:

- removal of the old application r10/single-PIC-base model from the VC source build;
- default Thumb build with ARM/Thumb interworking;
- Launcher V3 format v1.2;
- virtual code addressing;
- virtual code/data page-table construction in the calculator-side launcher;
- E/A flags controlling use of ERAM and main RAM;
- elimination of the old stack bias;
- improved cache flushing.

For HP49/50, hpclang should aim to reproduce the full Launcher V3 v1.2/VC behavior.

For HP39/40 and 48gII, compatible improvements can be ported deliberately without assuming the old 2.0 tree still supported those backends. The common simple/MMU ABI remains the fallback/common contract.

## HPGCC3 features to preserve

HPGCC3 provides useful higher-level evolution:

- CC3/EXT3 program format with minimum ROMLIB revision;
- versioned shared runtime ABI;
- richer startup policy;
- expanded Saturn object API (`sat3`);
- extended-real support;
- expanded filesystem/SD functions;
- larger GUI/runtime library surface;
- conventional `arm-none-eabi` ARM/Thumb organization.

These should influence hpclang's source API and optional installable shared-service ABI.

ROM modification is explicitly outside the hpclang project goal. An HPGCC3-compatible ROMLIB backend may be supported only when such an environment already exists; hpclang itself will not require users to install or modify a ROM.

## Final baseline

The long-term baseline is therefore:

```
LLVM-native common runtime/API
          +
Launcher V3 simple/MMU executable ABI
          +
family-specific platform adapter
          +
optional VC / installed-ROMLIB compatibility profiles
```

No one historical release owns the architecture.
