# Historical background

Historical HPGCC releases are research inputs, not selectable hpclang architectures.

## HPGCC 1.x

HPGCC 1.x used GCC-era APCS/PIC conventions, an MMU loader, custom linker scripts, and `elf2hp`.

The 1.1-p1 material is especially important because it contains calculator-family support that later releases do not preserve:

- the HP39 aplet/container path;
- `HP39-Aplet`;
- ARMToolbox39/library 275;
- the dedicated HP48gII `masd_source48` path;
- Launcher V3/MMU source used to establish the Saturn/ARM state layout and memory behavior.

The historical HP39 package is explicitly rooted in HP 39g+ support. The same lineage is usable on HP 39gs/40gs, which supports treating those calculators as one hp39 platform implementation while preserving separate model/firmware profiles.

## HPGCC 2.0

The 2.0 line focuses on HP 49g+/50g and evolves the launcher/runtime toward fixed virtual execution and ARM/Thumb interworking.

A direct comparison between the official SourceForge tags `frozen_1.1_MMU_trunk` and `frozen_2.0_VC_release` found:

- 761 files in the 1.1 tag;
- 736 files in the 2.0 VC tag;
- 279 identical common paths;
- 399 changed common paths;
- 83 paths only in 1.1;
- 58 paths only in 2.0.

Crucially, 2.0 removes the active HP39 aplet tree and the dedicated HP48 launcher source from the main supported path. Therefore HPGCC 2.0 must not be treated as a strict superset of 1.1 when reconstructing calculator-family support.

The architectural lesson retained by hpclang is that the calculator-side launcher can own the MMU and expose a simple fixed virtual application environment.

## HPGCC3

HPGCC3 introduces a custom-ROM/ROMLIB environment and a versioned executable format. It is useful as an API/runtime design reference but is not a required hpclang platform dependency.

hpclang does not require a custom calculator ROM and does not import the HPGCC3 runtime as a compatibility layer.

## Source provenance

Primary historical inputs include:

- SourceForge HP-GCC release/tag archives;
- the dedicated HPGCC 1.1-p1 39G package;
- HPGCC 2.0/SP2 source archives;
- the public HPGCC3 R004.revA distribution;
- reconstructed source mirrors used only for cross-checking.

Mirrors are not silently treated as authoritative release snapshots. When a historical binary or source fact matters, record the original release/tag when possible.

## What hpclang keeps

hpclang retains externally observable compatibility facts:

- calculator package/container formats;
- launcher entry contracts;
- memory-map and register facts;
- model-specific System RPL/ROM integration;
- stock transfer mechanisms;
- behavior demonstrated by emulator or hardware tests.

Old compiler backends, obsolete application PIC/static-base conventions, and abandoned hpclang experiments are not supported modes. Their conclusions remain documented where they explain a current architectural decision.

See [Retired experiments](experiments.md) for the experiments removed from the working source tree after their conclusions were incorporated.
