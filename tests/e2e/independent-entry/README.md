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

Build a Stock RPL Aplet using the prepared HP39 SDK data directory.

```sh
python3 tests/e2e/independent-entry/build-hp39-aplet.py \
  --assets "$HP39_APLET_HOME/data" --output /tmp/entry-probe
```

The default attachment ID is `0x7ff`, observed in both Function and its
user-saved copy on HP39g+ D1.55. Install the four generated files through the
Stock USB disk interface, start EntryProbe, press ENTER, then return to the
Aplet library. Verify the saved emulator SRAM against the original transfer:

```sh
python3 tests/e2e/independent-entry/check-hp39-aplet.py \
  /tmp/entry-probe/HPGCC000.000 /path/to/state/sram
```

In a local original-ROM run, Stock listed EntryProbe at 0.97 KB after import
and 0.98 KB after exit. The verifier matched the entire named Aplet directory
including the custom-data field changed from an empty list to the string
`hpclang-enter` and the two adjusted links. The code in the original transfer
also contains that string. Searching for the marker alone cannot establish
writeback.

For importer controls, build into fresh directories with `--attached-control`
(ID 275) and `--zero-control` (ID zero). All three packages omit the library.
Both controls delivered the Aplet and Reset files on fresh HP39g+ D1.55 Stock
but displayed "Getting library / Transfer Failed". The harness reports
`installed` when its scripted sequence ends even on that failure. The
directory verifier rejected the failed zero-ID snapshot.

For the copy and Reset check, save a copy named `2` from the Aplet library.
Check its snapshot with `--copy-state saved`. Select that copy, press Reset
and confirm, then return to the library and check with `--copy-state reset`.
Start the copy again, press ENTER and return to the library. Check with
`--copy-state saved` again. On HP39g+ D1.55 all three snapshots matched the
two named directories. Reset stored the Stock empty-list reference (0x05da2)
in the copy. The original retained its marker throughout. Each snapshot also
failed verification when given the opposite expected copy state.

HP39gs and HP40gs also passed with their respective Stock 2.22 firmware and
the same four-file bundle. The full named directories matched after initial
ENTER, copy, confirmed Reset and relaunch on each model. When scripting USB
reception for this bundle, use the model's Stock USB sequence and
`tools/wait-hp39-kermit-fetch.py wait LOG MARKER --last-file HPCRESET.000`.
This releases the payload gate after the named Reset program transfers.

This probe exercises RPL Start, ENTER, Exit and copied-Aplet Reset. ARM entry, Notebook resource
ownership and physical-device behavior require separate execution checks.

For the HP39g+ D1.55 ARM return probe, add `--arm-entry` to both the Aplet
build and the SRAM check above. Use a fresh output directory. The 32-byte
payload saves incoming r0, writes 0x48504139 over the seed in its executable
copy, restores r0 and returns through lr. Following ENTER, RPL stores its own
marker and Exit commits the modified data. The checker matches the named
directory including the unchanged ARM source, the aligned copy with its one
changed word and the RPL custom data. In a copy of the installed snapshot,
replacing the r0 pop with a four-byte stack adjustment reproduced abort mode
and an unsampled Aplet key. The positive trial returned to the Aplet list.
This ARM check has run on HP39g+ D1.55 only.
