# Tests

hpclang has two test layers.

## Unit tests

`unit/` contains host-side tests named `test_*.c`. Each test covers one
pure or hardware-independent implementation unit and is run through CTest.

Unit tests must not depend on calculator firmware, x50ng, stock UI navigation,
or test-only replacements for SDK runtime services.

## End-to-end tests

`e2e/` is one hpclang consumer project. The same project exercises the public
SDK/runtime surface and is built into one HPC1 application. CI launches that
application through each supported target path.

Target profiles, stock UI sequences, and emulator integration are harness data;
they do not define separate test programs:

- `e2e/targets/`: target facts used by the harness;
- `e2e/sequences/`: original-system UI sequences;
- `e2e/harness/`: x50ng host frontends.

When a new public SDK capability can be exercised on a calculator, add it to
the unified E2E project rather than creating another smoke application. Add a
unit test separately when the implementation has pure logic worth testing in
isolation.
