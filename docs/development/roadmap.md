# hpclang roadmap

Current checkpoint: 2026-10-26. **M8 is in progress; only M8.1 is accepted complete.**
Accepted main: `ebfb2757` (SD Reset fix #15 accepted after all six workflows passed). Local passing tests, submitted CI, CI acceptance and
milestone completion are separate states. The [development board](https://github.com/maxmixalex/hpclang/issues/1)
tracks current tasks; its compact body now matches independent readback.

## Scope and retained requirements

Reach practical HPGCC application capability parity, then exceed it with one
LLVM-native SDK/ABI, owned services, modern C++, declarative C UI and cross-family
validation. M0–M7 provide the accepted foundation; they do not complete M8.

The previous roadmap from both delivery branches is preserved in the
[frozen requirements/evidence archive](roadmap-history.md). **Every original
completion condition remains in force.** This page shortens status reporting;
it does not remove requirements, reopen accepted M7 scope or replace full
acceptance with focused experiments.

## M8 — HPGCC source compatibility, SDK UX, and post-HPGCC extensions

| Stage | Current state | Remaining completion gate |
| --- | --- | --- |
| M8.0 Native Port 0/1/2 | Implemented in part; active mainline | Complete safe reference handling and movement, public library management and Port 1 edits; Port2 trusted recovery installation, authority lifecycle, safe startup and reclamation |
| M8.1 Installable SDK | **Accepted complete** | Preserve installed external C/C++ builds, relocatability, packaging and reproducibility |
| M8.2 Declarative C UI core | Implemented substantially; stage open | Complete whole-application resource/lifecycle acceptance and pending CI; original design reviewed, six host UI regressions pass locally |
| M8.3 Components/renderers | Implemented substantially; stage open | Complete reusable components, renderer/theme independence and six-model interaction/resource acceptance |
| M8.4 HPGCC source adapters | Several batches accepted, including SD Reset #15; stage open | Complete retained historical application/API coverage, documented migrations and pending branch acceptance |
| M8.5 Modern C++/migration | Implemented in part; stage open | Complete ownership/error/lifetime contracts, migration tooling/guidance and public downstream examples |
| M8.6 Applications/release | Stage open | Nontrivial integrated apps, clean SDK use, cross-family resource/return checks, documentation and release acceptance |

Detailed stage contracts are preserved under [M8 stages](roadmap-history.md#m8-stages).
UI must retain the user's declarative C design: nested ordinary C declarations,
stable keyed identity, controlled inputs, explicit asynchronous state and
revision-driven canvas. A superficially similar imperative API is not completion.

## Current work: M8.0

Port reads, several CRUD operations, Port2 append/replacement/logical deletion and
selected recovery/retirement flows already have evidence. Those do not complete
native port services. The active dependency is execution-reference coverage for
safe movement and library registration; sound compatibility is not its prerequisite.

Current implementation and evidence:

- Register repair/assignment primitives and an optional original-ROM GETPTR
  return gate are submitted in [draft PR #24](https://github.com/maxmixalex/hpclang/pull/24)
  together with internal catalog insertion and ACCESSRAM return coverage at
  `ed56948c`, based on #23. Exact-head CI is terminal: five workflows pass,
  Core and one Runtime workflow fail. A local fixture adjustment frees 800 bytes
  while retaining both measured low-memory bounds; native rerun is pending.
  Two HP39 transfers stall before execution.
  Draft remains open; Port1 follow-up `ca17a98e` is local and unpushed.
- Execution-reference coverage remains partial. Submitted checks cover selected
  register transfers, ACCESSRAM, SAT2ARM and caller alignment. Submitted `a47ee507`
  adds distinct stack sentinels: 1,344 continuations pass, and safe alignment
  entry requires independently known live depth 1–6; cursor alone is insufficient.
  Submitted `ed56948c` joins the production return-root collector and C catalog mover
  to original post-ARMSYS RTNCC, the condition, full GETPTR and ROM dispatch.
  Both models pass 64 baseline/move cases, 80 omitted-repair controls and ten
  changed-input rejections. Seven other stack slots survive; this return path
  consumes one caller slot with no extra stack capacity or R0–R4 accesses.
  Earlier ARMJUMP reachability, arbitrary caller state, later Stock execution,
  opaque/banked references and complete launcher closure remain open.
- Internal `hpclang_rpl_catalog_insert_repaired` validates insertion geometry,
  grows with guarded repairs, and publishes the count last. Host sanitizers,
  ARM build/analysis and all ten HP49g+/HP50g controlled Stock cases pass;
  complete bindings, return markers, unchanged Flash and 1,890 corruption
  controls are checked. The delivery load image matches the tested image.
  Public registration still requires complete reference coverage.
- Submitted Port1 edit handling distinguishes preflight IO from INCOMPLETE after
  commit starts. Local `ca17a98e` also distinguishes hidden append-body failure
  from partial prologue publication. All 109 append write-failure boundaries
  and both success cases pass sanitizers; ARM build/analysis pass. The native
  stable-memory backend must not fail after preflight. Public Port1 edit wiring
  still needs complete banked-reference protection; this fix does not enable it.

Next actions, in order:

1. Join actual launcher/opaque-object provenance with complete relocation plans;
   close remaining bank and live-register obligations before public registration.
2. Repair and rerun #24 low-memory fixture and HP39 transfer failures before
   acceptance. Local emulator validation currently awaits Docker engine recovery
   (idle-pause timeout, then internal engine route unavailable). #17 previous
   head has all nine workflows passing; its resolved main integration awaits CI;
   #20 Runtime fails Direct system-services-exit/hp4950 and remains unaccepted.
   #18 has a confirmed HP49g+ Direct PWM IRQ failure (`0x46417b03`), not the
   separately numbered watchdog check; reproduce with IRQ diagnostics before fixing.
3. Complete the retained Port 1/library mutation and Port2 recovery, authority,
   startup and reclamation gates. Preserve all independent M8 stage work.

## Delivery and evidence policy

Before each push, inspect the preceding push's exact-head CI and investigate
failures. Do not poll newly pushed CI in the same turn or merge incomplete gates.
Private saved-state/ROM interpreter checks supplement full Stock tests; they do
not establish hardware behavior or complete milestone acceptance.

Keep this page concise. Put reproducible checks in tests and link their reports
from the relevant PR. Keep historical evidence in the frozen archive; do not
append per-turn experiment narration to the roadmap or board. Record only
current decisions, meaningful state changes, blockers and next acceptance gates.

## Deferred research — HP39gs / HP40gs ROM storage

Retain investigation of ROM-write capability inspired by
[HP39GS-Explorer](https://github.com/Yanye0xFF/HP39GS-Explorer), with user-provided
ROM dumps as research input. HP39g+ is excluded by its hardware limitation.
This is a deferred idea, not authorization to flash hardware now.

## Preserved approval boundaries

Port2 compromises may affect sequencing, but automatic recovery/reclamation stay
inside M8. The chosen HPGCC numeric-format behavior is explicit opt-in through
`HPCLANG_HPGCC_LEGACY_FORMAT=1`; standard C behavior remains the default.
Any actual scope reduction requires the user's agreement. No repeated approval
is needed for an already authorized implementation or a proven status update.
