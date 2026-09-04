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

**Status: HP39gs canonical runtime validated on real hardware; 39g+/40gs sibling launch validation pending**

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
7. [x] validate globals/BSS/TLS/heap/compiler-rt/libc on real HP39gs;
8. [ ] verify HP39g+ outer-launcher/model differences;
9. [ ] verify HP40gs outer-launcher/model/ROM differences.

The real HP39gs is the authoritative execution gate for the HP39/40 family.
HP39gs and HP40gs use the same 75 MHz S3C2410A/ARM920T platform, 256 KiB RAM
and 131x64 LCD. The important HP40gs differences are the 2 MiB flash/CAS image,
the absence of infrared, its calculator-side ROM, and model identity. Therefore
the common ARM runtime does not need a separate HP40gs-specific implementation;
support is complete only after the stock HP40gs Aplet/ARMToolbox outer path is
executed at least once and returns cleanly with model 3.

Until a true Little-Apple ARM emulator exists, sibling validation is split:

- CI proves the common HPC1/runtime/bootstrap on Armv4T and on x50ng's real
  ARM920T/S3C2410 execution engine;
- the HP39gs real machine proves the HP39/40 family stage0 + stock Aplet path;
- HP40gs requires one real-machine (or future true-ARM emulator) launch/return
  check, not a second re-validation of compiler-rt/TLS/libc internals.

The first hardware run reached `HPC1 PASS` and therefore validated the complete
stock-Aplet -> ARMToolbox stage0 -> stage1 -> common bootstrap -> HPC1 entry
chain. Later real-hardware work closed the return path and the canonical runtime
suite.

Completion criterion:

> The canonical HPC1 runtime passes on real HP39gs hardware, and each sibling
> target has its stock outer launcher/model integration executed at least once.

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

**Status: canonical HPC1 runtime PASS reached on x50ng; stage0 return and stock package path pending**

Steps:

- [x] boot official 49g+/50g firmware headlessly in x50ng;
- [x] execute the same canonical HPC1 runtime image used by HP39gs through
  stage0 -> stage1 -> common bootstrap;
- [x] observe machine-readable `0x50415353` and capture the live
  `RUNTIME PASS` LCD;
- [ ] close the synthetic outer-launcher return boundary;
- [ ] replace the synthetic ARMSYS allocation with the real stock 49g+/50g
  package/launcher path;
- [ ] validate on real hardware when useful.

Completion criterion:

> HP49g+ and HP50g execute the same HPC1 ABI through the same common bootstrap
> and the supported stock package/launcher path.

## M6 — Automated ARM emulator E2E

**Status: x50ng executes canonical HPC1; full return gate in progress**

Validated design:

- x50ng emulates the actual S3C2410/ARM920T hardware used by the 49g+/50g
  family and is the automated HPC1 execution backend;
- x50ng exposes calculator keys and the LCD controller through host-side APIs,
  so CI needs no GUI, mouse coordinates or image recognition;
- official 49g+/50g firmware boots to normal HOME in headless CI;
- the same canonical HPC1 image used on the real HP39gs reaches machine-readable
  PASS under x50ng and produces a deterministic 131x80 PASS screenshot;
- the remaining failure is after PASS, at the synthetic outer-launcher return
  boundary; HPC1 code/data mappings and the SWI vector are already restored.

Execution plan:

1. [x] boot official 49g+/50g ARM firmware and capture the LCD;
2. [x] run common bootstrap + canonical HPC1 and observe machine-readable PASS;
3. [x] capture the live PASS LCD frame in CI;
4. [ ] complete synthetic stage0 return and resume the saved stock HOME state;
5. [ ] upgrade the execution gate to the real stock 49g+/50g package/launcher.

Completion criterion:

> At least one real ARM calculator-family path runs automatically from source
> build through HPC1 execution, produces machine-readable success, returns to a
> responsive stock OS state and uploads deterministic before/PASS/after LCD
> evidence.

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


### HP39gs interrupt-isolation correction

The first common-bootstrap IRQ/FIQ isolation attempt used direct CPSR writes
from the normal bootstrap call chain. Real HP39gs hardware regressed to a white
screen before HPC1 diagnostics, so that implementation was rejected.

The bootstrap now uses the already-established SWI-0 supervisor trampoline.
The disable helper captures the caller CPSR from `SPSR_svc`, returns to the
same caller with IRQ/FIQ masked via exception return, and later restores the
saved CPSR through a second SWI after the stock MMU mapping is back. This works
independently of whether the stock launcher entered hpclang from a privileged
or unprivileged ARM mode.

Hardware re-validation is pending.


### HP39gs runtime checkpoint: ARMv4T 64-bit divmod interworking

After SWI-supervisor interrupt isolation, real HP39gs hardware progressed
reliably through TLS, constructors and 32-bit division and stopped at
`STEP 15`, immediately before/inside the 64-bit unsigned division check.

ATfE 22.1.0 resolves the operation through LLVM compiler-rt's ARM
`__aeabi_uldivmod` wrapper. Upstream's generic ARM wrapper restores its saved
return address with `pop {..., pc}`. That epilogue cannot interwork from ARM
state back to a Thumb caller on ARMv4T.

hpclang now supplies narrow ARMv4T-safe `__aeabi_uldivmod` and
`__aeabi_ldivmod` wrappers. They retain compiler-rt's canonical
`__udivmoddi4`/`__divmoddi4` algorithms and only replace the epilogue with
explicit LR restore plus `bx lr`.

Hardware validation is pending.


### HP39gs runtime checkpoint: target-built generic soft-float

The ARMv4T-safe 64-bit divmod veneer was validated on real HP39gs hardware:
the progressive runtime test advanced from `STEP 15` to `STEP 16`.

`STEP 16` then stopped in the double soft-float section. ATfE's Armv4T
multilib still resolves double arithmetic to compiler-rt's optimized ARM
assembly implementations. Those sources contain instructions and return
patterns intended for newer ARM revisions (including paths with `movw`,
`clz`, and `pop {..., pc}`), so merely fixing one entry epilogue would leave
the runtime structurally fragile.

hpclang now keeps compiler-rt as the canonical runtime but supplies a narrow
target-built double core following compiler-rt's generic C algorithms. It is
compiled explicitly with `-march=armv4t -marm`; thin EABI veneers provide the
Thumb-1 application boundary and return with `bx lr`. The first covered
operations are double multiply/add/subtract and ordered/unordered comparisons.

A host IEEE-754 self-test runs before every ATfE probe, and the HP39 hardware
test now has sub-checkpoints 61-64 inside the floating-point section.

Hardware validation is pending.


### HP39gs runtime checkpoint: visible final result

The first real-hardware run with the target-built ARMv4T double soft-float no
longer stopped at any progressive checkpoint. Launching the Aplet produced a
brief busy interval, then returned to a responsive stock OS; APLET opened the
normal menu. This strongly indicates that the full runtime validation completed
and returned.

The test result was not visually conclusive because `finish()` wrote
`RUNTIME PASS/FAIL`, then the diagnostic `hpclang_start()` immediately
overwrote it with teardown checkpoints `F0` and `FF` before returning to the
stock UI.

The hardware test now preserves the final PASS/FAIL page for a short finite
test-only hold and no longer overwrites it during fini-array teardown.
Production runtime behavior is unchanged.


### HP39gs hardware checkpoint: canonical runtime PASS

Real HP39gs hardware now reports:

```text
RUNTIME PASS
```

The validated path includes:

- initialized globals and BSS;
- static TLS (.tdata/.tbss) and TLS read/write;
- init-array constructors;
- compiler/runtime 32-bit and 64-bit division;
- ARMv4T-target-built double soft-float multiply/add/compare;
- picolibc memory/string routines;
- malloc/free;
- snprintf;
- clean return to the responsive stock OS.

The ARMv4T EABI divmod veneers, target-built soft-float core and common
bootstrap interrupt isolation are architecture/common-runtime code, not
HP39-specific platform code. HP39-specific code remains limited to packaging,
stage0/platform integration and hardware-facing validation helpers. The common
pieces still require execution validation on HP48gII and HP49g+/50g.
