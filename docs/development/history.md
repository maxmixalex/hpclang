# History and source provenance

hpclang is a new LLVM-native implementation. Historical HPGCC releases are used to understand calculator formats, launch behavior, and hardware integration; they are not selectable hpclang runtimes.

## HPGCC 1.x

HPGCC 1.x used GCC-era conventions, an MMU loader, custom linker scripts, and `elf2hp`. The 1.1-p1 distribution contains the dedicated HP39 aplet/ARMToolbox39 path originally associated with HP 39g+ and later usable on HP 39gs/40gs. The same era also contains a dedicated HP48gII ARMToolbox implementation.

These sources are important because later HPGCC releases do not preserve every earlier calculator family.

## HPGCC 2.x and HPGCC3

HPGCC 2.x concentrates on HP 49g+/50g and develops the fixed-virtual execution approach further. HPGCC3 introduces a custom-ROM/ROMLIB environment and a versioned executable format.

hpclang keeps the useful architectural lessons without adopting those historical application ABIs or requiring a custom ROM.

## What remains relevant

Historical material is retained only when it establishes facts still needed by the current implementation, such as calculator package formats, launcher entry behavior, memory/register layout, model-specific stock-system integration, or behavior confirmed by hardware/emulator tests.

Large generated inventories, tag comparisons, abandoned RWPI experiments, and the old Launcher V3 bootstrap have been removed from the working tree after their useful conclusions were incorporated into the current architecture and documentation. Git history remains the record for those experiments.

The HP 39g+ read-only flash dumper remains under `research/hp39gplus/` because it is still required to obtain the fixture that blocks HP 39g+ E2E.

## Licensing

Historical HPGCC 1.x/2.x source is GPL-licensed. hpclang therefore prefers independent implementation based on documented formats, observable behavior, and tests rather than copying historical implementation code.

HPGCC3 has separate redistribution restrictions and must not be vendored as an hpclang runtime/library implementation.

HP tools, System RPL assets, stock firmware, and calculator binary images have their own provenance and redistribution constraints. The normal source tree should contain only material suitable for redistribution; external firmware fixtures are fetched or otherwise handled according to their provenance.
