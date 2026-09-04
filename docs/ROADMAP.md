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

**Status: in progress**

Completed:

- Arm Toolchain for Embedded Armv4T multilib selected in CI;
- compiler-rt integer division, 64-bit arithmetic and software floating point;
- picolibc malloc, memory/string functions and formatting;
- static TLS;
- init/fini arrays;
- hosted C/C++ language semantics.

Remaining:

- close libc++ smoke with constructors and std::string;
- define the minimal platform hooks required by libc/libc++;
- keep exceptions and RTTI disabled unless an application demonstrates a need.

Completion criterion:

> A normal C and C++ HPC1 application links using only hpclang + the canonical
> LLVM/Arm embedded runtime stack, with no HPGCC or GCC runtime objects.

## M2 — Common ARM bootstrap

**Status: next**

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

**Status: next**

Targets:

- HP 39g+;
- HP 39gs;
- HP 40gs.

Steps:

1. verify the proposed HPC1 virtual slots in the stock Emu48 memory map;
2. package HPC1 through a clean HP39-family outer container;
3. execute a smoke image in Emu48;
4. validate globals/BSS/TLS/heap/compiler-rt/libc;
5. validate exit/state restoration;
6. repeat on real HP39gs hardware;
7. verify sibling 39g+/40gs differences.

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

**Status: investigation**

Preferred design:

1. use Emu48/Emu48+ source or emulator core directly;
2. avoid GUI-coordinate automation;
3. boot a known calculator state/ROM;
4. inject/load the generated calculator package;
5. run the test;
6. expose an unambiguous machine-readable PASS/FAIL result.

CI must not commit or redistribute calculator ROM images without a clear legal
basis. If a suitable ROM cannot be obtained reproducibly in public CI, keep the
same harness usable from a local/self-hosted runner with a user-provided ROM.

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
