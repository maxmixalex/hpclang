# HP48gII true-ARM E2E bring-up

This note records temporary bring-up evidence. Supported behavior belongs in
`docs/` only after the gate is running.

## Hardware target

Use the second-generation USB HP48gII as the representative target.

The newRPL firmware target is a useful independent hardware cross-check. Its
`target_48gii.h` inherits the HP50g S3C2410 target and changes the points that
matter to the emulator:

- physical RAM ends at `0x08040000`: 256 KiB;
- LCD height is 64 rather than 80;
- firmware preamble is `KINPOHP48GIIMAGE`;
- the target has a distinct set of LCD initialization values.

The hpclang x50ng adapter therefore keeps the common S3C2410 peripherals but
reduces the SRAM mapping/backing size and accepts the 48gII update-image header.

## Launcher evidence

Historical HPGCC 1.1 contains a dedicated `ARMToolbox/masd_source48` path.
Its Saturn-to-ARM conversion and memory-pool setup differ from the normal
49g+/50g path, but it exposes the same ARMSYS fields used by the current hpclang
stage0/stage1 boundary.

This supports a thin HP48 platform adapter rather than a second application ABI.

## Firmware fixture

The provenance chain has three distinct layers:

1. ZephRay's 2015 cnCalc hardware-modification write-up demonstrates the
   original method: replace the HP39gs NOR with SST39VF160, JTAG a ROM
   extracted from a real HP48gII at flash address zero, then boot as 48gII:
   https://www.cncalc.org/thread-11475-1-1.html
2. wjchen's 2020 cnCalc post publishes a specifically identified factory
   2.08 build-92 HP48gII JTAG dump and records its full-NOR properties.
3. lyc8503's 2021 write-up provides the currently live downloadable full-NOR
   fixture used by CI. It is an independent dump lineage, not a byte-for-byte
   mirror of wjchen's 2020 `CNA0000000` image.
The primary historical source is wjchen's 2020 cnCalc dump:
https://www.cncalc.org/thread-24739-1-1.html

It documents `48gII_208_92_NOR_CNA0000000.raw` as a complete 2 MiB JTAG
dump of a factory 2.08 build-92 HP48gII, including the bootloader, with the
serial cleared to `CNA0000000`. The original cnCalc attachment has expired.

A still-live mirror is published with lyc8503's 2021 hardware-modification
write-up:
https://blog.lyc8503.net/en/post/hp39gs-modify/

The mirrored `48gii.bin` was independently inspected in CI:

- exact size: `0x200000` bytes;
- SHA-256: `4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937`;
- ARM vector table at offset zero;
- `KINPOHP48GIIMAGE` in the boot area;
- `Kinposhcopyright` at the firmware boundary `0x4000`;
- bootloader serial `CNA6070206`, proving it is not wjchen's serial-cleared
  `CNA0000000` dump.

For an independent vendor-origin cross-check, hpcalc.org archives
Hewlett-Packard's 2.15 emulator ROM as a 2 MiB `rom.49g` and explicitly
describes it as a ROM image from the HP48gII/49g+/50g family:
https://www.hpcalc.org/details/6744

hpcalc.org also preserves the Hewlett-Packard 49g+/50g ARM bootloaders, but
does not currently provide a standalone 48gII ARM bootloader archive. Thus the
full-NOR mirror remains the current source for the 48gII boot area; CI pins its
SHA-256 rather than trusting mutable web content.

`tools/prepare-hp48gii-nor.py` installs the verified raw image directly as
x50ng's persistent `flash` state. Pinned x50ng defaults to
`HDW_REINIT_NONE`; passing `--flash` or `--flash-full` would overwrite the
already prepared HP48gII NOR.

## Direct runtime result

GitHub Actions run `34132800652` booted the verified full NOR on the hp48gii
x50ng personality and completed the canonical runtime contract:

- machine-readable result `0x50415353`;
- `saw_pass=true`;
- `returned=true`;
- 131x64 PASS and post-return LCD captures.

This closes the raw-NOR and Direct-runtime bring-up blockers. Remaining HP48gII
work is the supported calculator-side ARMToolbox48 launch path.
