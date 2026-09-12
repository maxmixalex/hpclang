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

HP 39g+ is filtered out by its explicit `automated_e2e: false` profile flag rather than by a hard-coded workflow exclusion.

GitHub Actions YAML is orchestration, not the test specification. Expected files, required captures, protocol events, and similar assertions belong under `tests/e2e/specs/` and are evaluated by the E2E harness. Fixture integrity checks such as a pinned firmware SHA-256 belong to fixture preparation, not to the behavioral test specification.

## Full-keyboard validation and progression evidence

The canonical runtime E2E injects every physical key position on each automated calculator model. The guest verifies DOWN/UP events, raw scan code, physical key identity, and dedicated-key modifier state for all 51 keys.

The final LCD screen renders the physical keyboard rather than the electrical scan matrix: six menu keys, two three-key control rows beside the cursor cluster, and seven five-key main rows. Passed positions are filled, and a failing position is left open and marked. The screen labels use the public font/text API, so calculator E2E exercises the same rasterization path instead of carrying a test-only glyph table. The machine-readable result remains the actual gate; the LCD diagram is a visual summary of the same checks.

Direct currently captures an initial hollow keyboard plus one frame after every accepted key release: **52 progression frames** for the 51-key test. Stock should mirror the same capture schedule. The merged progression contact sheet is supplementary evidence and should be published separately from the canonical `before / pass / return` summary rather than being squeezed into that three-frame layout.

## Font Gallery application

Font Gallery is a focused visual/sample E2E application, not part of the canonical runtime test. The public font core itself has no semantic theme-role layer: the Gallery selects calculator-native `hpclang_font_mini()` / `hpclang_font_big()` entries and generated font products directly.

The exhaustive Direct Gallery traverses **310 entries** — 308 generated faces plus two native entries — and renders both SAMPLE and TEXT pages, producing **620 captures per target**. The production matrix runs one family job for `hp39`, `hp48`, and `hp4950`, covering HP39gs, HP40gs, HP48gII, HP49g+, and HP50g and therefore both 64- and 80-line display layouts. It validates generated aliases/arbitrary face selection, metrics/spacing, representative coverage, deterministic pagination, PASS and settled return.

Native-font evidence is intentionally family-aware. The synthetic Direct launcher does not fabricate a live Saturn `Read[]` map, so exhaustive generated-font rendering is authoritative on all five targets, while HP48/49/50 native-font pixels require a real Stock logical map. HP50g's compact Stock Gallery closes the hp4950 live-native path through the original UI/ARMToolbox launch and one generated control face. Compact HP48gII Stock Gallery remains the outstanding 64-line live-native evidence item.

HP39gs/40gs are different: their fixed native raster tables are physical stock-ROM assets. Production bootstrap maps only the two required physical NOR pages (`0x00042000` and `0x00043000`) as uncached small pages in the private prefix of hpclang's own code L2 table. This design was chosen after diagnostics proved that a long-lived high-L1 NOR section alias could have its descriptor word overwritten by the active LCD framebuffer itself. Font Gallery therefore also serves as the regression gate for this bootstrap-private mapping.

Formal production run **34698825657 / #42** is green across the Build stage, universal x50ng, `Gallery / hp39`, `Gallery / hp48`, `Gallery / hp4950`, and `Stock Gallery / hp50g`. The earlier targeted HP39 exhaustive run independently completed 310 entries / 620 captures on both HP39gs and HP40gs.

Keeping Gallery separate prevents font exploration from inflating the runtime ABI/keyboard/storage test and lets its visual evidence evolve independently of canonical runtime PASS evidence.
