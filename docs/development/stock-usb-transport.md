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
3. HP39-family Conn3x/typed-transfer adapter;
4. HP48gII/49g+/50g Kermit adapter over the same USB byte stream.

Serial HP39/40 and SD/VVFAT gates remain useful secondary regressions. USB is
not allowed to replace them until repeated Stock runs demonstrate lower flake
rates and equivalent or stronger return-to-Stock evidence.

## Current x50ng gap

Pinned x50ng currently models `s3c2410_usbdev` as register storage only. It
does not implement host-visible EP0 transactions, endpoint FIFO semantics,
packet completion, endpoint interrupts, or a host transport.

U1 starts by adding opt-in UDC write tracing through
`HPCLANG_X50NG_USB_TRACE`. The trace is test-only instrumentation and is
disabled by default. Its purpose is to record the exact Stock-ROM programming
sequence before implementing semantics.

## Acceptance gates

U1 is closed only when a Stock ROM can enumerate through guest-owned descriptors
and complete a bulk loopback over the same EP1/EP3 endpoints used by the real
calculator.

U2 is closed when the host side sees a deterministic byte stream with USB packet
boundaries hidden below the adapter.

U3/U4 are protocol gates: HP39 Conn3x transfer and HP48/49/50 Kermit transfer
must install and launch the same packaged HPC1 payload through the original
Stock UI, then pass the existing payload, teardown and Stock-repaint witnesses.
