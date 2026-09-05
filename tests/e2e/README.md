# Emulator E2E layout

The emulator tests are split by what they prove, not by which historical
emulator happens to execute them.

## Test tiers

- **Official ROM boot** — boot an unmodified HP calculator ROM to a responsive
  normal calculator screen. This checks the emulator/firmware model only and
  does not execute hpclang.
- **Direct runtime** — bypass the calculator's menus and installers, enter the
  common hpclang ARM boundary directly, run the HPC1 runtime test, then return
  to the HP operating system. This tells us whether hpclang itself works.
- **Full calculator launch** — use the calculator exactly as a user would:
  install/select the required launcher, copy the hpclang program through the
  calculator UI, launch it, run HPC1, and return. This additionally tests the
  HP-side packaging, menus, libraries and launcher.

GitHub Actions orchestration lives only in `.github/workflows/e2e-*.yml`.
Emulator adapters live in `tests/x50ng/`. Human-like input is test data under
`tests/e2e/sequences/`; changing a key path or timing should not require
editing workflow YAML or recompiling a new schedule into the runner.

## Sequence files

A sequence is an absolute-time script:

```text
9200 capture files
9500 tap up
9800 tap f6
12000 chord rshift two
```

The x50ng stock runner currently accepts:

- `tap <key>`
- `chord <modifier> <key>`
- `capture <name>`
- `monitor`
- `finish-install`
- `fail <reason>` for deliberate diagnostic checkpoints

The runner translates logical key names to the calculator key matrix. There is
no GUI coordinate automation.

## HP39/40 stock fixtures

HP39gs/HP40gs do not have the SD-card injection path used by HP50g. Their normal
host transfer is Conn3x/USB/serial, while x50ng currently models the S3C2410
device registers but does not expose a host transport endpoint suitable for
Conn3x.

Therefore the first automated HP39/40 **full calculator launch** gate starts from a deterministic
post-transfer calculator-state fixture containing the normal packaged Aplet.
The test still drives the stock APLET UI, enters the historical outer launcher,
executes hpclang, checks the direct-runtime result word and verifies return to a
responsive stock OS.

PC-to-calculator transfer protocol is a separate integration concern. It should
only become part of this gate if a maintained emulator transport makes that
useful and reliable.
