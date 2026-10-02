# E2E internals

The test tree has two layers. `tests/unit/` contains host-side `test_*.c`
tests for pure implementation logic. `tests/e2e/` contains calculator-side
HPC1 applications plus the harness data used to launch them through different
targets.

The canonical runtime application and focused visual/sample applications are
separate executables. In particular, Font Gallery is a **separate E2E
application**; it must not grow as extra screens inside the canonical runtime
test merely to reuse its launch harness.

The emulator workflows are split by what they prove, not by which historical
emulator happens to execute them.

## Test tiers

**Direct runtime** boots a target-appropriate stock firmware or full-NOR fixture, bypasses calculator menus/installers, enters the common hpclang ARM boundary, runs the canonical runtime HPC1 application, and then restores the stock execution state.

**Stock launch** drives the calculator's original UI and supported launcher path, then requires the same machine-readable E2E PASS result from that same runtime application.

The two tiers are deliberately separate. Direct runtime owns common-bootstrap execution and restoration correctness. Stock launch owns calculator-side package, transfer, menu, and launcher integration. A Stock job may additionally probe the post-return UI, but that visible-return evidence is not interchangeable with reaching HPC1 PASS.

## Calculator-object and stack-mutation witness

The canonical runtime payload covers the closed M7.3a-e read/stack/adapter baseline. M7.3f named-store write/delete is in separate WIP development on `m7-object-parity`; it is not integrated into the canonical USB baseline and has no accepted write-round-trip E2E claim there.

- Direct/non-Stock environments and HP39-family targets reject the Stock-only RPL stack mutation surface cleanly with `ENOSYS`.
- Stock HP48gII and HP49g+/HP50g read `DSKTOP`/D1 and `EDITLINE`, derive user-visible depth after launcher-bias normalization, and snapshot only objects whose native extent is provable.
- The mutation witness is self-contained inside HPC1: it creates owned CHAR `A`/`B` snapshots, pushes A, replaces level 1 with B, peeks/validates B, pushes A again, pops/validates A, drops the remaining inserted object, and requires the final stack depth to equal the initial depth.
- Pop snapshots before it mutates D1. Replace allocates/copies the new object before committing the target pointer. Expected capacity/level failures are planned before the first mutation.
- No raw Saturn address or mapped ARM pointer is exposed to the E2E application.

Historical read-only evidence remains **Core #285 / Runtime #232**. Launcher-bias normalization is independently closed by fully-green **Runtime #237**. The stack-mutation focused evidence is **Core #296 (`35439583956`)** plus the successful canonical-build, Direct hp39/hp48/hp4950, Stock hp48 and Stock hp4950 jobs of **Runtime #241 (`35439583995`)**. M7.3d/e then add named-store reads, HP39 descriptor-driven Variable recall, and the bounded HPGCC adapters. The final read/adapter closure is **Runtime #338 (`35812005032`)**, fully green across Build, all Direct families, Stock hp39, Stock hp48, Stock hp4950, and evidence publication.

## Return evidence

Return validation has three distinct observations:

1. **payload PASS** — the guest reaches machine-readable `0x50415353`;
2. **control-flow restoration** — stage0 leaves HPC1 and the stock address space / saved machine state becomes active again;
3. **visible Stock-UI restoration** — after a harmless native UI action, the LCD is visibly redrawn by the stock OS rather than merely retaining the last hpclang framebuffer.

A nonblank stale hpclang frame proves neither item 2 nor item 3. The harness
must not copy the pre-launch framebuffer back into place and call that a return.
For HP39 hardware validation the established post-return action is **APLET**;
a valid return opens/redraws the normal APLET library. Other families should
use an equally harmless model-appropriate Stock-UI action.

## Stock transfer mechanisms

HP 39g+, HP 39gs, and HP 40gs use the original USB Connectivity Kit path by default. A headless Kermit server supplies the typed files:

- `HP39DIR.CUR`;
- `HP39DIR.000`;
- `HPGCC000.000`;
- `LIB275L.000`.

The stock ROM performs the transfer/install itself.

HP 48gII uses the Stock Kermit server over USB to receive `L275_48.LIB`, generated `HPCLANG.HP`, and any focused witness object such as `ZZE2E`, then installs library 275 into Port 0 and launches the payload through ARMToolbox48 / `PrRUN`. UART remains available as an override. The long-standing transfer race is synchronized by an explicit host-completion gate: calculator UI automation does not resume until the Kermit sender has exited successfully. The computed transfer timeout is only a watchdog ceiling, not a guessed delay.

HP 49g+ and HP 50g transfer SETUP, generated `HPCLANG.HP`, and `ZZE2E` over USB Kermit, then install ARMToolbox into Port 2. They share the same Stock UI sequence while retaining their native per-model ARM bootloaders. An explicit SD override retains the emulated-card path used by Storage. Current migration evidence is tracked in [Stock USB transport](stock-usb-transport.md).

The historical pre-USB cleanup baseline is retained for regression provenance. Canonical **Runtime #357 (`35832611353`)** is fully green on the final cleanup functional head, including HP48gII after the Kermit host-gate and bounded `ZZE2E` object witness. **Font Gallery #130 (`35832611436`) is also fully green** across Direct hp39/hp48/hp4950 and Stock hp39gs/hp40gs/hp48gii/hp50g. **Storage #141 (`35832611387`)** and **Quality #13 (`35832611366`)** are green on the same head. The earlier HP39 Gallery red path had two independent harness regressions: launch/UI settling required payload-aware launch-only retries, and the Gallery application retained an obsolete direct `platform_cookie == code-window` availability check after runtime moved HP39 private state behind an opaque tagged platform context. The runtime font service now owns that private mapping decision and the independent ROM pixel oracle validates the rendered result.

## Target profiles

`tests/e2e/targets/*.json` contains emulator/model metadata shared by workflows:

- `target` — workflow and filename identity;
- `family` — `hp39`, `hp48`, or `hp4950`;
- `model` — stable hpclang model ID;
- `lcd_height` — 64- or 80-line capture height;
- `automated_e2e` — optional Direct/focused automation eligibility; it defaults to true;
- `automated_stock_e2e` — optional original-Stock automation eligibility; it defaults to true and may be false when the stock host transport is not modeled;
- `stock_transport` — `usb`, `uart`, or `sd`; all six current profiles select USB, with `HPCLANG_E2E_STOCK_TRANSPORT_OVERRIDE` available for supported secondary paths;
- `nor` — optional checked full-NOR fixture metadata: repository path, compression, decoded byte count, and SHA-256.

Only targets distributed as per-model firmware archives add a `firmware` object. HP 39gs and HP 40gs use their hpcalc archive/member names.

HP 39g+ uses the verified real-hardware 1 MiB D1.55 full-NOR fixture under `tests/e2e/fixtures/hp39gplus/`; its profile records the decoded size and SHA-256 and is enabled in the automated family matrices.

HP 49g+ and HP 50g share the 2.15 OS image but carry distinct `bootloader` filenames because the bootloader difference is genuine per-model metadata.

HP 48gII also uses a pinned full-NOR fixture rather than an update archive.

Profiles do not carry empty placeholder fields.

`tools/e2e-target-env.py` exposes a selected profile as `HPCLANG_E2E_*` environment variables for the workflows.

## Sequence files

Human-like stock-UI key input belongs under `tests/e2e/sequences/`. Sequence
filenames describe the target and lifecycle step (for example
`hp39gs-stock-run.seq`), not the historical experiment that first established
the path. Offsets are nominal milliseconds from sequence start:

```text
9200 capture files
9500 tap up
9800 tap f6
12000 chord rshift two
```

The stock runner accepts logical key actions rather than GUI coordinates.

Events execute in offset order. The gap between adjacent offsets is a minimum
delay from actual delivery, so a host scheduling pause cannot collapse a key
press and release into one guest-invisible callback batch. `host-gate` pauses
the sequence until the transfer-completion marker exists; subsequent offsets
start at zero relative to gate release. `monitor` first waits up to two seconds
for a nonblank pre-launch LCD frame, then arms the payload deadline before
allowing launch keys to proceed. Blank screens still fail when that bound expires.

The current model-specific sequences exist because the stock user interfaces are genuinely different. They should not be unified by adding conditional logic to one large script unless that makes the behavior easier to understand.

## Workflow path selection

Push filters compare the branch's previous head with the new head. Merging main
into an older work branch therefore includes the incoming main changes, even
when those changes were already tested on main. Gallery runs for font changes
and its shared bootstrap, launcher, compiler setup and emulator dependencies;
an isolated object-store implementation change does not select Gallery.

## x50ng backend

CI checks out a pinned external x50ng revision. `tools/prepare-x50ng.py` installs
the Direct, Stock, and Gallery frontends from `tests/e2e/harness/x50ng/` and
applies the shared flash, SRAM, reserved-RAM, USB, and UART adaptations.
Gallery mapping sizes live in its C frontend; preparation does not rewrite
frontend constants or add firmware-specific diagnostic probes.

Tests read the emulated LCD controller and drive the calculator key matrix directly. No GTK/SDL window automation, Xvfb, mouse coordinates, or host-screen scraping is used at runtime.

Fresh-state missing register keys/groups use reset defaults silently. Pass
x50ng `--verbose` to inspect those optional state lookups. Other state lookup
errors still print normally; this does not filter or discard emulator stderr.

x50ng remains an external emulator/test dependency. None of its GPL implementation is copied into hpclang's production runtime.

Current roles:

- Direct runtime on HP 39g+, HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g;
- Stock USB typed-file/APLET launch for HP 39g+/39gs/40gs;
- Stock USB Kermit/ARMToolbox48 launch for HP 48gII;
- Stock USB Kermit/ARMToolbox launch for HP 49g+/50g;
- explicit UART and SD/VVFAT overrides for secondary regressions; Storage uses SD.

The x50ng preparation helper accepts the same three platform names as hpclang: `hp39`, `hp48`, and `hp4950`. These names describe hpclang's platform grouping; they are not historical HP codenames.

## HP 39g+ E2E

HP 39g+ now participates in the automated `hp39` matrices using a complete
1 MiB NOR image acquired from real hardware. The fixture is installed directly
as x50ng persistent flash state and is verified against the profile's byte count
and SHA-256 before each run. No sibling firmware archive or foreign bootloader is
substituted.

HP39g+ D1.55 uses the USB Connectivity Kit `Disk drive...` path. The common
USB device/host bridge now supports the HP39 typed-file Kermit adapter and
the profile enables `automated_stock_e2e`. The canonical USB Runtime gate establishes Stock
PASS on HP39g+, HP39gs, and HP40gs. The Conn3x GUI is not a CI dependency.

The existence of a trustworthy full-NOR fixture closes firmware identity and
boot/runtime validation. It does not automatically prove that HP39g+ shares the
HP39gs/40gs physical ROM-font offsets. The native gs/40gs main/mini/EditLine
font mapping remains excluded for model 1 until separately established.

## CI build stages

E2E applications are target-independent. CI should therefore avoid rebuilding a selected application once per calculator.

For the canonical runtime application, the implemented pipeline has one shared build plus two execution tiers and one shared-asset stage for Stock:

1. **application build** — build one canonical runtime HPC1 application and all target launchers;
2. **Direct family execution** — run `hp39`, `hp48`, and `hp4950` against that same artifact;
3. **Stock family execution** — drive the original package/UI/launcher paths against that same artifact; Stock transfer assets are prepared once and shared.

Direct and Stock therefore do **not** have separately compiled runtime payloads.
A focused application such as Font Gallery gets its own build artifact,
specification and evidence path while reusing the common target/harness
infrastructure.

The same rule applies to the focused **Storage** gate. `E2E / Storage` builds the storage-enabled payload separately from the canonical keyboard/runtime payload so storage timing and fixtures do not perturb the canonical Runtime gate. Its hp4950 Stock path creates the named-directory VVFAT fixture after ARMToolbox installation but before the run-phase emulator opens the card, arms result monitoring before `PrRUN`, and requires payload PASS, ARMToolbox stage ownership release, and settled Stock repaint on both HP49g+ and HP50g.

Storage #22 (`35378151818`) is the M7.2b historical closure evidence for the earlier KOS-backed stream/metadata bridge. Storage #23 then independently proved that Stock KOS syscall 19 can safely rebuild the calculator filesystem after direct ownership is released.

The integrated M7.2 implementation goes further: the first byte-stream operation acquires the process-wide SD/FAT lease, and all stdio, metadata, capacity, namespace mutation, and owned directory iteration use hpclang's S3C2410 SDI + FatFs backend until runtime shutdown. The focused payload verifies missing-file semantics, create/truncate/append, seek/tell, metadata and FAT timestamps, capacity/free space, rename/mkdir/rmdir, UTF-8 long filenames, case-insensitive lookup with preserved stored case, simultaneous independent iterators, ordinary stdio while an iterator is live, and final reopen/read before the runtime unmounts/releases the controller and invokes Stock ResetSD.

The M7.3f follow-up Storage run
[36509415707](https://github.com/zyf722/hpclang/actions/runs/36509415707)
passed the payload but crashed on HP49g+/HP50g during shutdown: Stock `ResetSD`
reused the launcher's live `L1[0x90]` mapping. The private KOS bridge now preserves
that descriptor across the call and publishes its restoration before returning
to stage1. KOS returns through the caller LR, so the bridge supplies an explicit
continuation before restoring the original ARM/Thumb caller. `ResetSD`, strict
return checks and the Stock UI witness remain required. Local Stock and Direct
hp4950 verification passes with this repair; formal CI acceptance is pending. Storage
path filters include `runtime/libc/kos-call.S` so future bridge changes exercise
this boundary.

Final M7.2 closure evidence is **E2E / Storage #53 (`35428824540`)** on `c4b827fe`. All jobs pass: Build, Direct hp39, Direct hp48, Direct hp4950, and Stock hp4950. The final payload additionally runs the bounded HPGCC `hpstdio.h`/`fsystem.h` adapter flow and validates HP-style paths, adapter-local cwd, LFN directory enumeration, volume size/free queries, and POSIX `ftruncate()` extension/shrink semantics including zero-filled extension.

HP 39g+ participates in Direct, focused, and original-Stock family matrices through its data-driven `nor` profile. All six profiles currently select `stock_transport: usb`. See [Stock USB transport](stock-usb-transport.md) for migration evidence and the distinction between implemented defaults and repeated-run stability.

GitHub Actions YAML is orchestration, not the test specification. Expected files, required captures, protocol events, and similar assertions belong under `tests/e2e/specs/` and are evaluated by the E2E harness. Fixture integrity checks such as a pinned firmware SHA-256 belong to fixture preparation, not to the behavioral test specification.

## Full-keyboard validation and progression evidence

The canonical runtime E2E injects every physical key position on each automated calculator model. The guest verifies DOWN/UP events, raw scan code, physical key identity, and dedicated-key modifier state for all 51 keys.

The final LCD screen renders the physical keyboard rather than the electrical scan matrix: six menu keys, two three-key control rows beside the cursor cluster, and seven five-key main rows. Passed positions are filled, and a failing position is left open and marked. The screen labels use the public font/text API, so calculator E2E exercises the same rasterization path instead of carrying a test-only glyph table. The machine-readable result remains the actual gate; the LCD diagram is a visual summary of the same checks.

Direct and Stock both capture an initial hollow keyboard plus one frame after every accepted key release: **52 progression frames** for the 51-key test. Stock frames are phase-gated by `KACK(n)`: a new frame is accepted only after the preceding DOWN→UP cycle has returned, the payload has updated its passed-bit state, and the display has been redrawn. Runtime #135 (`35298949365`) is the current canonical fully-green witness; #122 remains the first run that closed the same 52-frame contract. The merged progression contact sheet remains supplementary evidence beside the canonical `before / pass / return` summary.

## Font Gallery application

Font Gallery is a focused visual/sample E2E application, not part of the canonical runtime test. The public font core has no semantic theme-role layer: Gallery enumerates the concrete ROM fonts hpclang actually exposes, followed by generated font products. HP39gs/40gs expose minifont, main/default, and EditLine; HP48gII/49g+/50g expose minifont and bigfont/default.

The exhaustive Direct Gallery is family-dependent: HP39gs/40gs traverse **248 entries / 496 captures** (245 generated + three ROM fonts), while HP48gII/49g+/50g traverse **247 / 494** (245 generated + two ROM fonts). The production matrix still runs one family job for `hp39`, `hp48`, and `hp4950`, covering both 64- and 80-line display layouts. It validates generated aliases/arbitrary face selection, metrics/spacing, repertoire-sensitive specimens, deterministic pagination, PASS and settled Direct return.

Native-font evidence is intentionally family-aware. The synthetic Direct launcher does not fabricate a live Saturn `Read[]` map, so exhaustive generated-font rendering is authoritative on all six targets, while HP48/49/50 native-font pixels require a real Stock logical map. Compact Stock Gallery controls close both automated live-native families: HP50g validates the hp4950 path and HP48gII independently validates the 64-line hp48 path through the original UI/ARMToolbox launch.

The compact Stock navigation remains family-aware. HP39gs/40gs use **DOWN** across minifont → main/default → EditLine, then **RIGHT** to the generated control. HP48gII/HP50g use **DOWN** across minifont → bigfont/default, then **RIGHT** to the generated control. The #112 artifacts visibly retain those distinct concrete ROM-font pages.

HP39gs/40gs are different: their fixed native raster tables are physical stock-ROM assets. Production bootstrap maps only the two required physical NOR pages (`0x0007e000` and `0x0007f000`) as uncached small pages in the private prefix of hpclang's own code L2 table. This design was chosen after diagnostics proved that a long-lived high-L1 NOR section alias could have its descriptor word overwritten by the active LCD framebuffer itself. Font Gallery therefore also serves as the regression gate for this bootstrap-private mapping.

Gallery #99 was the first fully-green production closure across Build, universal x50ng, all three Direct families, and the four Stock Gallery targets; #102 later exercised the strict HP48 settled-return gate. The final M7.1 head is now covered by **35298949457 / #112**, which repeats the full matrix after the HP39 EditLine/native-font census correction and documentation sync.

Keeping Gallery separate prevents font exploration from inflating the runtime ABI/keyboard/storage test and lets its visual evidence evolve independently of canonical runtime PASS evidence.

### Current bitmap-only font closure status (2026-09-14)

Canonical bundled assets are bitmap-source only. The generated catalog is **245 faces / 1,639,485 mapped glyphs / 3,472 assembly sources** with **245/245** source-to-generated parity. Font Assets **#24** regenerates the catalog without drift and reports no exact duplicates, overflow, glyph-collapse, blank-heavy, or Powerline regressions.

The general TTF importer has been removed. Final bitmap curation adds Cubic 11, ChillBitmap 7px, and QuanPixel/May7 with provenance retained in the source package. Gallery labels/specimens were strengthened for Ark Pixel regional variants, width-sensitive Latin, digits/punctuation, accents/symbols, and supported CJK/Japanese/Korean repertoires.

`E2E / Font Gallery` **#130 (`35832611436`)** is a historical fully-green regression gate. Current integration acceptance is tracked in [Validation](validation.md). HP39gs/40gs Direct traverse **248 entries / 496 captures** including EditLine; HP48gII/49g+/HP50g remain **247 / 494**. Stock hp39gs/hp40gs pass mini/main/EditLine/generated-control plus ROM fingerprint/pixel oracles, and HP48gII/HP50g retain settled live-native evidence. Historical #112 remains the original final M7.1 native-census closure baseline.

The font implementation/evidence slice is therefore closed. Remaining work is repository integration and bounded Stock-harness closeout, not additional font-core or Gallery implementation.
