# hpclang roadmap

Live short-term work is tracked in
[issue #1](https://github.com/zyf722/hpclang/issues/1). This file records only
durable milestones and completion criteria.

## Principles

- one application ABI: HPC1 / hpclang v1;
- one common ARMv4T runtime;
- thin calculator-family platform backends;
- no required GCC, HPGCC runtime or ROM replacement;
- historical code is research evidence, not a compatibility mode;
- emulator fixtures are generated from current build products rather than
  committed as opaque calculator-state snapshots.

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

**Runtime and automated stock-launch validation complete for HP39gs/HP40gs.**

- [x] HP39gs real-hardware canonical runtime PASS and clean return.
- [x] HP39gs official-ROM direct E2E.
- [x] HP40gs official-ROM direct E2E.
- [x] HP39gs/HP40gs hardware-validation packages.
- [x] HP39g+ model-1 hardware-validation package using the shared HPGCC39
  APLET/ARMToolbox39 boundary.
- [x] exercise the stock serial-disk transfer of `HP39DIR.CUR`, `HP39DIR.000`,
  `HPGCC000.000` and `LIB275L.000` through the original ROM;
- [x] drive stock APLET -> HPCLANG -> ARMToolbox39 -> HPC1 PASS -> stock return
  on HP39gs and HP40gs;
- [ ] close HP39g+ sibling model/outer-launch validation.

The preferred fixture path uses the stock ROM's own transfer/install logic.
Direct SRAM/flash patching is a fallback only. The obsolete Conn3x GUI itself
is not part of the runtime regression contract.

## M4 — HP48gII

**Complete.**

- [x] add a model-4 / 131x64 E2E target profile;
- [x] record the true medium-apple hardware differences: 256 KiB physical RAM,
  64-line LCD and HP48gII firmware identity;
- [x] add a thin x50ng HP48gII preparation mode;
- [x] boot a genuine HP48gII full-NOR image on the 256 KiB medium-apple model;
- [x] execute the same HPC1 runtime and validate clean return;
- [x] exercise the supported ARMToolbox48 calculator-side launch path.

Completion criterion:

> HP48gII executes the same HPC1 runtime image through the common bootstrap and
> returns to a responsive stock environment without a distinct application ABI.

## M5 — HP49g+/50g family

**Complete.**

- [x] HP50g direct runtime E2E PASS + clean stock return.
- [x] HP50g original-system ARMToolbox install -> HPCLANG.HP -> PrRUN -> PASS.
- [x] add HP49g+ model 5 to the shared hp4950 Direct runtime matrix.
- [x] HP49g+ direct runtime E2E PASS + clean stock return;
- [x] close the supported HP49g+ stock package/ARMToolbox path.

49g+ and 50g share the hp4950 hardware backend, the official 2.15 OS image,
the stock File Manager/ARMToolbox sequences and the HPC1 ABI. Their distinct
ARM bootloaders are target metadata rather than a runtime fork.

Completion criterion:

> HP49g+ and HP50g use the same family backend and HPC1 ABI, with only genuine
> model differences below the common bootstrap.

## M6 — Automated emulator E2E

**Representative gates complete; family expansion active.**

Passing:

- HP39gs direct runtime;
- HP40gs direct runtime;
- HP48gII direct runtime on a genuine full-NOR/medium-apple model;
- HP49g+ direct runtime with its native ARM bootloader;
- HP50g direct runtime with its native ARM bootloader;
- HP49g+ full stock ARMToolbox/PrRUN launch;
- HP50g full stock ARMToolbox/PrRUN launch;
- HP48gII full stock Kermit/ARMToolbox48/PrRUN launch;
- HP39gs full stock serial-disk/APLET launch;
- HP40gs full stock serial-disk/APLET launch.

Active expansion:

- [x] HP48gII direct runtime;
- [x] HP39gs/HP40gs stock serial-disk/APLET launch;
- [x] HP48gII stock launch;
- [x] HP49g+ stock launch;
- [ ] HP39g+ sibling validation.

Direct runtime owns common-bootstrap execution and return correctness.
Stock-launch gates own calculator-side package/menu/launcher integration.
Host transfer tools are included only when they can be exercised by a
maintained deterministic transport.

## M7 — Common platform services

**Planned after M3/M4/M5 closure.**

Add display, keyboard, timers, storage and calculator-object/stack services
behind common APIs. Add only capabilities required by real applications.

## M8 — SDK stabilization

**Planned.**

Completion requires all three platform families to execute HPC1 through a
supported path, representative emulator coverage, primary real-hardware
coverage, coherent public headers/build UX and a fresh LLVM-only checkout.
