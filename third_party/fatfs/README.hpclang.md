# Vendored FatFs core

This directory contains the filesystem-layer portion of ChaN's FatFs for
hpclang M7.2c.

Baseline: FatFs R0.16 (2025-07-22) plus official patch-1 (2025-09-13) and
official patch-2 (2026-07-10), as documented at
https://elm-chan.org/fsw/ff/ and https://elm-chan.org/fsw/ff/patches.html.

The initial import used the public `abbrev/fatfs` mirror as a transport source,
but that transport is not accepted as provenance. `tools/verify-fatfs-vendor.sh`
downloads `ff16.zip`, `ff16p1.diff`, and `ff16p2.diff` directly from
`elm-chan.org`, applies patch-1 then patch-2 to the official `source/ff.c`,
and requires byte-for-byte equality with every vendored FatFs core file.
The `CI / FatFs vendor` gate therefore makes the official upstream archive and
patches authoritative; the mirror commit is not a version oracle. `ffconf.h`
is hpclang-owned configuration.

Initial profile: one FAT12/16/32 volume, fixed 512-byte sectors, UTF-8 LFN,
`FF_FS_RPATH=0`, no exFAT, no mkfs, and one static LFN workspace.

FatFs remains separate from media access. The S3C2410 SDIO/block layer will be
an independent hpclang implementation; HPGCC3 FSystem/SD code is behavior
evidence only and is not implementation source.

At the import commit FatFs is host-unit compiled only. It is not yet part of
`HPCLANG_LIBC` and does not alter calculator storage behavior.
