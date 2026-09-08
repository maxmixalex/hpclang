# Targets

hpclang supports six ARM calculator targets through three family backends.

## Status matrix

| Target | Model ID | Backend | Runtime validation | Stock launcher validation |
| --- | ---: | --- | --- | --- |
| HP 39g+ | 1 | HP39/40 | model-1 hardware package builds; firmware gate pending | pending stock fixture |
| HP 39gs | 2 | HP39/40 | real hardware PASS; emulator PASS | real hardware PASS; full stock emulator PASS |
| HP 40gs | 3 | HP39/40 | official-ROM emulator PASS | full stock emulator PASS |
| HP 48gII | 4 | HP48 | full-NOR medium-apple emulator PASS | full ARMToolbox48/PrRUN E2E PASS |
| HP 49g+ | 5 | HP49/50 | native-bootloader emulator PASS | full ARMToolbox/PrRUN E2E PASS |
| HP 50g | 6 | HP49/50 | native-bootloader emulator PASS | full ARMToolbox/PrRUN E2E PASS |

The 39gs is the primary real-hardware validation machine. It is not the
architectural center of the SDK.

## HP39/40 family

The family uses the HP39 aplet/container path and ARMToolbox39/library 275.
Its launcher has a distinct Saturn-to-ARM mapping and no need to expose later
49/50 launcher features to applications.

The common runtime is already validated on HP39gs hardware and on official
HP39gs/HP40gs ROMs in the ARM emulator backend. The same historical HPGCC39
aplet/library boundary now also produces a model-1 HP39g+ hardware-validation
package. Emulator/stock validation remains pending until a trustworthy HP39g+
ARM firmware fixture is available.

## HP48gII

The historical HPGCC 1.x tree contains a dedicated HP48gII ARMToolbox path.
Important differences from 49/50 include its 131x64 display, smaller RAM
configurations and a dedicated launcher memory model without the normal 49/50
Port1/ERAM scan.

The HP48 family adapter now feeds the same common bootstrap and HPC1 image on
a genuine 48gII full-NOR fixture with 256 KiB SRAM and a 131x64 display. The
Direct gate passes and returns cleanly. The full stock gate also transfers
ARMToolbox48 and HPCLANG through the calculator's original serial Kermit path,
installs library 275 into Port 0, launches PrRUN and reaches the same HPC1 PASS
result.

## HP49g+/50g family

49g+ and 50g share the standard ARMToolbox lineage and the same hpclang family
backend. Both original-system paths are automated:

```text
model-specific ARM bootloader + shared official 2.15 OS
 -> install ARMToolbox/library 275 from SD
 -> materialize HPCLANG.HP
 -> ARMTo
 -> PrRUN
 -> HPC1 RUNTIME PASS
```

The same stock UI sequences pass unchanged on both models. The only validated
model-specific boot difference is the native ARM bootloader
(`boot-49g+.bin` versus `boot-50g.bin`); it is carried by the target profile,
not by a separate runtime or application ABI.

## Hardware baseline

All native targets use an ARM9/ARM920T-class ARMv4T core. Their memory,
display, flash and calculator-side launch environments differ; those
differences belong in the platform backend.

Saturn-only 39G/40G, classic 48 and 49G machines are outside the native hpclang
runtime scope.
