# HP39g+ boot-fixture research

The remaining HP39g+ E2E blocker is provenance: there is no trustworthy public
stock ARM firmware/boot fixture comparable to the HP39gs/HP40gs update
archives.

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

The temporary emulator probe exists only to prove the mechanism against a known
loaded boot block. A successful probe is **not** HP39g+ firmware validation and
must not be recorded as a native HP39g+ fixture.
