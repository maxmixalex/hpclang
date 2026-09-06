# Historical background

Historical HPGCC releases are research inputs, not selectable hpclang
architectures.

## HPGCC 1.x

HPGCC 1.x used GCC-era APCS/PIC conventions, an MMU loader, custom linker
scripts and `elf2hp`.

The later 1.1-p1 package added the dedicated HP39/40 distribution path:
HP39 aplets, directory metadata and ARMToolbox39/library 275. The same era also
contains a dedicated HP48gII ARMToolbox implementation.

These sources are important because later HPGCC releases are not supersets of
all older calculator-family support.

## HPGCC 2.0

The 2.0 line focuses on 49g+/50g and evolves the launcher/runtime toward fixed
virtual execution and ARM/Thumb interworking. It does not carry the dedicated
HP39 and HP48 paths as a backwards-compatible family bundle.

The useful architectural lesson retained by hpclang is that the launcher can
own the MMU and expose a simple fixed virtual application environment.

## HPGCC3

HPGCC3 introduces a custom-ROM/ROMLIB environment and a versioned executable
format. It is a useful API/runtime design reference but is not a required
hpclang platform dependency.

## Source provenance

Primary historical inputs include:

- SourceForge HP-GCC release/tag archives;
- the dedicated HPGCC 1.1-p1 39G package;
- HPGCC 2.0/SP2 source archives;
- the public HPGCC3 R004.revA distribution;
- reconstructed source mirrors used only for cross-checking.

Mirrors are not silently treated as authoritative release snapshots. When a
historical binary or source fact matters, record the original release/tag when
possible.

## What hpclang keeps

hpclang retains externally observable compatibility facts:

- calculator package/container formats;
- launcher entry contracts;
- memory-map and register facts;
- model-specific System RPL/ROM integration;
- behavior demonstrated by emulator or hardware tests.

Old compiler backends, PIC/static-base conventions and obsolete hpclang
experiments remain research material rather than supported modes.
