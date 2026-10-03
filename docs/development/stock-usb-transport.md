# Common Stock USB transport

## Scope

hpclang's supported ARM calculators share the S3C2410 USB-device block and the
historical HP USB identity `03f0:0121`. The Stock E2E harness should therefore
model one USB device/host transport and keep family-specific application
protocols above that transport.

The intended layers are:

1. x50ng S3C2410 UDC model: bus reset, EP0 control transfers, endpoint FIFO/CSR
   behavior, endpoint/USB interrupts, EP1 bulk IN and EP3 bulk OUT;
2. a deterministic headless host bridge that exposes a byte stream while keeping
   USB packetization inside the emulator;
3. HP39-family typed-file Kermit adapter (the Conn3x transfer convention);
4. HP48gII/49g+/50g Kermit adapter over the same USB byte stream.

All six target profiles already select `stock_transport: usb`. UART and
SD/VVFAT remain explicit secondary paths; Storage overrides the default to SD.
Stock Gallery retains UART for HP39gs/40gs/48gII. HP50g Gallery now reuses
the canonical USB runner and appends only its interactive face-selection steps.
Its USB path explicitly selects RPN through Stock Kermit before installation.
The other dedicated Gallery runners remain secondary-transport evidence.
Repeated-run flake-rate and wall-clock comparison remains open: selecting USB
in the profiles is an implementation fact, not evidence that U5 is complete.

## Implementation and evidence

Upstream pinned x50ng supplies only a register stub. `tools/prepare-x50ng.py`
now integrates `tests/e2e/harness/x50ng/hpclang-usbdev-host.c`: bus reset,
guest-descriptor EP0 enumeration, indexed endpoint FIFO/CSR behavior, endpoint
interrupts, EP1 IN, EP3 OUT, SOF, and a PTY byte-stream bridge. The host USB
unit test covers enumeration, bulk transfers, AUTOSET/AUTOCLR, SOF, and reset.
This is a headless emulator transport, not host OS USB passthrough.

The common USB transport is implemented and passes canonical original-Stock
launch on HP 39g+, HP 39gs, HP 40gs, HP 48gII, HP 49g+ and HP 50g.
Direct runtime, Storage, Core, Quality, and Font Gallery all pass on integration
historical head `fdcd38b7`, including explicit RPN setup, keyboard frame synchronization,
and the harness timing fixes. See
[Validation](validation.md) for the historical records and current main `d1dd4688` acceptance. That historical Gallery acceptance used dedicated UART/SD launchers. The
current HP50g Gallery USB change is recorded separately in validation. The HP39 Direct Gallery job has a 25-minute budget for its
three sequential model sweeps; all other Direct Gallery families retain 15
minutes.

The implementation is functionally complete. U5 is a separately tracked,
non-blocking repeated-run reliability and wall-clock benchmark against the
secondary transport paths, not an unfinished USB feature or a prerequisite
for integration. The formal acceptance links are kept in
[Validation](validation.md).

## HP39 Kermit turnaround

The HP39-family server uses C-Kermit's `SET SEND PAUSE 10` to leave 10 ms
before each reply. Stock can still be completing its send/receive transition
when the host has already parsed the request checksum. Immediate replies can
reach the guest USB FIFO yet fail to advance the Stock Kermit exchange,
causing repeated directory requests and an eventual lazy-fetch timeout before
the payload is installed. The pacing applies to the shared HP39 server on
both USB and UART; it does not alter the USB controller or the UI sequences.
See [C-Kermit's packet-pacing command](https://www.kermitproject.org/ck10commandref.html).

## HP49g+/50g failure diagnosis

A later main regression exposed a third assumption: a timed MODE/NEG/F6
sequence could leave Stock in ALG mode while the harness continued as if RPN
were selected. The USB runner now explicitly executes `-95 CF -95 FC?` through
Stock Kermit after transfer. This configures the documented system flag and
returns its cleared-state witness. Remote-command and transfer errors fail the
sender before releasing the gate. UI package installation and PrRUN remain the
original Stock path.


There were two incorrect UI assumptions in `hp4950-stock-usb-run.seq`:

1. Fresh Stock state already selects USB/Kermit. Opening CHOOS highlights USB;
   the extra DOWN wraps to IRDA. The firmware then closes USB and opens IRDA,
   leaving incoming host USB bytes without a Stock reader. The fix accepts
   the existing USB selection and captures both the chooser and configured
   port for later inspection.
2. SETUP attaches ARMToolbox to HOME. After warmstart, the library menu exposes
   `ARMTo` on F1, followed by the port selectors. The old F3/F4 path entered an
   empty menu instead of ARMToolbox. The corrected sequence selects F1 and
   then launches PrRUN.

Screenshots established both mistakes. A bounded guest GDB investigation also
observed I/O device `0x1` being closed and device `0x100` opened after the IRDA
selection. The USB close path (`0xecbc`, caller `0x1b40c`) clears the receive
gate at `0x08000cf9`; this was a consequence of changing ports, not proof of an
endpoint-controller defect. No firmware-byte or runtime patch is needed.

The temporary CPU tracing, ROM dump, and receive-gate watchpoint have been
removed. Generic `HPCLANG_X50NG_USB_TRACE` and
`HPCLANG_X50NG_USB_TRACE_EVENTS` remain opt-in diagnostics. The runner also
removes prior target captures/results before each run so a retained build
directory cannot supply an old server-ready image or stale evidence.

## Reproduction

Use the pinned x50ng and Arm toolchain from the repository CI setup actions,
build the canonical payload with `tools/build-e2e.sh runtime`, prepare the
shared Stock assets, then run:

```sh
tests/e2e/harness/run-stock.sh hp49gplus build/e2e-runtime build/e2e-stock-assets
tests/e2e/harness/run-stock.sh hp50g build/e2e-runtime build/e2e-stock-assets
```

The runner requires the same runtime-library/tool paths and `RUNNER_TEMP` as
CI.

## Acceptance gates

U1 requires guest-descriptor enumeration and bidirectional EP1/EP3 traffic.
The host-unit test and the Stock Kermit flows exercise these layers; a separate
real-calculator loopback is not claimed.

U2 is closed when the host side sees a deterministic byte stream with USB packet
boundaries hidden below the adapter.

U3/U4 are closed protocol gates: HP39 typed-file transfer and HP48/49/50 Kermit
over USB install and launch the same packaged HPC1 payload through the original
Stock UI, with payload, teardown, and Stock-repaint witnesses. U5 measures
repeatability and elapsed time separately; its open status does not imply
unfinished USB transport implementation.


## Interactive payloads on the common HP49/50 runner

`HPCLANG_E2E_PAYLOAD_SEQUENCE` optionally names a sequence suffix appended after
the canonical USB install/launch sequence. Its timestamps use the same clock
reset by the host-transfer gate. Font Gallery supplies face captures and exit
keys through this suffix; it does not duplicate connection, mode selection,
library installation or launch steps. Runtime leaves the suffix unset.
