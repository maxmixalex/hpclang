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

**Status: HP39gs hardware PASS; HP39gs/HP40gs direct-runtime true-ARM E2E PASS; stock sibling outer-launch validation pending**

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
9. [x] build a dedicated HP40gs model-3 hardware-validation artifact in CI;
10. [x] execute model 3 on official HP40gs 2.22 ROM in the true-ARM x50ng backend and validate direct HPC1 entry/runtime/return;
11. [ ] automate the stock HP39gs and HP40gs Aplet/ARMToolbox outer path from deterministic post-transfer fixtures and validate return;

The real HP39gs is the authoritative execution gate for the HP39/40 family.
HP39gs and HP40gs use the same 75 MHz S3C2410A/ARM920T platform, 256 KiB RAM
and 131x64 LCD. The important HP40gs differences are the 2 MiB flash/CAS image,
the absence of infrared, its calculator-side ROM, and model identity. Therefore
the common ARM runtime does not need a separate HP40gs-specific implementation;
support is complete only after the stock HP40gs Aplet/ARMToolbox outer path is
executed at least once and returns cleanly with model 3.

Little-Apple true-ARM CI now boots the official HP39gs and HP40gs 2.22 ROM
images to their stock HOME states and runs the direct HPC1 image through
stage0 -> stage1 -> common bootstrap -> runtime -> return.

The split is now:

- real HP39gs hardware proves the stock HP39 Aplet/ARMToolbox outer path;
- official HP39gs and HP40gs ROMs in x50ng prove the common ARM execution path
  independently for model 2 and model 3;
- the emulator Stock tier will start from deterministic post-transfer Aplet
  fixtures, then drive the stock APLET UI and outer launcher entirely through
  the calculator key matrix;
- Conn3x/USB/serial host transport is kept separate from the runtime gate unless
  x50ng gains a maintained host transport suitable for reliable automation.

The first hardware run reached `HPC1 PASS` and therefore validated the complete
stock-Aplet -> ARMToolbox stage0 -> stage1 -> common bootstrap -> HPC1 entry
chain. Later real-hardware work closed the return path and the direct runtime
suite.

Completion criterion:

> The direct HPC1 runtime passes on real HP39gs hardware, and each sibling
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

**Status: HP50g stock ARMToolbox launch E2E PASS; HP49g+ sibling validation pending**

Completed on official HP50g 2.15 firmware in x50ng:

- [x] boot the stock ROM to HOME;
- [x] execute the common direct HPC1 runtime and return to a responsive stock OS;
- [x] install ARMToolbox/library 275 through the original calculator UI;
- [x] preserve the installed calculator state across the install and launch phases;
- [x] materialize `HPCLANG.HP` from SD through the stock File Manager;
- [x] enter `ARMTo` and execute `PrRUN`;
- [x] reach the same machine-readable `0x50415353` / `RUNTIME PASS` result as
  the direct-runtime gate;
- [x] capture a clean 131x80 PASS frame from the live LCD controller.

The two HP50g emulator gates intentionally have different boundaries:

- **Direct runtime** validates stage0 -> stage1 -> common bootstrap -> HPC1,
  temporary mapping restoration and clean return to the stock OS.
- **Stock launch** validates the real user path through the original HP50g UI,
  ARMToolbox and `PrRUN`, and stops once HPC1 reports PASS.

The stock gate does not inject a synthetic key after PASS. During development,
that extra host-side key event could make the pinned x50ng/QEMU teardown abort
after the runtime had already passed. Return correctness is therefore owned by
the direct-runtime gate rather than redundantly mixed into the stock UI gate.

Remaining:

- [ ] execute the equivalent supported package/launcher path on HP49g+;
- [ ] validate HP49g+/50g on real hardware when useful.

Completion criterion:

> HP49g+ and HP50g execute the same HPC1 ABI through the same common bootstrap
> and their supported stock package/launcher paths.

## M6 — Automated ARM emulator E2E

**Status: complete for the current representative emulator gates**

The unattended x50ng gates now cover:

- HP39gs direct HPC1 runtime: PASS + stock return;
- HP40gs direct HPC1 runtime: PASS + stock return;
- HP50g direct HPC1 runtime: PASS + stock return;
- official HP39gs 2.22 ROM boot: PASS;
- official HP40gs 2.22 ROM boot: PASS;
- official HP50g 2.15 stock UI -> ARMToolbox -> `HPCLANG.HP` -> `PrRUN`
  -> HPC1 PASS: PASS.

All calculator input used by these gates is driven through the emulated key
matrix rather than host GUI coordinates. LCD evidence is generated from the
live emulated controller and published separately from the source branch.

The successful direct-runtime checkpoint includes restored HPC1 data/code
mappings, the temporary stage mapping and SWI vector, followed by resumed stock
execution. The stock HP50g gate adds the end-user launcher path without making
menu automation part of the common-runtime correctness test.

CI screenshots live on the dedicated orphan `ci-evidence` branch and are
tracked through Git LFS. Weekly maintenance keeps the newest 20 evidence run
directories per family, rewrites that branch as a single root history, keeps up
to 20 completed runs for current workflows and only 2 for removed workflows.
Transient E2E artifacts use short retention windows rather than accumulating
indefinitely.

Completion criterion:

> A real ARM calculator-family emulator runs automatically from source build
> through direct HPC1 execution and return, produces machine-readable success,
> resumes a responsive stock OS and uploads deterministic LCD evidence.

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

hpclang now keeps compiler-rt as the direct runtime but supplies a narrow
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


### HP39gs hardware checkpoint: direct runtime PASS

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
