# Historical source inventory

This inventory separates authoritative archives from mirrors used for source archaeology.

## HPGCC 1.1 / 39G

Authoritative archive:

- SourceForge HP-GCC file tree:
  https://sourceforge.net/projects/hpgcc/files/hpgcc/
- Dedicated 39G package:
  https://sourceforge.net/projects/hpgcc/files/hpgcc/HPGCC-1.1%20with%2039G%20support%20for%20Windows/

Observed release:
- `HPGCC-1.1-p1_with_39G_support_WinSetup.exe`
- published 2006-03-28

Source mirror used for code-level inspection:
- https://github.com/lobakkang/patched-hpgcc-1.1-sdk-for-hp39gs

Important: the mirror is evidence and a convenient reconstruction source; the SourceForge package remains the provenance baseline.

## HPGCC 2.0 / SP2

Authoritative/archive metadata:

- SourceForge release tree:
  https://sourceforge.net/projects/hpgcc/files/hpgcc/
- hpcalc Linux SP2 archive:
  https://www.hpcalc.org/details/6384
- hpcalc Windows SP2 archive:
  https://www.hpcalc.org/details/6298

Observed SP2 Linux archive:
- `hpgcc-2.0-sp2-linux.tar.bz2`
- source included
- platform catalogued as 49/50
- contains `MMUloader.o`, `MMUglobals.o`, `MMUld.script`,
  `VCloader.o`, `VCld.script`, `crt0.o`, `libhplib.a`,
  `libhpg.a`, `libggl.a`, `libwin.a`, `libfsystem.a`,
  `libdecnumber.a`, and GCC 4.1.1.

The complete 2.0/SP2 source diff remains a work item until the archived source tree is locally materialized.

## HPGCC3

Authoritative site:
- https://hpgcc3.org/projects/hpgcc3/downloads

Latest archived release on the site:
- `R004.revA`
- published 2014-03-26
- source tarball: `hpgcc3_R004.revA.tar.gz`

Source reconstruction mirror used for code inspection:
- https://github.com/fpp3/hpgcc3_make

That mirror states that its source was extracted from the official R004.revA distribution and then changed only to remove the Eclipse build dependency.

## Acquisition policy

Do not silently substitute a mirror for an official archive when computing historical diffs.

For each release snapshot, record:

1. original archive filename;
2. source URL;
3. archive hash;
4. extracted file manifest;
5. any mirror commit used for cross-checking;
6. normalized compatibility changes applied by hpclang.

Calculator/runtime binary assets should remain outside this repository until redistribution terms have been audited.
