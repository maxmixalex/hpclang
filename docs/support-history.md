# Historical model-support timeline

This file distinguishes HPGCC release lineage from hpclang's intended target
support.

## HPGCC 1.x era

The 1.1-era source and examples cover multiple ARM calculator families:

- HP39g+;
- HP48gII;
- HP49g+.

The source tree contains both the ordinary ARMToolbox and a dedicated
`masd_source48` implementation for HP48gII.

The later 1.1-p1 39G support package adds the HP39 aplet and ARMToolbox39
distribution path used by the HP39/40 family.

Contemporary HP 39gs/40gs documentation states that HPGCC was originally
developed for the 49g+, and that support for the 39gs and 40gs was added later.

## HPGCC 2.0 era

By the 2.0/SP2 release, the public packaged platform is 49/50.

The official SourceForge `frozen_2.0_VC_release` confirms the architectural
shift:

- VC crt0 is present;
- the source build defaults to Thumb/interworking;
- the dedicated 1.1-era `masd_source48` paths are no longer present;
- HP39-Aplet paths are no longer present.

Contemporary HP 39gs/40gs documentation explicitly notes that support for
39g+/39gs/40gs was discontinued after the 2.0 transition.

This means HPGCC 2.0 is an evolution of the main 49/50 line, not a
backward-compatible source superset of every older calculator backend.

## HPGCC3 era

HPGCC3 targets the HP49g+/HP50g family and introduces its versioned ROMLIB/CC3
environment.

hpclang treats that as an optional compatibility environment and a design
reference. It is not required by the normal non-ROM SDK.

## hpclang policy

hpclang restores the branched family support into one modern project:

```
                    common LLVM ARM runtime
                           |
                  Launcher V3 simple/MMU
                           |
              +------------+------------+
              |            |            |
           HP39/40       HP48gII      HP49/50
              |            |            |
      family launcher   family launcher  family launcher
                                      |
                                      +-- optional HPGCC2 VC
                                      +-- optional existing ROMLIB
```

The historical fact that a later HPGCC release dropped a family is not a reason
for hpclang to drop it.
