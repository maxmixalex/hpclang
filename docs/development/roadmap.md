# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/zyf722/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Principles

- one application ABI: HPC1 / hpclang v1;
- one common ARMv4T runtime;
- thin platform implementations: `hp39`, `hp48`, and `hp4950`;
- no required GCC, historical HPGCC runtime, or ROM replacement;
- historical code is research evidence, not a compatibility mode;
- emulator fixtures are generated or prepared from verified inputs rather than committed as opaque calculator-state snapshots;
- model-specific differences remain metadata or platform code unless they genuinely require a different implementation.

## M0 — Unified application ABI

**Complete.**

HPC1, AAPCS32, Thumb-1/ARMv4T, soft float, and the fixed virtual layout are the single supported application contract.

Existing model IDs 1–6 are stable ABI values.

## M1 — Modern language/runtime stack

**Complete.**

LLVM compiler builtins, ARMv4T-safe integer and soft-float helpers, picolibc integration, malloc, static TLS, constructors, and normal C/C++ linkage are in the common runtime.

Historical GCC code generation, libgcc, r9/r10 static-base conventions, and legacy HPGCC runtime objects are not normal-build dependencies.

## M2 — Common ARM bootstrap

**Complete for the validated platforms.**

The common bootstrap installs the HPC1 mappings, enters the application, restores temporary state, and returns to the calculator OS.

HP 39gs real hardware plus the current emulator gates validate the design. HP 39g+, HP 39gs, and HP 40gs share the `hp39` platform; HP 48gII uses `hp48`; HP 49g+ and HP 50g share `hp4950`.

## M3 — HP39 platform

**Runtime and automated stock-launch validation complete for HP 39gs/40gs. HP 39g+ blocked on real hardware fixture.**

- [x] HP 39gs real-hardware canonical runtime PASS and clean return.
- [x] HP 39gs official-ROM Direct E2E.
- [x] HP 40gs official-ROM Direct E2E.
- [x] HP 39gs/40gs hardware-validation packages.
- [x] HP 39g+ model-1 hardware-validation package using the shared HPGCC39 APLET/ARMToolbox39 boundary.
- [x] Exercise stock serial-disk transfer of `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000` through the original HP39gs/40gs ROM.
- [x] Drive stock APLET → HPCLANG → ARMToolbox39 → HPC1 PASS → stock return on HP 39gs and HP 40gs.
- [x] Validate a read-only HP39 flash-dump mechanism byte-for-byte against emulated physical NOR.
- [ ] Acquire a real HP 39g+ and dump its complete stock flash.
- [ ] Add the verified HP 39g+ flash fixture to Direct runtime E2E.
- [ ] Automate the original-system HP 39g+ install/launch path available on real stock firmware.

The HP 39g+ items are **blocked**, not active research tasks. Do not substitute a sibling or foreign bootloader and call it native HP39g+ validation.

The preferred fixture path uses the stock ROM's own transfer/install logic. Direct SRAM/flash patching is a fallback only. The obsolete Conn3x GUI itself is not part of the runtime regression contract.

## M4 — HP48 platform / HP 48gII

**Complete.**

- [x] Add model 4 / 131×64 E2E target profile.
- [x] Record the genuine HP48gII hardware differences: 256 KiB physical RAM, 64-line LCD, and HP48gII firmware identity.
- [x] Add the x50ng HP48 preparation mode.
- [x] Establish and pin a genuine 2 MiB full-NOR fixture.
- [x] Boot the full-NOR image with 256 KiB RAM and 131×64 LCD.
- [x] Execute the same HPC1 runtime and validate clean return.
- [x] Exercise the supported ARMToolbox48 calculator-side launch path through stock Serial Kermit.

Completion criterion:

> HP48gII executes the same HPC1 runtime image through the common bootstrap and returns to a responsive stock environment without a distinct application ABI.

## M5 — hp4950 platform / HP 49g+ and HP 50g

**Complete.**

- [x] HP 50g Direct runtime E2E PASS and clean stock return.
- [x] HP 50g original-system ARMToolbox install → `HPCLANG.HP` → `PrRUN` → PASS.
- [x] Add HP 49g+ model 5 to the shared hp4950 Direct runtime matrix.
- [x] Use `boot-49g+.bin` for HP 49g+ and `boot-50g.bin` for HP 50g.
- [x] HP 49g+ Direct runtime E2E PASS and clean stock return.
- [x] HP 49g+ original-system stock package/ARMToolbox path PASS.
- [x] Keep one stock File Manager/ARMToolbox sequence for both models.

HP 49g+ and HP 50g share the `hp4950` platform, official 2.15 OS image, stock File Manager/ARMToolbox sequence, and HPC1 ABI. Their distinct ARM bootloaders are target metadata rather than a runtime fork.

Completion criterion:

> HP49g+ and HP50g use the same platform implementation and HPC1 ABI, with only genuine model differences below the common bootstrap.

## M6 — Automated emulator E2E

**Complete for five models; HP 39g+ blocked.**

Passing:

- HP 39gs Direct runtime;
- HP 40gs Direct runtime;
- HP 48gII Direct runtime on the verified full-NOR fixture;
- HP 49g+ Direct runtime with its native ARM bootloader;
- HP 50g Direct runtime with its native ARM bootloader;
- HP 39gs full stock serial-disk/APLET launch;
- HP 40gs full stock serial-disk/APLET launch;
- HP 48gII full stock Kermit/ARMToolbox48/`PrRUN` launch;
- HP 49g+ full stock ARMToolbox/`PrRUN` launch;
- HP 50g full stock ARMToolbox/`PrRUN` launch.

Blocked:

- [ ] HP 39g+ Direct runtime on a verified native stock flash fixture.
- [ ] HP 39g+ original-system launch on that fixture.

Direct runtime owns common-bootstrap execution and return correctness. Stock-launch gates own calculator-side package/menu/launcher integration. Host transfer tools are included only when they can be exercised through a maintained deterministic transport.

## M7 — Common platform services

**Planned.**

Add services behind common APIs only when required by real applications:

- display;
- keyboard;
- timers;
- storage;
- calculator object/stack integration.

The service layer must not recreate the historical HPGCC API by default. Each API should have a clear cross-platform purpose and defined behavior on all supported platform families.

## M8 — SDK stabilization

**Planned.**

Completion requires:

- all three platform families to execute HPC1 through a supported path;
- representative emulator coverage plus primary real-hardware coverage;
- HP 39g+ either validated or explicitly documented as the remaining external hardware blocker;
- coherent public headers and build UX;
- examples that build from a fresh LLVM-only checkout;
- application-facing documentation separated from this internal Development reference;
- a stable packaging story for the supported calculator profiles.

C++ exceptions/RTTI, broader POSIX/libc hooks, and additional compatibility APIs remain optional. Add them only when a real application requires them.
