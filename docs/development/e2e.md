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

HP 39gs and HP 40gs use the original serial-disk path. A headless Kermit server supplies:

- `HP39DIR.CUR`;
- `HP39DIR.000`;
- `HPGCC000.000`;
- `LIB275L.000`.

The stock ROM performs the transfer/install itself.

HP 48gII uses the stock Serial Kermit server to receive `L275_48.LIB` and generated `HPCLANG.HP`, then installs library 275 into Port 0 and launches the payload through ARMToolbox48 / `PrRUN`.

HP 49g+ and HP 50g use an emulated SD card to install ARMToolbox and copy generated `HPCLANG.HP`. They share the same stock UI sequence while retaining their native per-model ARM bootloaders.

## Target profiles

`tests/e2e/targets/*.json` contains emulator/model metadata shared by workflows:

- `target` — workflow and filename identity;
- `family` — `hp39`, `hp48`, or `hp4950`;
- `model` — stable hpclang model ID;
- `lcd_height` — 64- or 80-line capture height;
- `automated_e2e` — optional automation eligibility; it defaults to true.

Only targets distributed as per-model firmware archives add a `firmware` object. HP 39gs and HP 40gs use their hpcalc archive/member names.

HP 39g+ intentionally has no `firmware` object and sets `"automated_e2e": false`. Its model/platform profile remains useful for packaging, but no trustworthy public stock ARM flash fixture has been established.

HP 49g+ and HP 50g share the 2.15 OS image but carry distinct `bootloader` filenames because the bootloader difference is genuine per-model metadata.

HP 48gII uses a pinned full-NOR fixture rather than an update archive.

Profiles do not carry empty placeholder fields.

`tools/e2e-target-env.py` exposes a selected profile as `HPCLANG_E2E_*` environment variables for the workflows.

## Sequence files

Human-like stock-UI key input belongs under `tests/e2e/sequences/`. Sequence
filenames describe the target and lifecycle step (for example
`hp39gs-stock-run.seq`), not the historical experiment that first established
the path. A sequence is an absolute-time script:

```text
9200 capture files
9500 tap up
9800 tap f6
12000 chord rshift two
```

The stock runner accepts logical key actions rather than GUI coordinates.

The current model-specific sequences exist because the stock user interfaces are genuinely different. They should not be unified by adding conditional logic to one large script unless that makes the behavior easier to understand.

## x50ng backend

CI checks out a pinned external x50ng revision and replaces only its UI entry point with a headless test frontend from `tests/e2e/harness/x50ng/`.

Tests read the emulated LCD controller and drive the calculator key matrix directly. No GTK/SDL window automation, Xvfb, mouse coordinates, or host-screen scraping is used at runtime.

x50ng remains an external emulator/test dependency. None of its GPL implementation is copied into hpclang's production runtime.

Current roles:

- Direct runtime on HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g;
- stock serial-disk/APLET launch for HP 39gs/40gs;
- stock Serial Kermit/ARMToolbox48 launch for HP 48gII;
- stock File Manager/ARMToolbox launch for HP 49g+/50g.

The x50ng preparation helper accepts the same three platform names as hpclang: `hp39`, `hp48`, and `hp4950`. These names describe hpclang's platform grouping; they are not historical HP codenames.

## HP 39g+ E2E

HP 39g+ remains outside the automated matrices until a real-machine stock flash dump exists. The absence of that fixture must not be hidden by reusing a sibling or foreign bootloader and calling the result native HP39g+ validation.

The retained HP39g+ NOR-dumper experiment is useful evidence for a transient physical-NOR mapping mechanism only. It is not evidence that HP39g+ shares HP39gs/40gs flash identity or their native-font physical offsets.

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

Storage #22 (`35378151818`) is the current fully-green M7.2b evidence: all three Direct families pass, HP49g+ and HP50g Stock pass, and the payload covers stdio, path metadata, named-directory type, media/free-space snapshot semantics, and the fixed UTC-host-fixture → FAT/KOS last-modified civil-time witness.

HP 39g+ is filtered out by its explicit `automated_e2e: false` profile flag rather than by a hard-coded workflow exclusion.

GitHub Actions YAML is orchestration, not the test specification. Expected files, required captures, protocol events, and similar assertions belong under `tests/e2e/specs/` and are evaluated by the E2E harness. Fixture integrity checks such as a pinned firmware SHA-256 belong to fixture preparation, not to the behavioral test specification.

## Full-keyboard validation and progression evidence

The canonical runtime E2E injects every physical key position on each automated calculator model. The guest verifies DOWN/UP events, raw scan code, physical key identity, and dedicated-key modifier state for all 51 keys.

The final LCD screen renders the physical keyboard rather than the electrical scan matrix: six menu keys, two three-key control rows beside the cursor cluster, and seven five-key main rows. Passed positions are filled, and a failing position is left open and marked. The screen labels use the public font/text API, so calculator E2E exercises the same rasterization path instead of carrying a test-only glyph table. The machine-readable result remains the actual gate; the LCD diagram is a visual summary of the same checks.

Direct and Stock both capture an initial hollow keyboard plus one frame after every accepted key release: **52 progression frames** for the 51-key test. Stock frames are phase-gated by `KACK(n)`: a new frame is accepted only after the preceding DOWN→UP cycle has returned, the payload has updated its passed-bit state, and the display has been redrawn. Runtime #135 (`35298949365`) is the current canonical fully-green witness; #122 remains the first run that closed the same 52-frame contract. The merged progression contact sheet remains supplementary evidence beside the canonical `before / pass / return` summary.

## Font Gallery application

Font Gallery is a focused visual/sample E2E application, not part of the canonical runtime test. The public font core has no semantic theme-role layer: Gallery enumerates the concrete ROM fonts hpclang actually exposes, followed by generated font products. HP39gs/40gs expose minifont, main/default, and EditLine; HP48gII/49g+/50g expose minifont and bigfont/default.

The exhaustive Direct Gallery is family-dependent: HP39gs/40gs traverse **248 entries / 496 captures** (245 generated + three ROM fonts), while HP48gII/49g+/50g traverse **247 / 494** (245 generated + two ROM fonts). The production matrix still runs one family job for `hp39`, `hp48`, and `hp4950`, covering both 64- and 80-line display layouts. It validates generated aliases/arbitrary face selection, metrics/spacing, repertoire-sensitive specimens, deterministic pagination, PASS and settled Direct return.

Native-font evidence is intentionally family-aware. The synthetic Direct launcher does not fabricate a live Saturn `Read[]` map, so exhaustive generated-font rendering is authoritative on all five targets, while HP48/49/50 native-font pixels require a real Stock logical map. Compact Stock Gallery controls close both automated live-native families: HP50g validates the hp4950 path and HP48gII independently validates the 64-line hp48 path through the original UI/ARMToolbox launch.

The compact Stock navigation remains family-aware. HP39gs/40gs use **DOWN** across minifont → main/default → EditLine, then **RIGHT** to the generated control. HP48gII/HP50g use **DOWN** across minifont → bigfont/default, then **RIGHT** to the generated control. The #112 artifacts visibly retain those distinct concrete ROM-font pages.

HP39gs/40gs are different: their fixed native raster tables are physical stock-ROM assets. Production bootstrap maps only the two required physical NOR pages (`0x0007e000` and `0x0007f000`) as uncached small pages in the private prefix of hpclang's own code L2 table. This design was chosen after diagnostics proved that a long-lived high-L1 NOR section alias could have its descriptor word overwritten by the active LCD framebuffer itself. Font Gallery therefore also serves as the regression gate for this bootstrap-private mapping.

Gallery #99 was the first fully-green production closure across Build, universal x50ng, all three Direct families, and the four Stock Gallery targets; #102 later exercised the strict HP48 settled-return gate. The final M7.1 head is now covered by **35298949457 / #112**, which repeats the full matrix after the HP39 EditLine/native-font census correction and documentation sync.

Keeping Gallery separate prevents font exploration from inflating the runtime ABI/keyboard/storage test and lets its visual evidence evolve independently of canonical runtime PASS evidence.

### Current bitmap-only font closure status (2026-09-14)

Canonical bundled assets are bitmap-source only. The generated catalog is **245 faces / 1,639,485 mapped glyphs / 3,472 assembly sources** with **245/245** source-to-generated parity. Font Assets **#24** regenerates the catalog without drift and reports no exact duplicates, overflow, glyph-collapse, blank-heavy, or Powerline regressions.

The general TTF importer has been removed. Final bitmap curation adds Cubic 11, ChillBitmap 7px, and QuanPixel/May7 with provenance retained in the source package. Gallery labels/specimens were strengthened for Ark Pixel regional variants, width-sensitive Latin, digits/punctuation, accents/symbols, and supported CJK/Japanese/Korean repertoires.

`E2E / Font Gallery` **35298949457 / #112** is the current fully-green closure gate. HP39gs/40gs Direct traverse **248 entries / 496 captures** including EditLine; HP48gII/49g+/HP50g remain **247 / 494**. Stock hp39gs/hp40gs pass mini/main/EditLine/generated-control plus ROM fingerprint/pixel oracles, and HP48gII/HP50g retain settled live-native evidence.

The font implementation/evidence slice is therefore closed. Remaining work is repository integration and bounded Stock-harness closeout, not additional font-core or Gallery implementation.
