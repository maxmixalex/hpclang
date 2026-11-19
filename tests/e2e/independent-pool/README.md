# Independent Stock page allocation experiment

This HP50g fixture evaluates KOS-owned memory outside the transferred program.
It retains the independent entry/return path without library 275 and makes no
temporary HPC1 mappings. It is not a general launcher or allocator API.

The original HPGCC 2.0 `kos/kos_mem.c` and `syscall.h` identify service 201 with
heap selector `0x100` and byte count, and service 101 with the allocation pointer.
The probe uses that calling contract with the original Stock SWI vector. Calls
run in the original Stock mappings and interrupt state; the shared entry owns
the temporary stack and restores caller registers on return.

```sh
python3 tests/e2e/independent-entry/build.py --toolchain "$ATFE/bin" \
  --probe-source tests/e2e/independent-pool/probe.c --output /tmp/pool-build
python3 tests/e2e/independent-pool/verify.py --artifact /tmp/pool-build \
  --baseline /path/to/baseline --state /path/to/result/state \
  --report /tmp/pool-result.json
```

Use the independent-entry Stock USB procedure and its original HP50g RPN
baseline without library 275. The shared verifier checks the actual RPL return,
code and stack guards, owned entry storage, caller control state, SWI vector and
unchanged Flash. The pool verifier checks page geometry, identity mapping,
page-end write/read results, release status and reuse after allocation failure.

On 2026-11-14 the five requests produced 8, 16, 0, 0 and 8 usable 4 KiB pages.
Each successful request includes 4,095 alignment bytes; the failed requests
were 135,167 bytes and 1 MiB. Both successful blocks were released, and the
final small request reused the initial pointer. The 5,688-byte probe returned
normally. This is one original-ROM emulator baseline, not a universal free-heap
bound or physical-device result. HP48 and HP39 are not qualified by this run.

The current HP50g Notebook build needs at least 32 production pool pages before
additional heap headroom. Thus this baseline does not support a KOS-only pool
for that application. Further work must combine an owned RPL memory source or
another demonstrated allocation strategy with Stock service fences and cleanup.
Production launchers remain in use.

## Paused RPL page borrow

`rpl-probe.c` uses the original ARMSYS Saturn state, enabled with the explicit
`--stock-state` build switch. The default entry package remains byte-identical.
It reads the native RSKTOP/DSKTOP bounds and live Read map, retains 4 KiB below
and 256 bytes above the candidate pool, validates whole unique 4 KiB pages,
and tests/restores their first and last words. Stock stays suspended and IRQ/FIQ
remain masked throughout; no native service or allocator runs during the borrow.
The RPL bounds are checked unchanged before returning.

```sh
python3 tests/e2e/independent-entry/build.py --toolchain "$ATFE/bin" --stock-state \
  --probe-source tests/e2e/independent-pool/rpl-probe.c --output /tmp/rpl-pool
python3 tests/e2e/independent-pool/verify.py --rpl --artifact /tmp/rpl-pool \
  --baseline /path/to/baseline --state /path/to/result/state \
  --report /tmp/rpl-pool-result.json
```

A fresh HP50g original-ROM execution on 2026-11-14 borrows/restores 28 pages
(112 KiB), with entry/return checks and ten corrupted-report negatives passing.
The verifier independently checks each reported page against the saved Read map.
The package is 5,904 bytes before its RPL wrapper/reserve. Evidence:
`/work/nov14-independent-rpl-pool-live/verified.json`.

This establishes a paused-borrow path, not a lease across native object calls.
Do not add these 28 pages to the separate KOS run and claim a Notebook budget:
the full application changes RPL occupancy, and combined ownership/fences still
need qualification. No raw free gap may remain borrowed while Stock allocation
or garbage collection resumes without the corresponding service contract.
