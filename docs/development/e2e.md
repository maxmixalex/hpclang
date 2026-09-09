# E2E internals

The test tree has two layers. `tests/unit/` contains host-side `test_*.c`
tests for pure implementation logic. `tests/e2e/` contains one calculator-side
hpclang consumer project plus the harness data used to launch that same project
through different targets.

The emulator workflows are split by what they prove, not by which historical
emulator happens to execute them.

## Test tiers

**Direct runtime** boots a target-appropriate stock firmware or full-NOR fixture, bypasses calculator menus/installers, enters the common hpclang ARM boundary, runs the unified E2E HPC1 application, and then restores the stock execution state.

**Stock launch** drives the calculator's original UI and supported launcher path, then requires the same machine-readable E2E PASS result from that same application.

The two tiers are deliberately separate. Direct runtime owns common-bootstrap execution and return correctness. Stock launch owns calculator-side package, transfer, menu, and launcher integration.

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

Human-like key input belongs under `tests/e2e/sequences/`. A sequence is an absolute-time script:

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

## CI build stages

The E2E source project is target-independent. CI should therefore avoid rebuilding it once per calculator.

The implemented pipeline has two execution layers, plus one shared-asset stage for Stock:

1. **test application** — build one HPC1 application per test mode (Direct or Stock), including all calculator-testable public SDK coverage and all target launchers;
2. **family execution** — run one job for each of `hp39`, `hp48`, and `hp4950`. The job prepares/restores its x50ng variant once, installs runtime dependencies once, discovers eligible targets from `tests/e2e/targets/*.json`, and executes sibling targets in parallel.

Stock additionally prepares transfer/launcher assets once before the three family jobs. HP 39g+ is filtered out by its explicit `automated_e2e: false` profile flag rather than by a hard-coded workflow exclusion.

x50ng's cache is for cross-run reuse. The workflows do not repack a freshly prepared x50ng tree into an intra-run artifact merely to hand it to target jobs; the family job owns both platform preparation and target execution. Build artifacts are reserved for the HPC1 application/launchers and genuinely shared Stock assets.

GitHub Actions YAML is orchestration, not the test specification. Expected files, required captures, protocol events, and similar assertions belong under `tests/e2e/specs/` and are evaluated by the E2E harness. Fixture integrity checks such as a pinned firmware SHA-256 belong to fixture preparation, not to the behavioral test specification.
