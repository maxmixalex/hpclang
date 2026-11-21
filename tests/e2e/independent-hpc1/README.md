# Independent HPC1 prototype

This bounded HP48gII/HP49g+/HP50g experiment starts an embedded Thumb HPC1 application through
the common bootstrap from Stock RPL without installed library 275. It is an
evaluation fixture, not the default launcher or a general application loader.
It supplies no native-service context and does not qualify other model families.

The build needs ATFE tools, Python and repository sources; no ROM or historical
HPGCC binaries are required. The app receives the build's model ID, writes three words and returns 42.
HP49g+ and HP50g map six aligned pages, including four writable pages (16 KiB).
On HP48gII the Stock program reserves its arena once in the execution destination.
The initialized ARM payload is copied there before entry. The probe maps 29
aligned pages: one for the L2 tables, 16 for code and 12 for writable memory
(48 KiB). All three models use a 4 KiB entry stack. The build reuses the maintained
independent entry and common bootstrap sources.

```sh
python3 tests/e2e/independent-hpc1/build.py \
  --toolchain "$ATFE/bin" --output /tmp/hpc1-build
python3 tests/e2e/independent-hpc1/check-build.py --toolchain "$ATFE/bin"
```

Select `--model hp48gii` or `--model hp49gplus` for those builds. The default is
`hp50g`. The manifest records the model and the verifier checks its distinct ROM
fingerprint and boot metadata. Do not mix snapshots or packages between models.

The output directory must be new. `build.json` records source and output hashes.
The reproducibility check builds two isolated source copies, including a path
with spaces, compares all six release artifacts and their manifest, and removes
its temporary directories. Core CI runs this build check; it does not claim a
native execution check.

The HP48gII firmware regression boots the same full NOR image as the Stock
harness. With x50ng and its transport tools prepared, run:

```sh
dist="$PWD/build/x50ng/dist"
PATH="$dist/tools:$PATH" LD_LIBRARY_PATH="$dist/runtime-lib" \
  python3 tests/e2e/independent-entry/run-hp48gii.py --fixture independent-hpc1 \
  --toolchain "$ATFE/bin" --dist "$dist" --nor /path/to/48gii.bin --output build/hp48-hpc1
```

Add `--notebook /path/to/notebook.hpc` and choose a fresh output directory
to exercise the HP48gII Notebook built from the installed SDK. The package
contains a single execution reservation. A trailer outside the mapped pages
records the HPC1 header and the distance in nibbles to the initialized code.
On each launch the loader aligns the code, updates that distance and initializes
writable memory. A second Stock process deposits a versioned HOME note containing
`314` and recalls the resident program without sending it again. The editor
shows `314`, accepts `20` and exits with ON; HOME retains the original note.
The snapshots check the aligned code, its retained location and the restored
Stock state. After each ARM return, the Stock program creates an unreachable
temporary, records a live tag's address and calls GARBAGECOL. The reports
record the lower address and verify the tag at that location. This HOME-resident variant supplies a read-only object-store context.

With both `--notebook` and `--port0`, the runner installs the Notebook as
the first Port 0 backup named `ENTRY48` and recalls that same program twice.
On the first launch the editor creates a HOME note containing `20`. On the
second launch it reads that note and replaces it with `2020`. Each exit
checks the versioned HOME bytes directly in the Stock snapshot. The loader
checks the Stock map and its executing Port 0 record before passing the live
RPL stack bounds and saved return PC to the runtime. Port 0 remains read-only
to application code during this invocation.
The loader switches back to the saved ARM stack before computing the final
CRC over the backup, including the workspace and stack bytes written during
execution. Each snapshot checks that the executed code belongs to that record
and recalculates its CRC. This mode assumes the fresh HP48gII fixture layout.

For manual execution, use an original Stock snapshot for the selected model, without
library 275 and explicitly set to RPN mode, and the
existing Stock USB harness to transfer and evaluate `program.HP`. The program
returns decimal 124275 on the RPL stack. A fresh calculator can start in algebraic
mode, where the same EVAL key sequence enters an expression instead of executing
the recalled program. Select RPN through the Stock MODE form before capturing
the baseline; successful USB transfer alone does not prove execution. Preserve the baseline and resulting
`flash` and `sram` files, then run:

```sh
python3 tests/e2e/independent-hpc1/verify.py \
  --artifact /tmp/hpc1-build --baseline /path/to/baseline \
  --state /path/to/result/state --report /tmp/hpc1-verified.json
python3 tests/e2e/independent-hpc1/check-snapshot.py \
  --artifact /tmp/hpc1-build --baseline /path/to/baseline \
  --state /path/to/result/state --report /tmp/hpc1-negative.json
```

Verification requires the known ROM fingerprint, unchanged Flash, absent
library 275, expected application data and RPL result, owned allocation, restored
MMU entries/SWI vector/control state, unchanged code and stack guards. Fifteen
corrupted snapshot variants must fail. On HP48gII two additional cases corrupt
the last code and data page descriptors. Snapshots and ROM images are local inputs
and are not distributed with this fixture.

The default HP50g build reproduces the previously executed package SHA-256
`fbb0c00cfbfeb9d0f08e330c1c1edb9821e726fb651e0b8303b565aee305434c`.
The matching native snapshot passes this verifier. This establishes the bounded
prototype; arbitrary application loading, native-service coexistence and
cross-family resource ownership remain separate acceptance work.


The HP49g+ package is independently validated from a fresh baseline made with
its own bootloader and RPN explicitly selected. It returns model 5/result 42;
its package SHA-256 is
`42ae935a06ff0823e300147f847bfd41e1d2abdf3a5c742a3129f8ad6e88f0bf`.
Both targets pass two-copy artifact comparisons and fifteen corrupt-snapshot
checks. Swapping model packages and snapshots is rejected.
