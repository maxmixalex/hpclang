# hpclang roadmap

Live short-term work is tracked in
[issue #1](https://github.com/zyf722/hpclang/issues/1). This file records only
durable milestones and completion criteria.

## Principles

- one application ABI: HPC1 / hpclang v1;
- one common ARMv4T runtime;
- thin calculator-family platform backends;
- no required GCC, HPGCC runtime or ROM replacement;
- historical code is research evidence, not a compatibility mode.

## M0 — Unified application ABI

**Complete.**

HPC1, AAPCS32, Thumb-1/ARMv4T, soft float and the fixed virtual layout are the
single supported application contract.

## M1 — Modern language/runtime stack

**Complete.**

LLVM compiler builtins, ARMv4T-safe integer/soft-float helpers, picolibc,
malloc, static TLS, constructors and normal C/C++ linkage are in the common
runtime.

## M2 — Common ARM bootstrap

**Complete for the validated families.**

The common bootstrap installs the HPC1 mappings, enters the application,
restores temporary state and returns to the calculator OS. HP39gs hardware and
the current emulator gates validate the design.

## M3 — HP39/40 family

**Runtime validated; sibling stock-launch closure remains.**

- [x] HP39gs real-hardware canonical runtime PASS and clean return.
- [x] HP39gs official-ROM direct E2E.
- [x] HP40gs official-ROM direct E2E.
- [x] HP39gs/HP40gs hardware-validation packages.
- [ ] HP39g+ model/outer-launch validation.
- [ ] HP40gs stock aplet/ARMToolbox outer-launch validation.

## M4 — HP48gII

**Active next milestone.**

- [ ] add an HP48gII E2E target profile;
- [ ] validate the real HP48gII hardware/memory model used by the emulator;
- [ ] implement only the thin HP48 launch adapter required by the common
  bootstrap;
- [ ] run the canonical HPC1 runtime and validate clean return;
- [ ] exercise the supported calculator-side package/launcher path.

Completion criterion:

> HP48gII executes the same HPC1 runtime image through the common bootstrap and
> returns to a responsive stock environment without a distinct application ABI.

## M5 — HP49g+/50g family

**HP50g complete; HP49g+ active next milestone.**

- [x] HP50g direct runtime E2E PASS + clean stock return.
- [x] HP50g original-system ARMToolbox install -> HPCLANG.HP -> PrRUN -> PASS.
- [ ] add HP49g+ to the direct-runtime E2E matrix;
- [ ] validate HP49g+ model-specific boot/launcher differences;
- [ ] close the supported HP49g+ stock package/launcher path.

Completion criterion:

> HP49g+ and HP50g use the same family backend and HPC1 ABI, with only genuine
> model differences below the common bootstrap.

## M6 — Automated emulator E2E

**Current representative gates complete; target expansion in progress.**

Current passing gates:

- HP39gs direct runtime;
- HP40gs direct runtime;
- HP50g direct runtime;
- HP50g full stock ARMToolbox/PrRUN launch.

Next expansion:

- [ ] HP48gII;
- [ ] HP49g+.

The Direct runtime gate owns common-bootstrap execution and return correctness.
Stock-launch gates own the user-visible calculator package/menu path.

## M7 — Common platform services

**Planned after M4/M5 sibling closure.**

Add display, keyboard, timers, storage and calculator-object/stack services
behind common APIs. Add only capabilities required by real applications.

## M8 — SDK stabilization

**Planned.**

Completion requires all three platform families to execute HPC1 through a
supported path, representative emulator coverage, primary real-hardware
coverage, coherent public headers/build UX and a fresh LLVM-only checkout.
