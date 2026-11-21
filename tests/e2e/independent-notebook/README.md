# HP49g+ and HP50g independent Notebook save and relaunch

Run from the repository with the pinned ATfE toolchain, prepared x50ng bundle,
Kermit and socat. Use a new output directory:

```sh
dist="$PWD/build/x50ng/dist"
PATH="$dist/tools:$PATH" LD_LIBRARY_PATH="$dist/runtime-lib" \
  python3 tests/e2e/independent-notebook/regression.py \
  --toolchain "$ATFE/bin" --dist "$dist" --output build/independent-notebook
```

The default target is HP50g. Add `--model hp49gplus` and select a fresh
output directory to run HP49g+. The selected target determines the SDK build,
the model ID passed to the application, the bootloader and the expected ROM
fingerprint.

The test boots firmware 2.15, checks that library 275 is absent, installs
the SDK and builds its plain Notebook example. Stock creates a named Port 1
backup containing the staged code. KOS allocations and the backup supply a
fixture-specific split mapping. The loader receives the actual saved ARMSYS
continuation and denies Port 1 edits. This fixed-layout emulator fixture is
not a production launcher.

The first editor must be empty. Key events type `20`, save the note and exit.
The next process runs the same application image with 20 KiB of writable
memory advertised to the runtime, including its 8 KiB stack. Notebook returns
1 when initialization runs out of memory. The fixture restores the mappings,
checks every KOS release, reallocates and releases equivalents, then checks
the application status. A distinct RPL result identifies this failure run.

The last application launch uses the state left by the failure and recalls the original
32 KiB program. It must show `20` in the editor. Key events append another `2`, activate
the Port 0 backup button and exit normally. A fresh Stock session recalls
`:0:HPCNOTE` and verifies that the backup contains `202` while HOME retains
`20`. The verifier
checks the versioned HOME string, GC witness, RPL return and two settled Stock
frames in all three processes. It independently checks editor pixels on the
two successful launches. See `report.json` for the phase results. The GC
witness runs after application exit while the leases are still held.
