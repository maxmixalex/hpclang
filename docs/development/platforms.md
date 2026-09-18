# Platforms

hpclang supports six ARM calculator model profiles through three platform families.

## Status matrix

| Model | Model ID | Platform | Runtime validation | Stock launcher validation |
| --- | ---: | --- | --- | --- |
| HP 39g+ | 1 | `hp39` | package builds; real stock fixture blocked | blocked on real stock fixture |
| HP 39gs | 2 | `hp39` | real hardware PASS; emulator PASS | real hardware return PASS; stock emulator PASS |
| HP 40gs | 3 | `hp39` | official-ROM emulator PASS | stock emulator PASS |
| HP 48gII | 4 | `hp48` | verified full-NOR emulator PASS | ARMToolbox48/PrRUN E2E PASS; payload, control-flow return, and settled Stock UI repaint are all witnessed |
| HP 49g+ | 5 | `hp4950` | native-bootloader emulator PASS | ARMToolbox/PrRUN E2E PASS |
| HP 50g | 6 | `hp4950` | native-bootloader emulator PASS | ARMToolbox/PrRUN E2E PASS |

HP 39gs is the primary real-hardware validation machine. It is not the architectural center of the SDK.

## HP 39g+, HP 39gs, and HP 40gs

The `hp39` platform uses the HP39 aplet/container path and ARMToolbox39/library 275. Its launcher has a distinct Saturn-to-ARM mapping and does not need to expose later HP49/50 launcher features to applications.

The common runtime is validated on HP 39gs hardware and on official HP 39gs/40gs ROMs in the ARM emulator. The same historical HPGCC39 aplet/library boundary also produces a model-1 HP 39g+ hardware-validation package.

HP 39g+ remains blocked because no trustworthy public stock ARM flash fixture has been established. The compiler, runtime, stage0, HP39 package path, and read-only flash-dump mechanism already exist. Resume E2E only after a real HP 39g+ is available and its stock flash has been dumped and verified.

The preferred HP39/40 stock fixture path uses the original ROM's own transfer/install logic. Direct SRAM or flash patching is only a fallback. The obsolete Conn3x GUI is not part of the emulator regression contract.

## HP 48gII

Historical HPGCC 1.x contains a dedicated ARMToolbox48 path. Important differences from HP49/50 include the 131×64 display, 256 KiB physical RAM in the validated second-generation USB model, a distinct firmware identity, and a launcher memory model without the normal HP49/50 Port1/ERAM scan.

The `hp48` platform feeds the same common bootstrap and HPC1 image on a genuine HP48gII full-NOR fixture. Direct runtime passes and returns cleanly.

The full Stock gate transfers ARMToolbox48 and `HPCLANG.HP` through the calculator's original Serial Kermit path, installs library 275 into Port 0, warmstarts, launches `ARMTo` / `PrRUN`, and reaches the same HPC1 PASS result. Runtime #122 first closed the full return path; the final M7.1 head repeats that closure in Runtime #135 (`35298949365`), with payload PASS, control-flow restoration, stage-window teardown, exact keyboard progression, and a settled stock-owned repaint all green. Gallery #112 (`35298949457`) independently repeats the settled HP48 return around the live native-font path.

The architecture criterion is met: HP48gII executes the same HPC1 image and common bootstrap without a distinct application ABI. Payload correctness, control-flow restoration, and visible Stock UI restoration remain separate evidence dimensions, but all three now have passing automated witnesses.

## HP 49g+ and HP 50g

HP 49g+ and HP 50g share the standard ARMToolbox lineage, the `hp4950` platform implementation, the official 2.15 OS image, the stock File Manager/ARMToolbox sequences, and the HPC1 ABI.

Their original-system path is:

```text
model-specific ARM bootloader + shared official 2.15 OS
 -> install ARMToolbox/library 275 from SD
 -> materialize HPCLANG.HP
 -> ARMTo
 -> PrRUN
 -> HPC1 RUNTIME PASS
```

The same stock UI sequence passes on both models. The validated model-specific boot difference is the native ARM bootloader: `boot-49g+.bin` versus `boot-50g.bin`. That difference belongs in model fixture metadata, not in a separate runtime or application ABI.

## Hardware baseline

All native targets use an ARM9/ARM920T-class ARMv4T core. Their RAM, display, flash, bootloader, and calculator-side launch environments differ; those differences belong in platform or model metadata.

Saturn-only HP 39G/40G, classic HP 48, and HP 49G machines are outside the native hpclang runtime scope.
