# Launcher compatibility matrix

This file records calculator-side launcher behavior independently of the C
runtime. It is the basis for deciding what may safely move into
`runtime/common`.

## Common Launcher V3 simple structure

All three historical launcher families under study recognize the simple
Launcher V3 structure beginning with marker `0B34C`.

The shared payload concepts are:

- aligned ARM code;
- primary entry point;
- optional entry table where supported;
- memory-requirement word;
- M flag for an MMU page-table memory packer;
- R flag for additional OS/KOS RAM.

The family-specific implementation behind those concepts differs.

## Feature matrix

| Behavior | ARMToolbox39 | ARMToolbox48 | standard ARMToolbox (49/50) | ARMToolbox 3.1 / HPGCC 2.0 |
| --- | --- | --- | --- | --- |
| Simple marker `0B34C` | yes | yes | yes | yes |
| M memory packer | yes | yes | yes | yes |
| R OS RAM | yes | yes | yes | yes |
| Primary entry | yes | yes | yes | yes |
| Secondary entry table | intentionally removed | yes, parsed by `PRLNK` | yes | yes |
| Port1/ERAM scan | no matching path found | absent from dedicated `ARMJMP` | yes | yes, plus E control |
| Main RAM | yes | yes | yes | yes, plus A control |
| Virtual-code marker `0A94C` | no | not demonstrated | later extension | yes |
| V virtual-code flag | no | not demonstrated | later extension | yes |
| Stack-bias B flag | no v1.2 support | no v1.2 evidence | later extension | yes |
| Fixed virtual code `0x01220000` | no | no | no in simple mode | yes in VC mode |

## Saturn-to-ARM conversion

### HP39/40 launcher

ARMToolbox39 uses a conversion equivalent to:

```
arm = 0x07fc8000 + saturn / 2
```

for the directly visible Saturn region used by the launcher.

### HP48gII launcher

The dedicated 48 `PRLNK` and `ARMJMP` use the same visible-region base
`0x07fc8000`, but their memory limits and system-RAM handling differ from
HP39 and from 49/50.

### HP49g+/50g launcher

The standard launcher has bank-aware conversion. For ordinary RAM it uses the
`0x08000000 + saturn/2` mapping and has additional logic for flash banks and
Port1/ERAM.

Therefore address conversion is a family-shim responsibility.

## MMU packer differences

### ARMToolbox39

Builds a main-memory page list and can append OS RAM through the R path. It is
adapted to the HP39 calculator environment and removes secondary function
dispatch.

### ARMToolbox48

Builds a main-memory page list with 48-specific address/limit constants and can
append OS RAM. A dedicated ARM helper extends the page list with KOS allocation.
The launcher does not contain the ordinary 49/50 Port1/ERAM scan.

### ARMToolbox 49/50

Builds the richest page list:

- main RAM;
- free Port1/ERAM;
- optional KOS/OS RAM.

This model later grows E/A controls and virtual-code support in Launcher 3.1.

## Implication for hpclang

The following belongs in a common executable/runtime contract:

- Launcher V3 simple payload parsing semantics;
- normalized launch-context fields;
- ROPI/RWPI application ABI;
- common C startup after family state normalization.

The following must remain family-specific:

- Saturn-to-ARM mapping;
- RAM-region discovery;
- ERAM availability;
- KOS allocation details;
- package/container installation;
- exact System-RPL/ROM entry addresses.

No code should move from a family backend into `runtime/common` solely because
all machines use an ARM920T-class ARMv4T core.
