# Validation

Validation is split into runtime tests and original-system launch tests so a calculator menu or transfer problem cannot be mistaken for a runtime regression.

## Continuous gates

| Gate | What it proves |
| --- | --- |
| `CI / Core runtime` | LLVM/LLD, compiler builtins, libc integration, and common runtime invariants |
| `CI / HP39 package` | HP 39g+/39gs/40gs hardware-validation packages build correctly |
| `E2E / Direct runtime` | the common HPC1 runtime executes and returns on HP 39gs, 40gs, 48gII, 49g+, and 50g fixtures |
| `E2E / HP39/40 stock launch` | stock HP 39gs/40gs APLET UI and serial-disk transfer reach ARMToolbox39 and HPC1 |
| `E2E / HP48gII stock launch` | stock HP 48gII Kermit transfer, ARMToolbox48, and `PrRUN` reach HPC1 |
| `E2E / HP49/50 stock launch` | stock HP 49g+/50g SD/File Manager and ARMToolbox path reaches HPC1 |
| `Maintenance / CI` | old Actions runs and generated CI evidence are pruned |

HP 39g+ is deliberately absent from the Direct and stock-launch E2E matrices until a verified real-machine flash fixture is available.

## Runtime PASS contract

The canonical runtime test covers initialized globals, BSS, static TLS, constructors, 32-bit and 64-bit division, ARMv4T-safe software floating point, libc memory/string operations, heap allocation, and `snprintf`. Success is recorded as the machine-readable word `0x50415353` and a visible `RUNTIME PASS`.

The Direct emulator gate additionally verifies that temporary mappings and the SWI vector are restored before stock execution resumes.

## Original-system launch tests

HP 39gs and HP 40gs boot the official 2.22 firmware and use the calculator's serial-disk path to request `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000`. The installed APLET launches through ARMToolbox39 and must produce the same HPC1 PASS result.

HP 48gII boots the pinned full-NOR fixture, uses the stock Serial Kermit server to receive library 275 and `HPCLANG.HP`, installs the library into Port 0, and launches through ARMToolbox48/`PrRUN`.

HP 49g+ and HP 50g use their separate native bootloaders with the shared official 2.15 OS. Both install ARMToolbox and `HPCLANG.HP` through the stock File Manager and use the same launch sequence.

Human-like key sequences are stored under `tests/e2e/sequences/`. Model fixture metadata is stored under `tests/e2e/targets/`. x50ng frontends under `tests/x50ng/` drive the calculator key matrix and LCD directly; CI does not use desktop-window automation or screen scraping.

## HP 39g+ blocker

The remaining HP 39g+ work requires a real calculator. The retained research dumper can read stock NOR without programming or erasing it, and the 16 KiB boot-dump mechanism has already been verified byte-for-byte against emulated physical flash.

When a real HP 39g+ is available, the next step is to obtain and verify its complete stock flash image, then add that fixture to Direct runtime and original-system launch tests. Until then, further HP 39g+ E2E work is intentionally paused.

## Evidence retention

LCD summaries are published to the image-only `ci-evidence` branch. Transient test artifacts use short retention windows, and the maintenance workflow prunes old Actions runs and evidence directories.
