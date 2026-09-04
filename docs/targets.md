# Target families

hpclang treats the ARM calculators as related hardware with distinct calculator-side launch environments.

## Target matrix

| Target | Family backend | Historical evidence | Baseline executable | Additional profiles | Current validation |
| --- | --- | --- | --- | --- | --- |
| `hp39gplus` | `hp39` | HPGCC 1.1-p1 39G support / ARMToolbox39 | Launcher V3 simple M/R | future backports of selected v1.2 flags | source archaeology |
| `hp39gs` | `hp39` | patched 39G SDK is known to run on 39gs | Launcher V3 simple M/R | same as hp39gplus | **existing hardware path; clean backend in progress** |
| `hp40gs` | `hp39` | same ARM HP39/40 generation; HPGCC-specific validation incomplete | Launcher V3 simple M/R candidate | same as hp39gplus | hardware validation required |
| `hp48gii` | `hp48` | HPGCC source compatibility work + dedicated ARMToolbox48 sources | Launcher V3 simple M/R | investigate v1.2 feature backports | source archaeology |
| `hp49gplus` | `hp49` | core HPGCC target; HPGCC3 project includes 49g+ family | Launcher V3 simple M/R | HPGCC 2.0 VC; optional ROMLIB compatibility | source archaeology |
| `hp50g` | `hp49` | HPGCC 2.0 SP2 49/50 platform; HPGCC3 | Launcher V3 simple M/R | HPGCC 2.0 VC; optional ROMLIB compatibility | source archaeology |

The matrix distinguishes project targets from claims of historical validation. In particular, 40gs requires an explicit hardware/release audit before being labelled byte-compatible with the 39gs package.

## Why these are separate backends

### HP39/40

The 39G support package introduces a different distribution model:

- HP39 aplet;
- `HP39BinB`;
- HP39 directory files;
- ARMToolbox39/library 275;
- primary-entry-only launcher modification.

Its Saturn-to-ARM bridge and packaging are not interchangeable with 49/50 RPL libraries.

### HP48gII

The ordinary ARMToolbox source contains a dedicated `masd_source48`.

Notable differences visible in the launcher sources include:

- different Saturn-to-ARM address conversion;
- no 49/50 Port1/ERAM scan in the same form;
- different RAM limits;
- a dedicated system-RAM extension helper.

Therefore 48gII should not be hidden behind a generic `hp49` define.

### HP49g+ / HP50g

The standard ARMToolbox has the richest Launcher V3 implementation.

The historical memory packer can collect memory from:

- main RAM;
- Port1/ERAM;
- OS/KOS RAM.

HPGCC 2.0 ARMToolbox 3.1 additionally supplies virtual-code addressing. HPGCC3 later moves a large shared runtime into ROMLIB.

49g+ and 50g remain distinct model IDs even though the HPGCC3 Saturn documentation says their physical 512 KiB RAM layout is identical. Device features and validation can still differ.

## Common capabilities

Targets should expose capabilities instead of forcing every backend to mimic the newest launcher.

Planned capability classes:

- `L3_SIMPLE` — Launcher V3 simple program format;
- `MMU_PACKER` — M flag and page-table delivery;
- `OS_RAM` — R flag / extra OS RAM;
- `MULTI_ENTRY` — Launcher V3 secondary function-entry table;
- `ERAM` — Port1/ERAM use;
- `L3_VIRTUAL` — HPGCC 2.0 V-addressing format;
- `ARM_THUMB_INTERWORK`;
- `ROMLIB` — HPGCC3-compatible shared runtime environment.

A source library can then state requirements such as:

```
requires: L3_SIMPLE + MMU_PACKER
optional: ERAM
```

rather than naming a calculator in generic code.

## Out of scope

The native hpclang architecture requires an ARM processor. Saturn-only machines such as the original 39G/40G, classic 48 family and 49G are not HPGCC-native targets.

They may share System RPL tooling and file formats with hpclang's packaging tools, but they are not targets for the ARM runtime.
