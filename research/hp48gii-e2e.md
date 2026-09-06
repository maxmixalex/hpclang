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

A public cnCalc post documents
`48gII_208_92_NOR_CNA0000000.raw`: a 2 MiB JTAG dump of factory 2.08 build 92
with the bootloader included. That is the preferred fixture for a true-ARM
x50ng boot.

The old forum attachment currently resolves to an expired attachment URL, so CI
must not depend on it until a stable mirror is available. A Zhihu mirror also
documents the same raw file, but its linked Baidu object has not yet been
recovered into a stable direct URL.

`tools/prepare-hp48gii-nor.py` validates a 2 MiB full dump and installs it as
x50ng's persistent `flash` state. Once a stable fixture is available, add
`hp48gii` to the Direct runtime matrix using the `hp48gii` x50ng preparation
mode and model ID 4.
