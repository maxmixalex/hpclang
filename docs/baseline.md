# hpclang baseline

## Decision

hpclang has one native application ABI.

Historical HPGCC 1.x, 2.x and 3.x are research sources, not selectable runtime
architectures.

The production baseline is:

```
Clang + LLVM + LLD
ARMv4T
AAPCS32
Thumb-1 C/C++
soft float
fixed virtual application layout
HPC1 logical executable
model-family platform backend
```

## Why fixed virtual addresses

The HPGCC 2.0 work demonstrated the useful architectural idea: if the
calculator launcher owns the ARM MMU, application code does not need to carry
a PIC/GOT convention at all.

hpclang generalizes that idea across every supported ARM family.

The older Launcher-V3 M/R path was useful to prove a clean 39gs bootstrap, but
keeping ROPI/RWPI or an APCS-era static-base ABI solely because an old launcher
expects it would make the new SDK unnecessarily complicated.

## Family boundary

The three platform families still differ in real hardware/OS details:

- HP39/40;
- HP48gII;
- HP49g+/50g.

They differ in Saturn address translation, physical RAM discovery, ERAM/OS-RAM
availability and native calculator packaging.

Those differences belong below the common ABI.

A normal application should not know whether its pages came from main RAM,
Port1, KOS memory or another platform-specific source.

## Historical releases

HPGCC 1.x is valuable because it contains the earlier HP39/40 and 48gII launch
paths.

HPGCC 2.0 is valuable because it removed the old r10 single-PIC-base design,
used ARM/Thumb interworking and introduced fixed virtual execution.

HPGCC3 is useful for studying later APIs and runtime organization.

None of these releases defines hpclang's binary ABI.

## Non-ROM policy

The normal hpclang SDK does not require a replacement ROM or ROM patch.

Platform support must be installable through the calculator's ordinary
software mechanisms.
