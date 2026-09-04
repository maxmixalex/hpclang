# 39gs-specific HPGCC surface

This document separates genuine 39G support from generic HPGCC 1.x code that
merely happens to mention the 39g+.

## Clearly isolated 39G support

The patched 1.1 SDK contains a dedicated `sources/HP39-Aplet/` tree. The
important parts are:

### Host aplet builder

`hp2aplet/hp2aplet.c`

It converts the HPGCC ARM string into an HP39 aplet and generates the directory
support files. Observable format details include:

- aplet header `HP39BinB`;
- attached ROM part/library id 0x113 (decimal 275);
- output `HPGCC000.000`;
- directory files `HP39DIR.CUR` and `HP39DIR.000`;
- bundled `LIB275L.000`;
- ARM payload embedded after dropping the outer `HPHP49-C` transfer header.

`hp2aplet/hp39wiz.c` only gathers aplet title/author/grob metadata. It has no
target ABI significance.

### Aplet skeleton

`Skeleton/hpgccaplet.s` is a full HP39 System RPL aplet template. It contains
far more policy/UI than a minimal hpclang launcher needs:

- standard view lifecycle;
- splash/about screens;
- Enter-key action;
- custom persistent data;
- calls to ARMToolbox39 ROMPs.

The build uses `rplcomp`, `sasm`, and `sload` from HP Tools.

`Skeleton/howtopatch.txt` documents how historical `hp2aplet` split a built
aplet into `apletcode.bin` and `apletepilog.bin` and patched offsets around
the embedded ARM string.

A clean hpclang backend should generate a minimal aplet directly from source
rather than preserve these prebuilt fragments.

### ARMToolbox39 / library 275

`LIB275/lib275.s` defines ROM id 0x113 and the ARM launch entry used by the
aplet.

`LIB275/SEXE.s` contains the actual Saturn/System-RPL-side ARM launcher. It:

- accepts the ARM object/string;
- aligns the ARM code when necessary;
- converts Saturn addresses to ARM addresses;
- allocates or accepts RAM;
- builds the virtual-memory page table for MMU launch mode;
- invokes the calculator's ARM system switch.

One explicit 39G adaptation is documented in that source: secondary HPGCC
function-entry dispatch was removed, leaving only the primary entry point.

`LIB275/SUPROM39.A` and the Skeleton `=Entry39.h` provide HP39 supported ROM
entry addresses.

These files are the calculator-specific bridge. They are not compiler backend
changes.

## What has not yet been proven 39G-specific

The main HPGCC libraries (`hplib`, `hpg`, `ggl`, `win`) contain comments
and conditionals referring to 39g+/hardware behavior. A mention alone is not
evidence that a file was introduced or patched for the 39G release.

An exact generic-1.1 vs 1.1-p1-with-39G archive diff is still required before
claiming the complete patch set outside `HP39-Aplet/`.

## Can these pieces move to HPGCC 2.0?

The packaging concepts can, because they sit outside the C compiler:

- HP39 directory/aplet container;
- System RPL aplet lifecycle;
- calculator-side ARM launcher;
- payload metadata.

However HPGCC 2.0's frozen VC source changed the ARM memory contract. VC code is
linked at 0x01220000 with writable data at 0x01101000 and its loader no longer
installs the 1.1 MMU mapping itself.

Therefore direct use of VC on 39gs depends on the HP39 launcher creating the
same virtual mapping. Until that is demonstrated, the safe migration is:

- keep the known 39G launcher entry/MMU contract;
- adopt 2.0's removal of r10/GOT;
- use LLVM ROPI+RWPI for new code.

That is exactly what the clean loader experiment does.

## Can these pieces move to HPGCC3?

The HP39 aplet and launcher layer is independent of HPGCC3's compiler sources,
but HPGCC3's runtime contract is not portable to 39gs: it relies on a custom
50g ROM/ROMLIB and its own launch API.

Useful 3.0 source can still be backported selectively:

- portable C algorithms;
- data structures;
- UI/math code that does not call ROMLIB;
- improved ARM/Thumb-friendly implementations.

ROMLIB calls, RLloader and CC3-specific launch metadata remain a 50g backend.

## hpclang ownership

The end state should make the family boundary explicit:

    core/
        portable C APIs and algorithms

    runtime/hp39gs/
        ARMv4T loader, hardware state, linker layout

    platform/hp39/
        HP39 aplet/container + Saturn/System RPL launcher

    platform/hp49/
        49g+/50g stock-OS packaging/launcher

    platform/hp50g-romlib/
        optional HPGCC3-compatible ROMLIB backend

39gs support then stops being a patch hidden inside an old HPGCC SDK and
becomes a first-class maintained backend.
