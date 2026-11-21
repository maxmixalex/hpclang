# Independent classic Stock entry fixture

This fixture builds a Stock RPL program that enters ARM code without
ARMToolbox/library 275, calls freestanding C using a program-owned 4 KiB stack
and 536-byte workspace plus a 20-byte control-state record, restores the saved ARM context, and returns to RPL.
The program then pushes the exact integer 124275. It does not load HPC1 apps.

Build with the project's ATFE 22.1.0 toolchain (LLVM tools in `bin`):

```sh
python3 tests/e2e/independent-entry/build.py \
  --toolchain /path/to/atfe/bin --output /tmp/independent-entry
```

The default is HP50g. Select `--model hp48gii` for the 256 KiB Stock
main-RAM address translation, or `--model hp49gplus` for HP49g+.
The snapshot verifier uses the build manifest to select SRAM and ROM checks.

The output directory must not exist. `program.HP` is the transfer artifact;
`probe.elf`, `probe.bin` and `metadata.json` describe its ARM payload and reserved
storage. `build.json` records compiler identity and source/artifact SHA-256s.
The shared assembly is a hashed build input; `--bootstrap-sync` can select its
path for independent source copies. No research scripts, ROM files or installed HPGCC objects are build inputs.

Core runtime CI also runs the build-only reproducibility check:

```sh
python3 tests/e2e/independent-entry/check-build.py --toolchain /path/to/atfe/bin
```

It builds two independent source copies (including a path containing a space),
checks their input/output manifests and compares all five emitted release/debug
artifacts byte for byte. Temporary sources and builds are removed on completion
or failure. This gate needs no ROM and does not replace Stock execution checks.

The Saturn encoder uses Stock SAVE/RESTORE addresses and the selected SRAM
mapping. It copies the ARM payload into aligned storage reserved inside the
program object. The entry saves registers and uses the shared
`runtime/bootstrap/sync.S` SWI helpers to mask IRQ/FIQ before entering C and
restore the caller control state afterward. Its earlier direct CPSR write was
ineffective in user mode; the snapshot now checks the actual control bits
before, during and after C execution, plus restoration of the SWI vector.
This remains a bounded synchronous experiment, not a service-coexistence or
interrupt-latency qualification.

Validate a completed emulator state against the unchanged pre-run snapshot:

```sh
python3 tests/e2e/independent-entry/verify.py \
  --artifact /tmp/independent-entry --state /path/to/final-state \
  --baseline /path/to/pre-run-state --report /tmp/entry-result.json
python3 tests/e2e/independent-entry/check-snapshot.py \
  --artifact /tmp/independent-entry --state /path/to/final-state \
  --baseline /path/to/pre-run-state --report /tmp/entry-negative.json
```

The verifier checks the ROM fingerprint and physical SRAM size for the model in
the build manifest (256 KiB for HP48gII and 512 KiB for HP49g+/HP50g). Both snapshots must have no library 275 registered.
It verifies artifact hashes, the 128 workspace values/checksum, stack
alignment/bounds, IRQ/FIQ masking and restoration, SWI vector restoration,
unchanged code/guards, object containment, the RPL return
integer and unchanged Flash. Equivalent SRAM mapping-table copies are allowed;
conflicting mappings are rejected. The negative check corrupts ten temporary
snapshot copies and removes those copies when finished.

To reproduce the HP48gII CI run, supply the original 2 MiB NOR image used
by the HP48gII Stock harness and a prepared x50ng bundle:

```sh
dist="$PWD/build/x50ng/dist"
PATH="$dist/tools:$PATH" LD_LIBRARY_PATH="$dist/runtime-lib" \
  python3 tests/e2e/independent-entry/run-hp48gii.py --nor /path/to/48gii.bin \
  --toolchain "$ATFE/bin" --dist "$dist" --output build/independent-hp48-entry
```

The runner boots fresh firmware, transfers the program over emulated USB,
evaluates it in Stock and writes the verified return to `report.json`.
The ten corruption checks write their results to `corruptions.json`.

HP50g local validation includes a fresh USB transfer and Stock execution using a copy
of the historical no-library-275 emulator baseline, not a freshly initialized
ROM or physical calculator. The returned HOME screen shows 124275; the snapshot
verifier and all ten corruptions pass. The observed CPSR control sequence is
`0x10 -> 0xd0 -> 0x10`; both SWI vector observations are `0x0003f568`. The install-mode harness does not set
its generic application `returned`/`return_settled` flags, so those flags are not
claimed as evidence here. The snapshot and observed screen establish this
fixture's return. No ROM or SRAM dumps are shipped with this fixture.

Full independent-launcher acceptance additionally requires HPC1 integration,
allocation/MMU and native-service coexistence, other model families including
the HP39 Aplet path, and an explicit packaging/migration decision. Do not infer
those capabilities or hardware safety from this entry fixture. The existing
application launch paths remain required.

## HP39 Aplet import probe

Run the following with the prepared HP39 SDK data directory to build a small
Stock RPL Aplet. Pass `--attached-control` with a second output directory to
retain 275 in the directory header. Both bundles omit the support library and
replace its four template calls. ENTER is intended to store the string
`hpclang-enter` as the active Aplet's custom data. This experiment targets
the native view and writeback contract before introducing the ARM payload.

```sh
python3 tests/e2e/independent-entry/build-hp39-aplet.py \
  --assets "$HP39_APLET_HOME/data" --output /tmp/entry-probe
```

Local HP39g+ D1.55 original-ROM USB runs on fresh state transferred the
Aplet and named Reset program. Both attachment settings produced the Stock
dialog "Getting library / Transfer Failed". After dismissal the Aplet list
selected Function, and ENTER opened Function Symbolic View. The harness
reported `installed` when its scripted sequence ended. Check the Stock
dialog and selected Aplet before interpreting any subsequent ENTER or marker.
The transferred payload itself contains the marker string.