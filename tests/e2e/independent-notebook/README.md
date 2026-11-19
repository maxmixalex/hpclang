# HP50g independent Notebook save and relaunch

Run from the repository with the pinned ATfE toolchain, prepared x50ng bundle,
Kermit and socat. Use a new output directory:

```sh
dist="$PWD/build/x50ng/dist"
PATH="$dist/tools:$PATH" LD_LIBRARY_PATH="$dist/runtime-lib" \
  python3 tests/e2e/independent-notebook/regression.py \
  --toolchain "$ATFE/bin" --dist "$dist" --output build/independent-notebook
```

The test boots HP50g firmware 2.15, checks that library 275 is absent, installs
the SDK and builds its plain Notebook example. Stock creates a named Port 1
backup containing the staged code. Four KOS blocks and the backup supply a
fixture-specific split mapping. The loader receives the actual saved ARMSYS
continuation and denies Port 1 edits. This fixed-layout emulator fixture is
not a production launcher.

The first editor must be empty. Physical key events type `20`, activate Save
and exit. A second emulator process uses the first process's state and recalls
the installed program without another transfer. The verifier checks the full
versioned HOME string, renders the expected editor text independently, checks
the GC witness and RPL return, and compares two Stock frames one second apart.
See `report.json` for the phase results. The fixture exercises GC after exit
while leases remain held, rather than forcing GC inside the application's Save.
