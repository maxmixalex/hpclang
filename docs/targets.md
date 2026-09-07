# Targets

hpclang supports six ARM calculator targets through three family backends.

## Status matrix

| Target | Model ID | Backend | Runtime validation | Stock launcher validation |
| --- | ---: | --- | --- | --- |
| HP 39g+ | 1 | HP39/40 | pending sibling gate | pending |
| HP 39gs | 2 | HP39/40 | real hardware PASS; emulator PASS | real hardware PASS; full stock emulator PASS |
| HP 40gs | 3 | HP39/40 | official-ROM emulator PASS | full stock emulator PASS |
| HP 48gII | 4 | HP48 | full-NOR medium-apple emulator PASS | full ARMToolbox48/PrRUN E2E PASS |
| HP 49g+ | 5 | HP49/50 | official-ROM emulator PASS | pending stock launcher gate |
| HP 50g | 6 | HP49/50 | official-ROM emulator PASS | full ARMToolbox/PrRUN E2E PASS |

The 39gs is the primary real-hardware validation machine. It is not the
architectural center of the SDK.

## HP39/40 family

The family uses the HP39 aplet/container path and ARMToolbox39/library 275.
Its launcher has a distinct Saturn-to-ARM mapping and no need to expose later
49/50 launcher features to applications.

The common runtime is already validated on HP39gs hardware and on official
HP39gs/HP40gs ROMs in the ARM emulator backend.

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
backend. The HP50g original-system path is now fully automated:

```text
official 2.15 ROM
 -> install ARMToolbox/library 275
 -> materialize HPCLANG.HP
 -> ARMTo
 -> PrRUN
 -> HPC1 RUNTIME PASS
```

HP49g+ now passes the same Direct runtime gate on the shared hp4950 hardware
backend. Its remaining sibling work is the stock ARMToolbox/package path; no
model-specific runtime fork is required.

## Hardware baseline

All native targets use an ARM9/ARM920T-class ARMv4T core. Their memory,
display, flash and calculator-side launch environments differ; those
differences belong in the platform backend.

Saturn-only 39G/40G, classic 48 and 49G machines are outside the native hpclang
runtime scope.
