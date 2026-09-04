# Removing GCC and HPGCC from the build

The target state is:

> A fresh checkout can build the ARM payload and final calculator package from
> source without an HPGCC SDK, historical HPGCC object, GCC target compiler, or
> libgcc.

## Dependency elimination matrix

| Historical dependency | Clean replacement | Status |
| --- | --- | --- |
| `arm-elf-gcc` / `arm-none-eabi-gcc` | Clang | proven for ARMv4T source objects |
| GNU `as` | Clang integrated assembler | proven for clean loader |
| GNU `ld` | LLD | proven for clean ELF |
| GNU `ar` | `llvm-ar` | straightforward; library migration pending |
| `libgcc.a` | LLVM compiler-rt builtins | ARMv4T is supported upstream; integration pending |
| `MMUloader.o` | `runtime/hp39gs/loader.S` | host-link proven, hardware pending |
| `MMUglobals.o` | RWPI r9 + normal C state | eliminated in clean probe |
| HPGCC `crt0.o` | `runtime/hp39gs/runtime.c` | minimal replacement exists; state save/restore pending |
| `MMUld.script` | `runtime/hp39gs/link.ld` | clean replacement exists |
| old r10/GOT runtime patching | LLVM ROPI+RWPI | eliminated in clean probe |
| `elf2hp` for the 39gs path | direct ELF-to-HP39 aplet packer | format reconstruction in progress |
| `hp2aplet` | new hp39 packer | historical layout identified; implementation pending |
| prebuilt `apletcode.bin` / `apletepilog.bin` | rebuild minimal aplet from System RPL/Saturn source | pending |
| prebuilt `LIB275L.*` | rebuild or replace the minimal ARM launcher | pending |
| HP Tools binaries (`rplcomp/sasm/sload`) | build HP Tools 3.0.8 source with host Clang, or replace with narrow in-repo encoder | research path identified |

## Target compiler runtime

Removing GCC does not mean arithmetic helper routines disappear.

A current Clang ARMv4T soft-float object may reference routines such as:

- `__udivsi3`;
- `__divsi3`;
- `__muldf3`;
- `__divdf3`;
- `__adddf3`.

LLVM compiler-rt is explicitly the libgcc replacement and its bare-metal build
supports `armv4t`. hpclang should build a small compiler-rt builtins archive
for its selected ABI rather than copy HPGCC's `libgcc.a`.

For the earliest smoke programs, avoiding division/floating point allows a
complete link with no compiler-support archive at all.

## Why clean ROPI+RWPI is better than legacy ABI emulation

The hybrid r10/r9 experiment remains valuable for binary compatibility with old
HPGCC libraries. It is not the desired final ABI.

When every target object is rebuilt with Clang:

- ROPI handles code/read-only relocation;
- RWPI handles writable data through r9;
- no GOT is necessary;
- no GOT runtime patching is necessary;
- no r10 reservation is necessary;
- no LLVM fork is necessary.

This removes the only concrete reason found so far for an ARM
`-ffixed-r10` LLVM patch.

## Packaging is now the longest remaining chain

The historical 39G support is mostly a calculator packaging/launcher layer, not
a compiler feature. Its host-side converter builds an `HP39BinB` aplet,
generates `HP39DIR.CUR`/`HP39DIR.000`, embeds the ARM payload as a string,
and supplies ARMToolbox39 library 275.

The historical skeleton was compiled with HP Tools 3.0.8
(`rplcomp`, `sasm`, `sload`). HP Tools 3.0.8 source is publicly available
and explicitly covers 39/40-class machines. Therefore even this stage does not
require GCC or HPGCC binaries: the tools can be rebuilt with a host C compiler,
or their very small required functionality can ultimately be replaced by a
narrow hpclang packer.

## Milestones

1. **Clean ARM ELF** — no HPGCC/GCC inputs. Host proof exists.
2. **Clean HP string/container** — replace elf2hp for the required subset.
3. **Clean minimal HP39 aplet** — build a simple launcher aplet from source.
4. **Clean calculator-side ARM launcher** — no prebuilt LIB275 blob.
5. **39gs hardware smoke** — first fully source-rebuilt package.
6. Expand runtime/library surface and repeat on 49g+/50g backends.
