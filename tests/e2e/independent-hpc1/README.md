# Independent HPC1 prototype

This bounded HP50g experiment starts an embedded Thumb HPC1 application through
the common bootstrap from Stock RPL without installed library 275. It is an
evaluation fixture, not the default launcher or a general application loader.
It supplies no native-service context and does not qualify other model families.

The build needs ATFE tools, Python and repository sources; no ROM or historical
HPGCC binaries are required. The app receives model 6 and 16 KiB of RW memory,
writes three words, cleans its cache and returns 42. The loader owns a 28 KiB
alignment arena and the entry owns a 4 KiB stack. The six aligned arena pages
hold the L2 table, code and four RW pages. The build reuses the maintained
independent entry and common bootstrap sources.

```sh
python3 tests/e2e/independent-hpc1/build.py \
  --toolchain "$ATFE/bin" --output /tmp/hpc1-build
python3 tests/e2e/independent-hpc1/check-build.py --toolchain "$ATFE/bin"
```

The output directory must be new. `build.json` records source and output hashes.
The reproducibility check builds two isolated source copies, including a path
with spaces, compares all six release artifacts and their manifest, and removes
its temporary directories. Core CI runs this build check; it does not claim a
native execution check.

For execution, use an original HP50g Stock snapshot without library 275 and the
existing Stock USB harness to transfer and evaluate `program.HP`. The program
returns decimal 124275 on the RPL stack. Preserve the baseline and resulting
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
corrupted snapshot variants must fail. Snapshots and ROM images are local inputs
and are not distributed with this fixture.

The maintained build reproduces the previously executed package SHA-256
`fbb0c00cfbfeb9d0f08e330c1c1edb9821e726fb651e0b8303b565aee305434c`.
The matching native snapshot passes this verifier. This establishes the bounded
prototype; arbitrary application loading, native-service coexistence and
cross-family resource ownership remain separate acceptance work.
