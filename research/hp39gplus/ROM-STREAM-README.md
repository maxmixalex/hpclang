# HP39g+ one-launch USB ROM streamer

This is the experimental first-real-hardware path for obtaining the complete
**1 MiB physical NOR image** from an HP39g+.

The calculator-side payload is read-only with respect to flash: it maps NOR for
loads and contains no NOR erase/program sequence. Internally the image is
validated in 64 blocks of 16 KiB, but all blocks are streamed automatically over
USB Raw HID. You transfer the aplet once and start it once.

The PC side is a standalone Windows x64 Go executable. It uses the Windows
HID/SetupAPI stack directly and does not require Python, hidapi, Conn3x APIs, or
an extra runtime DLL.

## Before the first run

1. Make a normal backup of the calculator.
2. Use good main batteries and a good backup battery. Keep the rear reset button
   accessible.
3. Using the normal HPGCC39/Conn3x transfer route, install all four files from
   the package's `calculator/` directory:
   - `HPGCC000.000`
   - `HP39DIR.000`
   - `HP39DIR.CUR`
   - `LIB275L.000`
4. Close Conn3x / HP Connectivity Kit completely after the transfer.
5. Unplug the calculator USB cable.

## Dump the complete ROM

1. On a normal 64-bit Windows PC, open Command Prompt or PowerShell in the
   extracted package directory.
2. Start the receiver:

       hp39gplus-rom-receiver.exe hp39gplus-flash.bin

   Leave this window running. It will wait for the calculator.
3. With USB still unplugged, launch the aplet named **H39 ROM USB** on the
   calculator.
4. Plug the USB cable into the calculator.
5. The transfer runs automatically. The PC validates each 16 KiB block and
   requests a retry automatically if a block fails its byte-sum/FNV checks.
6. Wait until the PC prints **`Dump complete`**.
7. Only after that message appears, **unplug the USB cable**. Physical disconnect
   is the boundary that lets the aplet restore the saved USB/clock/interrupt
   state and return to the stock OS.

Successful output:

- `hp39gplus-flash.bin` — exactly **1,048,576 bytes**;
- `hp39gplus-flash.bin.json` — protocol metadata and the image SHA-256.

Do not edit, pad, or otherwise transform the `.bin` before using it as the
hpclang fixture.

## Stop conditions for the first hardware test

If the temporary **HP39g+ ROM Stream** HID device never appears, the calculator
freezes or resets, or the stock OS reports an error, do not repeatedly relaunch
the aplet. Unplug USB and use the rear reset button if necessary, then record
the exact behavior.

If the PC receiver aborts after the HID device is active, unplug USB. The aplet
uses physical disconnect as its safe restoration boundary.

## Windows receiver details

The receiver is built with `CGO_ENABLED=0 GOOS=windows GOARCH=amd64`. It
enumerates the Windows HID class with SetupAPI, selects VID `03f0` / PID
`0121`, verifies the `HP39g+ ROM Stream` product string and Raw HID
usage page/usage, obtains report lengths from Windows, and uses overlapped
`ReadFile` / `WriteFile` I/O.

Windows prepends a zero report-ID byte to this device's 64-byte HID payload, so
the Win32 buffers are 65 bytes while the application protocol remains 64 bytes.

The Go protocol tests deliberately corrupt one first-pass block and require an
automatic retry before accepting the final 1 MiB image.
