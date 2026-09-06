# hpclang

A clean LLVM-native SDK for the ARM-based HP graphing calculators.

hpclang uses one application ABI across HP 39g+/39gs/40gs, HP 48gII and
HP 49g+/50g. Historical HPGCC source is research evidence for calculator
hardware, package formats and launch behavior; it is not a selectable runtime.

## Application model

New applications use:

- Clang / LLVM / LLD;
- ARMv4T, AAPCS32 and soft float;
- Thumb-1 C/C++ with ARM startup where useful;
- fixed virtual code and writable-memory addresses;
- the model-independent HPC1 executable format.

There is no normal-build dependency on GCC, libgcc, historical HPGCC objects
or a replacement calculator ROM.

## Architecture

```text
application -> hpclang v1 / HPC1 -> common ARM runtime
                                  -> HP39/40 platform
                                  -> HP48gII platform
                                  -> HP49g+/50g platform
```

Calculator-specific launchers own native packaging, physical-memory discovery,
MMU setup, Saturn-to-ARM transition and state restoration. Those details do not
change the application ABI.

## Validation status

- HP39gs: canonical runtime PASS on real hardware and in the ARM emulator.
- HP40gs: canonical runtime PASS on official firmware in the ARM emulator.
- HP50g: canonical runtime PASS/return plus full original-system
  ARMToolbox -> HPCLANG.HP -> PrRUN -> PASS E2E.
- HP49g+: being added to the same direct-runtime matrix.
- HP48gII: active next family bring-up; it requires a genuine 48gII hardware
  model rather than relabeling the 49/50 emulator.

CI screenshots are published to the image-only Git-LFS `ci-evidence` branch.

See the [documentation index](docs/README.md), [ABI](docs/abi.md),
[target status](docs/targets.md), [validation](docs/validation.md), and
[roadmap](docs/ROADMAP.md).
