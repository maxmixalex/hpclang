# Tests

hpclang has two source-test layers.

## Unit tests

`unit/` contains host-side tests named `test_*.c`. Each test covers one
pure or hardware-independent implementation unit and is run through CTest.

Unit tests must not depend on calculator firmware, x50ng, stock UI navigation,
or test-only replacements for SDK runtime services.

## End-to-end tests

`e2e/` contains calculator-side HPC1 applications plus the shared harness data
used to launch them through supported calculator paths.

The canonical runtime application owns ABI/runtime/device/time/display/keyboard/
storage regression coverage and is built once for the Direct and Stock launch
tiers. Focused applications may be separate executables when their lifecycle or
evidence would make the canonical runtime gate less deterministic. Font Gallery
is the first explicit example: it is a separate visual/sample application rather
than extra pages inside the canonical runtime payload.

Target profiles, stock UI sequences, E2E specifications, and emulator
integration are shared harness data rather than separate platform test suites:

- `e2e/targets/`: target/model facts used by the harness;
- `e2e/sequences/`: original-system UI sequences;
- `e2e/specs/`: machine-readable behavioral/evidence expectations;
- `e2e/harness/`: x50ng host frontends and family runners;
- `e2e/font-gallery/`: focused all-face font visual/sample application.

When a new public SDK capability belongs naturally in the canonical runtime
contract, add it there. Visual/demo-heavy or independently scoped behavior may
use a focused E2E application, but it should reuse the common target and harness
infrastructure rather than create another emulator framework. Add a unit test
separately when the implementation has pure logic worth testing in isolation.
