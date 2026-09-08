# Emulator E2E layout

The emulator tests are split by what they prove, not by which historical
emulator happens to execute them.

## Test tiers

- **Direct runtime** — boot a target-appropriate stock firmware/full-NOR
  fixture, bypass calculator menus/installers, enter the common hpclang ARM
  boundary, run the canonical HPC1 runtime, then restore the stock execution
  state.
- **Stock launch** — drive the calculator's original UI and supported launcher
  path, then require the same machine-readable HPC1 PASS result.

Current stock-launch transports intentionally match each calculator family:

- HP39gs/HP40gs use the original serial-disk path. A headless Kermit server
  supplies `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000` and
  `LIB275L.000`; the stock ROM performs the transfer/install itself.
- HP48gII uses the stock serial Kermit server to receive `L275_48.LIB` and
  the generated `HPCLANG.HP`, then installs library 275 into Port 0 and
  launches the payload through ARMToolbox48 / PrRUN.
- HP49g+/HP50g use an emulated SD card to install ARMToolbox and copy the
  generated `HPCLANG.HP` payload. They share the same stock UI sequences
  while retaining their native per-model ARM bootloaders.

GitHub Actions orchestration lives in `.github/workflows/e2e-*.yml`.
Emulator frontends live in `tests/x50ng/`. Human-like key input belongs under
`tests/e2e/sequences/`.

## Target profiles

`tests/e2e/targets/*.json` contains only emulator target metadata shared by
workflows:

- `target`: workflow/filename identity;
- `family`: firmware/emulator family selector;
- `model`: hpclang stage0 model ID;
- `lcd_height`: 64- or 80-line capture height.

Only targets distributed as per-model firmware archives add a `firmware`
object. Today that is HP39gs/HP40gs, whose object supplies the hpcalc archive
name and member path. HP49g+/HP50g share the 2.15 OS image but carry their
distinct `bootloader` filenames because that is genuine per-model metadata.
HP48gII uses a pinned full-NOR fixture. Profiles do not carry empty fields.

`tools/e2e-target-env.py` exposes a selected profile as `HPCLANG_E2E_*`
variables for the workflow.

## Sequence files

A sequence is an absolute-time script:

```text
9200 capture files
9500 tap up
9800 tap f6
12000 chord rshift two
```

The stock runner accepts logical key actions rather than GUI coordinates.
