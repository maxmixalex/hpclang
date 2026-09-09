# HP39g+ boot-fixture research

The remaining HP39g+ E2E blocker is provenance: there is no trustworthy public
stock ARM firmware/boot fixture comparable to the HP39gs/HP40gs update
archives.

This directory is intentionally research-only but reproducible:

- `boot-dump.S` is the read-only ARM payload;
- `boot-dump.ld` links it as the flat ARM image expected by the old launcher;
- `boot-dump-wrap.py` wraps that image for ARMToolbox39 old `PrRUN`;
- `x50ng-boot-dump-main.c` is the retired emulator-side mechanism validator;
- `extract-boot-dump.py` extracts a completed real-device dump object.

None of these files participates in normal CI while HP39g+ remains blocked.

`boot-dump.S` is a deliberately read-only research payload for the historical
HPGCC39/ARMToolbox39 old `PrRUN` path. ARMToolbox requests one alignment
slop byte beyond the 0x4010-byte result, then leaves the resulting string on
the Saturn stack after the ARM function returns. The payload itself uses only
byte accesses for the returned buffer because old `PrRUN` guarantees byte,
not word, alignment. The
payload:

1. reads that output pointer from the stock Saturn/ARM state;
2. temporarily maps physical NOR address 0 through an unused high L1 section;
3. copies exactly the first 16 KiB;
4. restores the original L1 descriptor;
5. marks the returned string `H39BOOT1` + length + PASS;
6. cleans the returned range from the ARM D-cache before returning to Saturn.

It never executes a flash erase/program command. The temporary section is
non-cacheable/non-buffered and is removed before return.

`extract-boot-dump.py` scans a returned aplet/object for the completed marker
and extracts the 16 KiB block.

The emulator mechanism probe passed byte-for-byte against the loaded physical
NOR and has been removed from CI now that the result is established. This
directory is retained for the next real-hardware step.

HP39g+ E2E is blocked until a real calculator is available. When that happens,
use the validated read-only mapping approach to obtain and verify the stock
flash image, then add that fixture to the normal Direct and stock-launch E2E
gates. The current payload extracts the first 16 KiB boot block; a complete
1 MiB dump should be transferred in bounded chunks rather than buffered in RAM.

Emulator probe success is not HP39g+ firmware validation and must never be
recorded as a native HP39g+ fixture.
