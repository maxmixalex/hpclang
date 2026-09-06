# Emulator E2E layout

The emulator tests are split by what they prove, not by which historical
emulator happens to execute them.

## Test tiers

- **Direct runtime** — boot stock firmware, bypass calculator menus/installers,
  enter the common hpclang ARM boundary, run the canonical HPC1 runtime, then
  restore the exact stock execution state.
- **Stock launch** — begin from a normal calculator-side installed state, drive
  the stock UI and supported launcher, execute HPC1, and return to the stock
  calculator environment.

The HP50g gate can also create its installed state through the emulated SD card.
HP39gs/HP40gs have no equivalent SD injection path, so their stock-launch gate
uses a generated post-transfer fixture.

GitHub Actions orchestration lives in `.github/workflows/e2e-*.yml`.
Emulator frontends live in `tests/x50ng/`. Human-like key input belongs under
`tests/e2e/sequences/`.

## Generated HP39/40 fixtures

The fixture generator consumes the same transfer set produced for real
hardware:

- `HP39DIR.CUR`;
- `HP39DIR.000`;
- `HPGCC000.000`;
- `LIB275L.000`.

The preferred builder boots the official ROM and feeds those files through a
headless host transport so the calculator's own transfer/install code creates
its persistent state. The result is an x50ng datadir containing the normal
`flash`, `sram` and `state` files.

The cache key includes the firmware identity, all four input-file digests and
the builder version. A payload/package change therefore rebuilds the fixture
automatically.

Direct patching of calculator SRAM/flash is a fallback only. Conn3x's Windows
GUI is not part of the regression gate.

## Sequence files

A sequence is an absolute-time script:

```text
9200 capture files
9500 tap up
9800 tap f6
12000 chord rshift two
```

The stock runner accepts logical key actions rather than GUI coordinates.
