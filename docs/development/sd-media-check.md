# HP49g+ / HP50g SD-card check

This optional hardware check complements the automated card-image tests. The
maintainer has an HP49g+ and SD card; running it is a nonblocking follow-up for M7.
No physical result is claimed until the check is actually performed.

## Package and launch

Download the **hp4950-sd-media-check** artifact from the Storage workflow. Use
`hp49gplus.hp` on HP49g+, or `hp50g.hp` on HP50g. These are ordinary hpclang
ARMToolbox payloads. Recall the selected file as an object, then run ARMToolbox's
`PrRUN` in RPN mode, using the same launcher installation as other hpclang programs.
If ARMToolbox is not installed, the package also contains `SETUP.BIN`; recall
and run that installer first and follow its installation prompts.

To reproduce the package with the repository's pinned Arm toolchain:

```sh
bash tools/build-e2e.sh sd-media-manual build/sd-media-manual hp4950
```

## Procedure

1. Insert a writable SD card. Keep the **same card** throughout this check.
2. Launch the program and press ENTER at `SD CARD CHECK`. It exclusively creates
   `/HPCSDCHK.BIN`; an existing file is never overwritten.
3. At `REMOVE AND REINSERT`, remove the card and insert it again. Then press
   ENTER. The application makes no storage queries during this wait.
4. Expect `PASS`. This means the old buffered write was rejected, the original
   bytes survived, fresh reads/writes worked, and the test file was removed.
5. Press ENTER or ON to return. Open the original system's file manager and
   confirm the card is still accessible.
6. Report the model, ROM version, card size/type, displayed result and whether
   the original file manager works. Repeating with a quick and a slower removal
   gives useful additional evidence.

ON cancels a prompt. Cancellation or failure can leave `/HPCSDCHK.BIN`; keep it
for diagnosis, then remove this test file before retrying. Successful runs clean
it up automatically. The check does not format the card or edit other files.

| Code | Meaning |
| --- | --- |
| 1 | `/HPCSDCHK.BIN` already exists; inspect it before removing it for another run |
| 2–3 | Could not create/write the initial file |
| 4 | Could not prepare the old buffered stream |
| 5 | The old stream did not fail with the required stale-handle error |
| 6–7 | Could not reopen the original data, or its bytes changed |
| 8–9 | Fresh write/read/cleanup failed after reinsertion |

The automated tests additionally swap two distinct FAT images and independently
inspect the replacement image, proving that stale buffered writes cannot alter
its sentinel file. That evidence does not replace this physical card check.
