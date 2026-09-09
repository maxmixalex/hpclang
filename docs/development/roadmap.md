# hpclang roadmap

Live short-term work is tracked in [issue #1](https://github.com/zyf722/hpclang/issues/1). This page records durable milestones, completed evidence, and completion criteria.

## Scope of the completed milestones

M0–M6 describe the **execution foundation**: compiler/runtime viability, the
HPC1 ABI, calculator launch paths, stock-system return, packaging, fixtures,
and automated E2E.

They do **not** mean that hpclang already reproduces the application-facing
feature set of HPGCC 1.x, HPGCC 2.x, or HPGCC3. At the current stage the public
hpclang headers expose the execution ABI, bootstrap contract, and model/family
identity. Common calculator services such as graphics, keyboard input, timers,
files/storage, Saturn/RPL object exchange, and higher-level UI facilities are
still future SDK work.

hpclang is also not intended to be binary-compatible with historical HPGCC
executables or to reproduce HPGCC3's custom-ROM/ROMLIB environment.

## Principles

- one application ABI: HPC1 / hpclang v1;
- one common ARMv4T runtime;
- thin platform implementations: `hp39`, `hp48`, and `hp4950`;
- no required GCC, historical HPGCC runtime, or ROM replacement;
- historical code is research evidence, not a compatibility mode;
- emulator fixtures are generated or prepared from verified inputs rather than committed as opaque calculator-state snapshots;
- model-specific differences remain metadata or platform code unless they genuinely require a different implementation;
- stock-hosted backends preserve host hardware ownership where practical; a
  future hpclang-owned bootloader/OS may own IRQs, timers, clocks and peripherals
  without changing the application-facing service APIs.

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

## M7 — Application-facing calculator services

**In progress.**

M7 is where hpclang becomes a practical calculator SDK rather than only a
validated native execution environment.

### M7.0 — API inventory and compatibility design

Before freezing the public service API:

- [x] inventory the public HPGCC 1.0.3/1.1 and 2.0 headers by functional area
  from complete distribution archives;
- [x] inventory the relevant HPGCC3 public interfaces separately;
- [x] classify the major historical API families as direct source-compatible,
  compatibility-adapter, migration-only, or intentionally unsupported;
- [ ] finish ownership, error, coordinate, string/buffer, and capability-query
  conventions for the remaining services;
- [x] define the C ABI boundary and C++ wrapper policy;
- [x] choose the first service slice: device/capability facts + monotonic time +
  keyboard + display/surface.

See [SDK API design](sdk-design.md).

M7.1 implementation is active. Device information, stock-preserving
monotonic time, raw keyboard snapshots, debounced/repeating keyboard events,
and the first opaque display-surface primitives pass five-model Direct E2E.
The remaining first-slice closure is the new C++23 wrapper/runtime probe and
then a representative application example rather than more bootstrap work.

The minimum cross-platform service set should cover:

- display and framebuffer/graphics primitives;
- keyboard scanning and key events;
- monotonic time, delays, and timers;
- files and storage where the calculator exposes them;
- calculator/Saturn object exchange;
- RPL stack integration where applicable;
- clean access to required stock-OS services;
- model capability discovery when a service is not available everywhere.

These APIs need hardware/emulator tests and representative real-application
tests. The exact historical HPGCC function names are not the design target:
compatibility wrappers may be added later where they materially ease porting,
but the core API should remain coherent across `hp39`, `hp48`, and
`hp4950`.

### Historical HPGCC parity is not complete

The old HPGCC SDKs included or supported substantial calculator-facing
functionality beyond merely entering C code: screen/graphics, keyboard,
file I/O, Saturn/RPL integration, OS/syscall access, and higher-level helper
libraries. hpclang has not yet reimplemented that breadth.

Therefore M0–M6 completion must not be reported as "HPGCC 1.x/2.x compatible"
or "HPGCC feature complete".

## M8 — Practical SDK and porting layer

**Planned.**

After the common services exist:

- establish stable application-facing headers;
- provide examples that exercise graphics, input, timers, storage, and
  calculator-object integration;
- validate one or more non-trivial applications on each platform family;
- document source-porting guidance from common HPGCC 1.x/2.x APIs where useful;
- decide which compatibility wrappers are worth maintaining;
- establish a stable packaging/build UX from a fresh LLVM-only checkout;
- separate public application documentation from this internal Development
  reference.

HPGCC binary compatibility is not a goal.

HPGCC3 compatibility is also a separate question: HPGCC3 uses a revisioned
library environment tied to a custom calculator ROM. hpclang currently follows
the opposite non-ROM design and does not implement that ROMLIB ABI.

C++ exceptions/RTTI, broader POSIX/libc hooks, advanced graphics/UI libraries,
and additional compatibility APIs remain optional until a real application
requires them.

## M9 — SDK stabilization

**Planned.**

Completion requires:

- all three platform families to execute HPC1 through a supported path;
- representative emulator coverage plus primary real-hardware coverage;
- HP 39g+ either validated or explicitly documented as the remaining external
  hardware blocker;
- the M7 calculator-service baseline to be usable by real applications;
- stable public headers and packaging;
- representative applications passing on all supported platform families;
- coherent application-facing documentation and porting guidance.

Only at this stage should hpclang be described as a broadly usable replacement
SDK rather than a completed execution foundation.
