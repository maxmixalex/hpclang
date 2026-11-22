# HP49g+ and HP50g independent Notebook save and relaunch

Run from the repository with the pinned ATfE toolchain, prepared x50ng bundle,
Kermit, socat, dosfstools and mtools. Use a new output directory:

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
Both models can share one SDK installation. Pass its prefix with
`--sdk /path/to/sdk` when testing the same source revision.

The test boots firmware 2.15, checks that library 275 is absent, installs
the SDK and builds its plain Notebook example. Stock creates a named Port 1
backup containing the staged code after a 4,500-byte padding string in its
own backup. The loader finds `NBLSE` in the native record chain, verifies its
image and maps all 20 code pages read-only from that record. After Stock
purges `NBLSE`, the same entry must return its refusal result, preserve the
HOME note and leave the remaining Port 1 contents intact.
The normal entry requests nine aligned KOS pages (eight writable pages and one
page for the two coarse page tables). The low-memory entry requests six
(five writable pages and one table page). These counts exclude alignment
slack and KOS allocator metadata. The loader receives the actual saved ARMSYS
continuation and denies Port 1 edits. This fixed-layout emulator fixture is
not a production launcher.

The first editor must be empty. Key events type `20`, save the note and exit.
A second launch asks KOS for 35 aligned pages. On partial allocation the
loader releases every acquired block before installing any mappings. The test
reacquires all those block sizes together and releases them again. Stock GC
runs, and the private RPL result remains `124277` for admission refusal.

The next process runs the same application image with 20 KiB of writable
memory advertised to the runtime, including its 8 KiB stack. Notebook returns
1 when initialization runs out of memory. The fixture restores the mappings,
checks every KOS release, reallocates and releases equivalents, then checks
the application status. A distinct RPL result identifies this failure run.

On relaunch the original 32 KiB program must show `20` in the editor.
Key events append a second `2`, export the edit to the FAT16 card, activate
the Port 0 backup button and exit. A fresh Stock session recalls `:0:HPCNOTE`.
The backup must contain `202` and HOME must still contain `20`. The card
verifier reads `HPCNOTE.TXT` and a pre-existing file and checks their bytes.

The Stock verifier checks the versioned HOME string, GC witness, RPL return
and two settled Stock frames after each application exit. It also checks the
editor pixels on the two successful launches. The GC witness runs with leases held after application exit and with the
partial pool already released after admission refusal. See `report.json` for the
phase results.
