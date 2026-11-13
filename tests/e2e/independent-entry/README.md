# Independent HP50g entry fixture

This fixture builds a Stock RPL program that enters ARM code without
ARMToolbox/library 275, calls freestanding C using a program-owned 4 KiB stack
and 536-byte workspace, restores the saved ARM context, and returns to RPL.
The program then pushes the exact integer 124275. It does not load HPC1 apps.

Build with the project's ATFE 22.1.0 toolchain (LLVM tools in `bin`):

```sh
python3 tests/e2e/independent-entry/build.py \
  --toolchain /path/to/atfe/bin --output /tmp/independent-entry
```

The output directory must not exist. `program.HP` is the transfer artifact;
`probe.elf`, `probe.bin` and `metadata.json` describe its ARM payload and reserved
storage. `build.json` records compiler identity and source/artifact SHA-256s.
No research scripts, ROM files or installed HPGCC objects are build inputs.

The Saturn encoder uses HP50g SAVE/RESTORE addresses and the SRAM physical
mapping. It copies the ARM payload into aligned storage reserved inside the
program object. IRQ and FIQ are masked during the C call; the saved CPSR and
registers are restored on return. This is a bounded synchronous experiment,
not a service-coexistence or interrupt-latency qualification.

Current validation: rebuilt payload and transfer program match the historical
experiment byte for byte. Rechecking its saved HP50g state with the rebuilt
artifact confirms the 128 workspace values/checksum, stack alignment/bounds,
unchanged code/guards, object containment, return integer, absent library 275
and unchanged Flash. This is archived-state replay, not a fresh emulator run.

Before release use, add a maintained snapshot verifier and fresh Stock run.
Full independent-launcher acceptance additionally requires HPC1 integration,
allocation/MMU and native-service coexistence, other model families including
the HP39 Aplet path, and an explicit packaging/migration decision. Do not infer
those capabilities or hardware safety from this entry fixture. The existing
application launch paths remain required.
