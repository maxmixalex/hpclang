# Removing GCC and HPGCC from the build

The target state is:

> A fresh checkout can build calculator-ready programs from source for every
> supported hpclang target without an HPGCC SDK, historical HPGCC target
> object, GCC target compiler or libgcc.

This requirement applies independently to the HP39/40, 48gII and 49g+/50g
backends.

## Dependency elimination matrix

| Historical dependency | Clean replacement | Status |
| --- | --- | --- |
| `arm-elf-gcc` / `arm-none-eabi-gcc` | Clang | proven for ARMv4T source objects |
| GNU `as` | Clang integrated assembler | proven for clean loader |
| GNU `ld` | LLD | proven for clean ELF |
| GNU `ar` | `llvm-ar` | library migration pending |
| `libgcc.a` | LLVM compiler-rt builtins | integration pending |
| HPGCC 1.x MMU startup objects | new source runtime | HP39 bootstrap host-proven; other families pending |
| old r10/GOT runtime patching | LLVM ROPI+RWPI | eliminated in clean bootstrap |
| `elf2hp` | direct ELF-to-Launcher-V3 packer | clean implementation exists |
| historical calculator package builders | family-specific source packers | HP39 reconstruction in progress |
| prebuilt ARMToolbox/LIB275 artifacts | rebuild/installable family support libraries from source | pending |
| HP Tools host binaries | rebuild source with a modern host compiler or replace the narrow required encoders | pending |

## Compiler support runtime

Removing GCC does not mean arithmetic helper routines disappear.

Clang ARMv4T soft-float code can reference helpers such as:

- `__udivsi3`;
- `__divsi3`;
- `__muldf3`;
- `__divdf3`;
- `__adddf3`.

hpclang should build the required LLVM compiler-rt builtins for the selected
ARMv4T ABI rather than copy an HPGCC `libgcc.a`.

## Family bootstrap requirements

### HP39/40

Replace:

- historical MMUloader/MMUglobals;
- hp2aplet;
- prebuilt aplet fragments;
- prebuilt ARMToolbox39/library 275.

The current Clang+LLD+Launcher-V3 payload is the first clean implementation.

### HP48gII

Rebuild the dedicated ARMToolbox48 launcher/package path from source.

Do not reuse 49/50 Saturn address conversion or memory limits merely because
the ARM CPU family is similar.

### HP49g+ / HP50g

Rebuild the ordinary ARMToolbox launcher and RPL packaging path from source.

The initial common profile should accept Launcher V3 simple/MMU payloads. A
second profile should reproduce HPGCC 2.0 Launcher V3 v1.2 virtual-code
addressing.

### HPGCC3 compatibility

This is optional.

If the calculator already has a compatible HPGCC3 ROMLIB environment, hpclang
may emit CC3/EXT3 programs that use it. The core SDK must never require ROM
replacement.

## Why clean ROPI+RWPI is preferred

When every target object is rebuilt with Clang:

- ROPI handles code/read-only relocation;
- RWPI handles writable data through r9;
- no historical GOT is necessary;
- no GOT runtime patching is necessary;
- no r10 reservation is necessary;
- no LLVM fork is currently necessary.

Legacy GCC-built objects remain a compatibility/testing concern, not the normal
application ABI.

## Milestones

1. **Common clean ARM toolchain** — Clang/integrated assembler/LLD/compiler-rt.
2. **Common Launcher V3 simple/MMU packer** — one payload format usable by all
   historical launcher families that implement the common contract.
3. **HP39/40 source bootstrap** — aplet + launcher/support library built without
   historical binaries; hardware validation on 39gs first, then sibling models.
4. **HP48gII source bootstrap** — dedicated launcher/package path and hardware
   validation.
5. **HP49g+/50g source bootstrap** — ordinary ARMToolbox path and hardware
   validation on both models.
6. **HPGCC 2.0 VC profile** — Launcher V3 v1.2 virtual-code compatibility for
   supported 49/50 environments, followed by feasibility tests on other
   families.
7. **Library migration** — compiler-rt, libc subset, hpg/ggl, Saturn object APIs,
   filesystem and hardware services with explicit capabilities.
8. **Optional HPGCC3 compatibility** — CC3/EXT3 + versioned ROMLIB when already
   installed, never as a prerequisite.

A target is not considered complete merely because a smoke program links. It
must eventually pass a non-trivial runtime suite covering global/static data,
heap, integer/soft-float helpers, LCD, keyboard, timer, ARM/Thumb interworking
and calculator-OS interaction.
