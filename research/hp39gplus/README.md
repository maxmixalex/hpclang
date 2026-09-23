# HP39g+ boot-fixture research

The remaining HP39g+ E2E blocker is provenance: a real HP39g+ is now available,
but hpclang still needs a verified complete stock-flash image from that machine.
There is no trustworthy public HP39g+ fixture comparable to the HP39gs/HP40gs
update archives.

The supported acquisition path in this directory is the **one-launch Raw HID
streamer**. Historical first-16-KiB boot-dump files are retained only as
research evidence for the read-only NOR mapping mechanism; they are not a
supported full-ROM workflow.

The historical mapping probe established that an ARMToolbox39 payload can
temporarily map physical NOR address 0 read-only, copy bytes, restore the
original descriptor, and return without issuing any flash erase/program
sequence. Emulator success is evidence for the mapping mechanism only and must
not be recorded as native HP39g+ firmware validation.

## One-launch Raw HID streamer

This path is still **real-hardware-unvalidated**. The first physical run must
start with USB unplugged, and the cable must be unplugged again after the PC
receiver reports completion before control is returned to the stock OS.

The current acquisition path is:

- `rom-stream-entry.S` borrows five otherwise-unused high MMU sections for
  read-only NOR plus S3C2410 USB/clock/GPIO/interrupt-controller access;
- `rom-stream.c` temporarily owns the USB device controller as a polled Raw
  HID endpoint pair and streams the complete 1 MiB image without buffering it
  in calculator RAM;
- `go-receiver/` contains the native Windows x64 receiver, including protocol
  tests with a forced first-pass block corruption/retry;
- `build-rom-streamer.sh` packages one `H39 ROM USB` aplet and the standalone
  `hp39gplus-rom-receiver.exe`;
- `ROM-STREAM-README.md` is the physical-machine operating procedure.

The transfer internally validates 64 blocks of 16 KiB, but this is automatic:
the user transfers the aplet once, starts it once, and the Go receiver drives
ACK/retry for the full image. There is no 64-aplet/manual-chunk fallback.

Flash remains read-only. The temporary USB/MMU state is restored only after
physical cable disconnect.

The first physical attempts established the HPGCC39 launch sequence: START
enters the aplet view and ENTER executes the ARM action. They also exposed two
stock-state assumptions in the initial streamer. First, cable detection read
GPFDAT before selecting GPF1/EINT1. Second, HPGCC39's CRT enters slow mode
(about 3 MHz PCLK in its own source comments), while newRPL requires PCLK above
20 MHz for S3C2410 USB. The corrected streamer configures/restores GPF1/EINT1,
uses a USB-safe temporary clock/divider with all SoC IRQs masked and LCD off,
and runs its C code on 4 KiB of PrRUN-allocated scratch RAM rather than the
unknown caller stack.

Until a physical HP39g+ completes the corrected path, CI/build success proves
packaging, relocation safety, Windows receiver construction, and host protocol
behavior; it does not yet prove stock HP39g+ USB compatibility.
