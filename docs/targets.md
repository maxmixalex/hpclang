# Target families

hpclang treats the ARM calculators as related hardware with distinct calculator-side launch environments.

## Target matrix

| Target | Family backend | Historical evidence | Baseline executable | Additional profiles | Current validation |
| --- | --- | --- | --- | --- | --- |
| `hp39gplus` | `hp39` | dedicated HPGCC 1.1-p1 39G support / ARMToolbox39 | Launcher V3 simple M/R | selected Launcher v1.2 ideas may be backported | source archaeology |
| `hp39gs` | `hp39` | HPGCC support documented for 39gs; patched 39G SDK is hardware-proven in 39gsX | Launcher V3 simple M/R | same as hp39gplus | **existing hardware path; clean backend in progress** |
| `hp40gs` | `hp39` | contemporary HP 39gs/40gs documentation explicitly records HPGCC support | Launcher V3 simple M/R candidate | same family backports | historical support confirmed; package/hardware equivalence still to validate |
| `hp48gii` | `hp48` | HPGCC 1.1 compatibility work + dedicated ARMToolbox48 sources | Launcher V3 simple M/R | future family-specific extensions | source archaeology |
| `hp49gplus` | `hp49` | original/core HPGCC target; HPGCC 2.0 VC and HPGCC3 lineage | Launcher V3 simple M/R | HPGCC 2.0 VC; optional ROMLIB environment | source archaeology |
| `hp50g` | `hp49` | HPGCC 2.0 SP2 49/50 platform; HPGCC3 | Launcher V3 simple M/R | HPGCC 2.0 VC; optional ROMLIB environment | source archaeology |

The matrix distinguishes project targets from claims of byte-for-byte historical compatibility. The 39gs is the first clean hardware-validation machine, not the architectural center of the SDK.

## Historical branch split matters

The official SourceForge `frozen_2.0_VC_release` is not a superset of every earlier calculator backend.

Direct path probes against that official tag show:

- `sources/crt0/Makefile` exists and builds the VC runtime;
- `sources/HP39-Aplet/...` is absent at the historical 1.1 path;
- `sources/ARMToolbox/masd_source48/...` is absent at the historical 1.1 path;
- `sources/ARMToolbox/L275_48.LIB` is absent at the historical 1.1 path.

In contrast, the 1.1-era source tree contains both the dedicated 48gII ARMToolbox and the later 39G support branch.

This is why hpclang must merge family knowledge from several historical branches rather than “upgrade everything to HPGCC 2.0”.

## Why these are separate backends

### HP39/40

The 39G support package introduces a different distribution model:

- HP39 aplet;
- `HP39BinB`;
- HP39 directory files;
- ARMToolbox39/library 275;
- a launcher modified to support only the primary Launcher V3 entry point.

Its Saturn-to-ARM bridge and packaging are not interchangeable with 49/50 RPL libraries.

The historical 39G launcher supports the common simple Launcher V3 format plus M/R memory packing, but not the later 2.0 virtual-code header/flag.

### HP48gII

The 1.1-era ARMToolbox contains a dedicated `masd_source48`.

Confirmed behavior includes:

- Launcher V3 simple marker `0B34C`;
- primary and secondary/function entry-table parsing in `PRLNK`;
- M memory-packer support;
- R/system-RAM support;
- Saturn-to-ARM conversion based on `0x07fc8000 + saturn/2`;
- different main-RAM limits from 49/50;
- a dedicated ARM-side system-RAM extender;
- no standard 49/50 Port1/ERAM scan in the 48 launcher.

Therefore 48gII should not be hidden behind a generic `hp49` define.

The official frozen 2.0 VC tag no longer contains the dedicated 48 launcher at its 1.1 path, so virtual-code support on 48gII is a future hpclang backport question, not a historical 2.0 compatibility claim.

### HP49g+ / HP50g

The standard ARMToolbox has the richest Launcher V3 implementation.

Its historical memory packer can collect memory from:

- main RAM;
- Port1/ERAM;
- OS/KOS RAM.

HPGCC 2.0 ARMToolbox 3.1 additionally supplies:

- Launcher V3 v1.2;
- virtual-code addressing;
- E/A controls for ERAM/main-RAM use;
- removal of the old stack bias;
- improved cache flushing.

HPGCC3 later adds a versioned ROMLIB environment and a much larger shared runtime.

49g+ and 50g remain distinct model IDs even though HPGCC3 documents the same 512 KiB physical RAM organization for both.

## Baseline capability matrix

This table records currently demonstrated historical/common-launcher capabilities. It is deliberately conservative.

| Capability | HP39/40 backend | 48gII backend | 49g+/50g backend |
| --- | --- | --- | --- |
| Launcher V3 simple (`0B34C`) | yes | yes | yes |
| M: MMU memory packer | yes | yes | yes |
| R: extra OS RAM | yes | yes | yes |
| Secondary/multi-entry table | **no in ARMToolbox39** | yes | yes |
| Port1/ERAM memory-packer source | no evidence in 39 launcher | no in dedicated 48 launcher | yes |
| Launcher V3 v1.2 virtual code | no | not historically demonstrated | yes |
| ARM/Thumb interworking for new hpclang code | planned/common ARMv4T capability | planned/common ARMv4T capability | historically used by HPGCC 2.0 and planned |
| HPGCC3 ROMLIB | no | no | optional installed environment only |

ROMLIB is intentionally not a static property of the physical calculator model. It is an optional software environment capability.

## Capability-oriented API

Common source should test requirements instead of guessing them from a model number.

Planned capability classes:

- `L3_SIMPLE`;
- `MMU_PACKER`;
- `OS_RAM`;
- `MULTI_ENTRY`;
- `ERAM`;
- `L3_VIRTUAL`;
- `ARM_THUMB_INTERWORK`;
- `ROMLIB`.

For example:

```
requires: L3_SIMPLE + MMU_PACKER
optional: ERAM
```

The runtime descriptor ABI remains intentionally unfrozen until the HP39, HP48 and HP49 launcher entry states have all been normalized.

## Out of scope

The native hpclang architecture requires an ARM processor. Saturn-only machines such as the original 39G/40G, classic 48 family and 49G are not native ARM runtime targets.

They may share System RPL tooling and file formats with hpclang's host-side packaging tools.
