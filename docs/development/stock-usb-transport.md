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
The separate Stock Gallery runners still use UART for HP39gs/40gs/48gII and
SD for HP50g. Their results validate regression behavior, not USB transport;
the profile default does not change those dedicated runners.
Repeated-run flake-rate and wall-clock comparison remains open: selecting USB
in the profiles is an implementation fact, not evidence that U5 is complete.

## Implementation and evidence

Upstream pinned x50ng supplies only a register stub. `tools/prepare-x50ng.py`
now integrates `tests/e2e/harness/x50ng/hpclang-usbdev-host.c`: bus reset,
guest-descriptor EP0 enumeration, indexed endpoint FIFO/CSR behavior, endpoint
interrupts, EP1 IN, EP3 OUT, SOF, and a PTY byte-stream bridge. The host USB
unit test covers enumeration, bulk transfers, AUTOSET/AUTOCLR, SOF, and reset.
This is a headless emulator transport, not host OS USB passthrough.

Runtime [#458](https://github.com/zyf722/hpclang/actions/runs/36149460082),
at `53e2b653`, passes all Direct families and Stock HP39g+/39gs/40gs/48gII.
Stock HP49g+/50g fails before transfer; the investigation below identifies
the incorrect UI sequence. Storage #207 passes on the same head using SD,
so it cannot establish USB correctness. Gallery also needs a fresh regression
run before cross-workflow closure is claimed.

Formal Runtime [#459](https://github.com/zyf722/hpclang/actions/runs/36286455537)
on `d2897cdb` is fully green: Build, universal x50ng, shared Stock assets, all
six Direct models, all six USB Stock models, and evidence publication. Both
HP49g+ and HP50g record `status=pass`, `saw_pass=true`, `returned=true`, and
`return_settled=true`.

Storage [#208](https://github.com/zyf722/hpclang/actions/runs/36286455562) and
Quality [#107](https://github.com/zyf722/hpclang/actions/runs/36286455548) pass
on the same head. Gallery
[#150](https://github.com/zyf722/hpclang/actions/runs/36286481830) passes every
Stock job and Direct hp48/hp4950. Direct hp39 logs complete PASS for HP39g+
(247 faces / 494 captures), HP39gs and HP40gs (248 / 496 each), each across
20/20 shards, but Actions cancels the job at the old 15-minute ceiling.
The HP39 family now runs three models sequentially, so its orchestration
budget is raised to 25 minutes; other families retain 15 minutes. A new
Gallery run is required before calling that workflow fully green.

## HP49g+/50g failure diagnosis

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

U3/U4 are protocol gates: HP39 Conn3x transfer and HP48/49/50 Kermit transfer
must install and launch the same packaged HPC1 payload through the original
Stock UI, then pass the existing payload, teardown and Stock-repaint witnesses.
