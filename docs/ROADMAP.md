# hpclang roadmap

The live short-term status is tracked in
[issue #1](https://github.com/zyf722/hpclang/issues/1). This document defines
the durable milestones and their completion criteria.

## Principles

hpclang is a clean LLVM-native SDK, not an HPGCC compatibility layer.

The project maintains:

- one application ABI;
- one common runtime;
- thin calculator-family platform backends;
- historical HPGCC code as research evidence only.

Do not add compatibility paths for obsolete hpclang experiments unless they are
required to run current applications.

## M0 — Unified application ABI

**Status: complete**

Completion criteria:

- HPC1 is the sole supported logical executable format;
- ARMv4T / AAPCS32 / soft-float;
- Thumb-1 C/C++ by default;
- fixed virtual code and writable addresses;
- no APCS-GNU, ROPI/RWPI, r9/r10 static-base ABI or Launcher V3 flags exposed
  to applications;
- all supported calculator profiles select the same ABI.

## M1 — Modern language/runtime stack

**Status: complete**

Completed:

- Arm Toolchain for Embedded Armv4T multilib selected in CI;
- compiler-rt integer division, 64-bit arithmetic and software floating point;
- picolibc malloc, memory/string functions and formatting;
- static TLS;
- init/fini arrays;
- hosted C/C++ language semantics.

Also validated:

- libc++ links for Armv4T with std::string and global constructors;
- the only extra platform hooks needed by the smoke are normal termination/stdio hooks;
- exceptions and RTTI remain intentionally disabled until a real application needs them.

Completion criterion:

> A normal C and C++ HPC1 application links using only hpclang + the canonical
> LLVM/Arm embedded runtime stack, with no HPGCC or GCC runtime objects.

## M2 — Common ARM bootstrap

**Status: implementation complete; HP39 entry and return validated on hardware**

Build one common ARM bootstrap used by all calculator families.

Responsibilities:

- receive a small family-specific launch context;
- acquire/map writable and executable pages;
- install the common HPC1 virtual layout;
- initialize the HPC1 boot page;
- perform required cache/TLB maintenance;
- establish the stack;
- enter HPC1;
- restore state needed before returning to the calculator OS.

Family-specific Saturn/System-RPL code should remain as small as practical.

Completion criterion:

> The bootstrap contains no calculator-model-specific policy except through a
> small platform adapter/context.

## M3 — HP39/40 family

**Status: real-hardware HPC1 entry/return validated; runtime and emulator gates pending**

Targets:

- HP 39g+;
- HP 39gs;
- HP 40gs.

Steps:

1. [x] define the clean HP39 stock-launcher stage0/stage1 boundary;
2. [x] package HPC1 through the stock HP39 Aplet outer container;
3. [x] build a visible + machine-readable HPC1 smoke in public CI;
4. [x] restore stock HPGCC39 transfer filenames for maximum Conn3x compatibility;
5. [x] enter HPC1 on a real HP39gs over USB: model 2 and RW 0x00014000 observed;
6. [x] validate return/state restoration on the same real HP39gs;
7. [ ] execute the same smoke in Emu48 through a driverless serial peer;
8. [ ] validate globals/BSS/TLS/heap/compiler-rt/libc;
9. [ ] verify sibling 39g+/40gs differences.

The first hardware run reached `HPC1 PASS` and therefore validated the complete
stock-Aplet -> ARMToolbox stage0 -> stage1 -> common bootstrap -> HPC1 entry
chain. The first two diagnostic builds are not return-path evidence: one waited
on its own APLET GPIO poll with IRQ/FIQ masked; the next used an uncalibrated
60,000,000-iteration busy-wait while IRQ/FIQ remained masked. Missing letters
in those pages were independently traced to omissions in the tiny test font.

The active return gate now contains no display, GPIO, interrupt masking or
delay: `main()` records the boot fields and returns immediately.

Completion criterion:

> The canonical HPC1 runtime test suite passes in Emu48 and on the real 39gs
> without a legacy Launcher V3 application ABI.

## M4 — HP48gII family

**Status: planned**

Steps:

- verify fixed virtual slots;
- implement the thin HP48gII launch shim;
- run the same HPC1 runtime image;
- validate calculator-object/stack integration.

Completion criterion:

> The same HPC1 test binary used by HP39 can execute through the HP48gII
> backend without application recompilation for a different ABI.

## M5 — HP49g+/50g family

**Status: planned**

Steps:

- replace the historical VC launcher dependency with the common bootstrap;
- validate on Emu48;
- validate on real hardware when useful;
- preserve only platform-specific packaging and OS integration.

Completion criterion:

> HP49g+ and HP50g execute the same HPC1 ABI through the same common bootstrap.

## M6 — Automated emulator E2E

**Status: design validated; driverless HP39 transport in progress**

Validated design:

- Emu48 exposes direct core APIs for engine state, document/state loading,
  object loading, keyboard injection and memory inspection;
- use those APIs from a thin headless harness instead of GUI-coordinate
  automation;
- HP39/40 Aplets cannot use Emu48's object-stack loader and must enter through
  the calculator communication path;
- Emu48's calculator UART behavior is separable from its Win32 host-COM
  transport, so HP39 automation can replace only the host endpoint with an
  in-memory byte peer rather than requiring a virtual COM kernel driver;
- boot a known calculator ROM/state, transfer the generated package, execute it
  and inspect a machine-readable result in emulated memory;
- HP published a dedicated 2.15 emulator ROM for 48gII/49g+/50g, making that
  family the simplest public-CI ROM source;
- HP also published the 39gs 2.22 flash image; its reproducible conversion into
  the Emu48 ROM format still needs to be pinned down.

HPComm38 and Emu48 source are protocol/emulator evidence only. Keep them as
external test/research dependencies and independently implement the narrow
hpclang-side transfer peer.

Completion criterion:

> At least one calculator-family E2E test runs automatically from source build
> through calculator execution and returns machine-readable success.

## M7 — Common platform services

**Status: planned**

Add APIs only after the common execution path is stable:

- display/framebuffer;
- keyboard;
- timers;
- storage/filesystem;
- calculator object/stack bridge;
- optional audio and other hardware services.

Do not copy the historical HPGCC API surface wholesale. Define the smallest
coherent hpclang API needed by real applications.

## M8 — SDK stabilization

**Status: planned**

Completion criteria:

- all three platform families pass the common runtime suite;
- emulator E2E covers representative targets;
- real-hardware validation covers at least the primary development models;
- public headers and package/build UX are coherent;
- legacy research code is clearly separated from supported code;
- a fresh checkout can build applications without GCC or HPGCC.


### HP39gs hardware checkpoint: entry and return

Real hardware now validates both directions of the common execution chain.

Observed entry:

```text
HPC1 PASS
MODEL 00000002
RW 00014000
```

A later side-effect-free smoke performed no framebuffer, GPIO, interrupt or
delay operations and returned immediately from `main()`. The LCD still showed
the prior diagnostic contents until the stock OS received a redraw-triggering
key event, but ENTER/ON responded and APLET opened the normal APLET library.

Therefore control flow, interrupts and calculator state had returned to the
stock OS. Immediate LCD repaint is a separate HP39 platform-integration concern,
not a bootstrap-return failure.


### HP39gs runtime checkpoint: interrupt isolation

The first extended runtime validation could hang before producing its final
result, and the progressive diagnostic could be interrupted while drawing the
first glyph. The earlier visible smoke was stable only after it masked IRQ/FIQ,
while the very short immediate-return smoke could complete without waiting long
enough to observe the race.

The common bootstrap now owns this requirement: it saves CPSR and masks IRQ/FIQ
before installing the temporary HPC1 MMU entries, then restores the stock MMU
entries and only afterwards restores the original CPSR. Stock OS interrupt
handlers must never execute against the HPC1 virtual layout.

Hardware validation of this change is pending.
