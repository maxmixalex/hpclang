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

**Status: real-hardware canonical runtime validated; Saturn-side Emu48 automation optional**

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
7. [ ] automate HP39 stock UI/package-transfer coverage in Emu48 where useful;
8. [x] validate globals/BSS/TLS/heap/compiler-rt/libc on real HP39gs;
9. [ ] verify sibling 39g+/40gs differences.

Emu48 does not emulate the ARM native execution engine used by HPGCC/HPC1 on
the Apple-family calculators: its NATIVE/ARMSYS/ARMSAT paths are not
implemented. It therefore cannot be an execution oracle for the HP39 HPC1
runtime. Real HP39gs hardware is the authoritative execution gate; Emu48 may
still cover the Saturn-side stock UI and transfer protocol.

The first hardware run reached `HPC1 PASS` and therefore validated the complete
stock-Aplet -> ARMToolbox stage0 -> stage1 -> common bootstrap -> HPC1 entry
chain. The first two diagnostic builds are not return-path evidence: one waited
on its own APLET GPIO poll with IRQ/FIQ masked; the next used an uncalibrated
60,000,000-iteration busy-wait while IRQ/FIQ remained masked. Missing letters
in those pages were independently traced to omissions in the tiny test font.

The active return gate now contains no display, GPIO, interrupt masking or
delay: `main()` records the boot fields and returns immediately.

Completion criterion:

> The canonical HPC1 runtime test suite passes on real HP39gs hardware without
> a legacy Launcher V3 application ABI, with any Emu48 coverage limited to the
> Saturn-side stock UI/package-transfer path.

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
- validate ARM execution on x50ng/x49gp-class hardware emulation;
- validate on real hardware when useful;
- preserve only platform-specific packaging and OS integration.

Completion criterion:

> HP49g+ and HP50g execute the same HPC1 ABI through the same common bootstrap.

## M6 — Automated emulator E2E

**Status: headless infrastructure active; true ARM execution backend moving to x50ng**

Validated design:

- Emu48+ can be driven from a headless x86 controller for stock UI, state,
  keyboard, memory/debugger API and screenshot infrastructure;
- Emu48 does not implement the Apple-family ARM-native NATIVE/ARMSYS/ARMSAT
  execution path and therefore cannot execute HPC1;
- x50ng/x49gp emulates the actual S3C2410/ARM hardware and is the execution
  backend for automated HPC1 tests;
- x50ng exposes calculator keys and the LCD controller through host-side APIs,
  allowing a no-GUI test frontend to inject input and capture a deterministic
  131x80 LCD image directly;
- keep both emulators as external test dependencies. No emulator implementation
  becomes part of the hpclang runtime.

Execution plan:

1. [x] build the Emu48+ controller and deterministic screenshot pipeline;
2. [ ] boot official 49g+/50g ARM firmware in x50ng without a runtime GUI and
   capture its LCD as a CI artifact;
3. [ ] run common bootstrap + the canonical HPC1 runtime image under x50ng and
   stop on machine-readable PASS;
4. [ ] capture the PASS LCD frame in CI;
5. [ ] upgrade the ARM execution gate to the real stock 49g+/50g
   package/launcher path.

Completion criterion:

> At least one real ARM calculator-family path runs automatically from source
> build through HPC1 execution, produces machine-readable success and uploads a
> deterministic LCD screenshot.

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
