# Independent HP50g entry fixture

This fixture builds a Stock RPL program that enters ARM code without
ARMToolbox/library 275, calls freestanding C using a program-owned 4 KiB stack
and 536-byte workspace, restores the saved ARM context, and returns to RPL.
The program then pushes the exact integer 124275. It does not load HPC1 apps.

Build with the project's ATFE 22.1.0 toolchain (LLVM tools in `bin`):

```sh
python3 tests/e2e/independent-entry/build.py \
  --toolchain /path/to/atfe/bin --output /tmp/independent-entry
```

The output directory must not exist. `program.HP` is the transfer artifact;
`probe.elf`, `probe.bin` and `metadata.json` describe its ARM payload and reserved
storage. `build.json` records compiler identity and source/artifact SHA-256s.
No research scripts, ROM files or installed HPGCC objects are build inputs.

Core runtime CI also runs the build-only reproducibility check:

```sh
python3 tests/e2e/independent-entry/check-build.py --toolchain /path/to/atfe/bin
```

It builds two independent source copies (including a path containing a space),
checks their input/output manifests and compares all five emitted release/debug
artifacts byte for byte. Temporary sources and builds are removed on completion
or failure. This gate needs no ROM and does not replace Stock execution checks.

The Saturn encoder uses HP50g SAVE/RESTORE addresses and the SRAM physical
mapping. It copies the ARM payload into aligned storage reserved inside the
program object. IRQ and FIQ are masked during the C call; the saved CPSR and
registers are restored on return. This is a bounded synchronous experiment,
not a service-coexistence or interrupt-latency qualification.

Validate a completed emulator state against the unchanged pre-run snapshot:

```sh
python3 tests/e2e/independent-entry/verify.py \
  --artifact /tmp/independent-entry --state /path/to/final-state \
  --baseline /path/to/pre-run-state --report /tmp/entry-result.json
python3 tests/e2e/independent-entry/check-snapshot.py \
  --artifact /tmp/independent-entry --state /path/to/final-state \
  --baseline /path/to/pre-run-state --report /tmp/entry-negative.json
```

The verifier is qualified for the HP50g ROM code fingerprint embedded in
`verify.py` and 512 KiB SRAM. Both snapshots must have no library 275 registered.
It verifies artifact hashes, the 128 workspace values/checksum, stack
alignment/bounds, unchanged code/guards, object containment, the RPL return
integer and unchanged Flash. Equivalent SRAM mapping-table copies are allowed;
conflicting mappings are rejected. The negative check corrupts six temporary
snapshot copies and removes those copies when finished.

Local validation includes a fresh USB transfer and Stock execution using a copy
of the historical no-library-275 emulator baseline, not a freshly initialized
ROM or physical calculator. The returned HOME screen shows 124275; the snapshot
verifier and all six corruptions pass. The install-mode harness does not set
its generic application `returned`/`return_settled` flags, so those flags are not
claimed as evidence here. The snapshot and observed screen establish this
fixture's return. No ROM or SRAM dumps are shipped with this fixture.

Full independent-launcher acceptance additionally requires HPC1 integration,
allocation/MMU and native-service coexistence, other model families including
the HP39 Aplet path, and an explicit packaging/migration decision. Do not infer
those capabilities or hardware safety from this entry fixture. The existing
application launch paths remain required.
