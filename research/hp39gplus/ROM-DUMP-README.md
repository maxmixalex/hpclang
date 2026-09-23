# HP39g+ physical ROM dumper

This bundle extracts the **1 MiB physical NOR flash** from a real HP 39g+
in 64 independent 16 KiB chunks.

The calculator-side payload is deliberately read-only. It temporarily maps
physical NOR address 0 through the same unused high L1 section used by the
previous validated 16 KiB probe, reads one 0x4000-byte range, restores the
original descriptor, publishes the result back to Saturn, and returns through
the historical HPGCC39 / ARMToolbox39 old `PrRUN` path. It never issues a
flash erase/program command.

## Before starting

- Make a normal calculator backup first.
- Use fresh main batteries; keep the backup battery installed.
- Have the rear reset button accessible.
- Install/use this only on the HP 39g+ for the first real-hardware pass.
- Start with package `00`. If it does not return normally, stop and report
  exactly what the calculator displayed/did; do not continue through all 64.

## One chunk

Each directory under `packages/` is named `00` through `3F`.

For chunk `00`:

1. Transfer `packages/00/HP39DIR.CUR`, `HP39DIR.000`,
   `HPGCC000.000`, and `LIB275L.000` with Conn3x / the HP connectivity
   software, using the same transfer procedure as an HPGCC39 aplet.
2. Launch the aplet named **H39 ROM 00** once.
3. It should return to the stock OS. The 16 KiB result is stored back into the
   aplet's TopicVar1 by the stock HPGCC wrapper.
4. Transfer/export that aplet back to the PC and keep the returned
   `HPGCC000.000` as, for example, `returned-00.000`.
5. Verify/extract it:

       python tools/extract-rom-dump.py returned-00.000 chunk-00.bin

The script accepts the chunk only when:
- the returned `H39ROM01` marker is complete;
- the embedded physical offset is 16 KiB aligned and inside the 1 MiB flash;
- the returned checksum satisfies `status XOR byte_sum == 0x50415353`.

After chunk 00 works, repeat for `01` through `3F`. LIB275L only needs to
be installed once if your transfer software preserves it; transferring the
four-file package each time is the conservative path.

## Assemble the full flash

Put all returned aplet files in one directory (names do not matter) and run:

    python tools/collect-rom-dump.py returned hp39gplus-flash.bin

The collector recursively scans the directory, rejects conflicting duplicate
chunks, requires all 64 physical offsets exactly once, writes the 1 MiB image,
and also writes `hp39gplus-flash.bin.json` with the full-image SHA-256 and
per-chunk hashes.

Do not edit or pad the resulting image before sending it back for hpclang
fixture analysis.
