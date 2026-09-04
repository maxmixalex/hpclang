# Removing GCC and HPGCC from the build

The end state is simple:

> A fresh checkout builds hpclang applications for every supported ARM
> calculator using LLVM tools and hpclang source only.

## Toolchain

| Need | hpclang |
| --- | --- |
| C/C++ compiler | Clang |
| assembler | Clang integrated assembler |
| linker | LLD |
| archiver | llvm-ar |
| compiler helpers | LLVM compiler-rt builtins |
| executable packer | hpclang HPC1 packer |
| calculator package | hpclang family backend |

There is no normal-build role for GCC, libgcc, historical HPGCC object files or
HPGCC runtime libraries.

## Already proven

The unified core CI currently proves:

- ARMv4T AAPCS32 code generation;
- Thumb-1 C and ARM startup interworking;
- fixed virtual RX/RW segments;
- initialized data and BSS layout;
- model-independent HPC1 output;
- no unresolved symbols in the current core image.

## Next dependency: compiler-rt

Real applications require compiler helper routines for operations that ARMv4T
cannot express directly, including integer division, 64-bit arithmetic and
software floating point.

These will come from LLVM compiler-rt, not from HPGCC's copied `libgcc.a`.

hpclang should build a normal ARMv4T compiler-rt builtins archive matching the
same AAPCS32/soft-float ABI as application code.

## libc/runtime

After compiler-rt, the common runtime should provide a small normal libc layer:

- memory/string primitives;
- allocator;
- basic formatting;
- integer conversion;
- selected math support.

Calculator I/O is implemented underneath through hpclang platform services,
not by preserving old HPGCC syscall wrappers.

## Platform completion

Each family is complete when its backend can take the same HPC1 image,
construct the common virtual environment, run it, restore the calculator
cleanly and pass the same runtime test suite.

The test suite should include:

- globals/data/BSS;
- heap;
- integer division and 64-bit helpers;
- soft-float arithmetic;
- ARM/Thumb calls;
- LCD;
- keyboard;
- timer;
- filesystem where present;
- calculator object/stack interaction where present.
