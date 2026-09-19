# Vendored FatFs core

This directory contains the filesystem-layer portion of ChaN's FatFs for
hpclang M7.2c.

Baseline: FatFs R0.16 (2025-07-22) plus official patch-1 (2025-09-13) and
official patch-2 (2026-07-10), as documented at
https://elm-chan.org/fsw/ff/ and https://elm-chan.org/fsw/ff/patches.html.

The development executor cannot directly retrieve the upstream ZIP, so these
source bytes were transported from the public `abbrev/fatfs` mirror at commit
`ab992fcf2926b8a1ee3f0a8f9cefb6584174100d`. Import-time checks require the patch-2 banner and all
security-relevant patch-1/patch-2 hunks. `ff.c`, `ff.h`, `diskio.h`, and
`ffunicode.c` remain vendor code; `ffconf.h` is hpclang-owned configuration.

Initial profile: one FAT12/16/32 volume, fixed 512-byte sectors, UTF-8 LFN,
`FF_FS_RPATH=0`, no exFAT, no mkfs, and one static LFN workspace.

FatFs remains separate from media access. The S3C2410 SDIO/block layer will be
an independent hpclang implementation; HPGCC3 FSystem/SD code is behavior
evidence only and is not implementation source.

At the import commit FatFs is host-unit compiled only. It is not yet part of
`HPCLANG_LIBC` and does not alter calculator storage behavior.
