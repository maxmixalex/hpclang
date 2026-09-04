# HPGCC lineage and calculator-specific split

This file records the historical baselines hpclang is comparing. Version numbers are not a simple compatibility ladder.

## HPGCC 1.1

The original 1.1-era toolchain used an `arm-elf-gcc` ARM920T/ARMv4T toolchain plus HPGCC runtime libraries, an MMU loader, a custom linker script and `elf2hp`.

Typical historical compiler options included:

```
-mtune=arm920t
-mcpu=arm920t
-mapcs-32
-fPIC
-mpic-register=r10
-msingle-pic-base
-mlittle-endian
-fomit-frame-pointer
-msoft-float
```

The MMU loader itself was special: it was built non-PIC with `-ffixed-r10 -fno-inline`.

### 1.1-p1 with 39G support

The dedicated 39G maintenance package was released after ordinary HPGCC 2.0 had already appeared. It adds calculator-specific material that is not merely compiler configuration:

- HP39 Aplet Builder (`hp2aplet`);
- ARMToolbox39 / library 275 support;
- HP39 directory and aplet packaging;
- aplet template payloads such as `apletcode.bin`, `apletepilog.bin`, `blank.gro`, and `LIB275L.L39`;
- the HPGCC MMU startup/linking path used by the ARM payload.

This branch is the historical basis for hpclang's 39g+/39gs/40gs profile.

## HPGCC 2.0 and 2.0 SP2

The archived 2.0/SP2 distributions are targeted at the 49/50 family. They retain the HPGCC native-ARM model and expanded libraries, but the dedicated HP39 aplet-builder payload is not part of the 2.0 SP2 distribution.

Therefore hpclang must not infer that “2.0 > 1.1” means the 39gs profile should be rebased onto 2.0. The 39-series branch and the later 49/50 branch need separate profiles.

Exact source-level deltas between 1.1, 2.0, p1 and SP2 are still being reconstructed from the archived distributions.

## HPGCC3 / 3.0

HPGCC3 is an architectural change, not a drop-in HPGCC2 upgrade.

The R004.revA source contains:

- `RLloader.c`;
- `RLld.script`;
- ROM globals;
- launchers and the HPGCC 3.0 EXT3 executable format;
- a versioned ROMLIB.

The loader calls ROM-resident `__launch_loader` and `__launch_startup`. The executable format carries a `CC3` marker and a required ROMLIB revision. Significant runtime functionality moved into the custom ROM.

Consequently, the HPGCC3 profile should model a ROMLIB ABI and HPGCC3 executable format; it should not replace the HPGCC1/2 MMU runtime profile.

## Working interpretation

hpclang currently treats the historical family as three related but distinct compatibility contracts:

```
HPGCC 1.x/2.x native ARM
        |
        +-- 39-series branch: 1.1-p1 + HP39 aplet/ARMToolbox39 layer
        |
        +-- 49/50 branch: 2.0 -> SP2

HPGCC3 / 3.0
        |
        +-- 50g custom-ROM + ROMLIB + CC3 executable format
```

This model may be refined as the official archives are diffed, but calculator-specific behavior must remain explicit.
