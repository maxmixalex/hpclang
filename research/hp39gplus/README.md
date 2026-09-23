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

## Full real-hardware dumper

Now that a real HP39g+ is available, the retained 16 KiB mapping mechanism is
also packaged as a conservative full-flash acquisition tool:

- `rom-dump.S` keeps the same 16 KiB/result-buffer geometry and changes only
  a compile-time physical offset;
- `build-rom-dumper.sh` builds 64 fixed-offset aplets covering the complete
  1 MiB NOR without buffering the full image in calculator RAM;
- `extract-rom-dump.py` validates/extracts one returned chunk;
- `collect-rom-dump.py` requires and assembles all 64 chunks;
- `ROM-DUMP-README.md` is the real-device operating procedure.

This remains read-only research code until the first physical-machine dump is
obtained and checked.

## One-launch Raw HID streamer

This path is still **real-hardware-unvalidated**. The first physical run must start with USB unplugged, and the cable must be unplugged again after the PC receiver reports completion before control is returned to the stock OS.

The preferred real-device experiment is now the one-launch USB path:

- `rom-stream-entry.S` borrows five otherwise-unused high MMU sections for
  read-only NOR plus S3C2410 USB/clock/GPIO/interrupt-controller access;
- `rom-stream.c` temporarily owns the USB device controller as a polled Raw HID
  endpoint pair and streams the entire 1 MiB image without buffering it in RAM;
- `receive-rom-stream.py` validates every 16 KiB block, requests automatic
  retries, writes the complete image, and records SHA-256 metadata;
- `build-rom-streamer.sh` packages one `H39 ROM USB` aplet plus the PC receiver;
- `test-rom-stream-receiver.py` runs an offline 1 MiB protocol simulation and
  deliberately corrupts one first-pass block to exercise retry handling;
- `ROM-STREAM-README.md` documents the physical plug/unplug ownership boundary
  used for the first real-hardware probe.

The transport still uses 64 internal validation blocks, but the user does not
manually transfer or launch them. The calculator is started once and the PC
receiver drives ACK/retry automatically. Flash remains read-only; the temporary
USB/MMU state is restored only after physical cable disconnect.

Until a physical HP39g+ completes this path, CI/build success is evidence only
for packaging, relocation safety, and host protocol behavior—not proof of stock
HP39g+ USB compatibility.
