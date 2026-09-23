# HP39g+ one-launch USB ROM streamer

This is an **experimental first-real-hardware probe** for obtaining the 1 MiB
physical NOR image from an HP39g+ without manually transferring 64 objects.
It has build/static/protocol-simulation coverage, but it has not yet been
proved on a physical HP39g+.

The calculator payload is read-only with respect to flash: it maps the NOR for
loads and contains no NOR erase/program sequence. Internally it validates the
image in 64 blocks of 16 KiB, but those blocks are streamed automatically over
USB Raw HID. The user starts the aplet only once.

## Before the first run

1. Make a normal backup of the calculator.
2. Use good main batteries and a good backup battery. Keep the rear reset
   button accessible.
3. Install the four files in `calculator/` using the same HPGCC39/Conn3x route
   used for ordinary aplets. `LIB275L.000` only needs to be installed once if
   the matching library is already present.
4. Close Conn3x / HP Connectivity Kit before running the stream receiver.
5. On Windows, no runtime install is required for the preferred receiver. Use
   `hp39gplus-rom-receiver.exe`, which talks directly to the built-in Windows
   HID/SetupAPI stack and does not require Python, hidapi, or an extra DLL.

   The Python receiver remains in `tools/` as a diagnostic fallback. Its
   optional dependency file is `requirements-python-fallback.txt`.

## Dump procedure

1. **Unplug the calculator USB cable.** The aplet deliberately refuses to take
   ownership of a USB controller that is already connected to a host.
2. On a normal 64-bit Windows PC run:

   `hp39gplus-rom-receiver.exe hp39gplus-flash.bin`

   The receiver is a standalone Go executable. It waits for the temporary HID
   device through Windows `hid.dll` / `setupapi.dll`; no Conn3x API, Python,
   hidapi package, or bundled runtime DLL is used.
3. On the calculator launch **H39 ROM USB** while USB is still unplugged.
4. Plug the USB cable in.
5. The transfer now runs automatically. The PC checks every 16 KiB block and
   automatically requests a retry if a block fails its byte-sum/FNV checks.
6. When the PC prints `Dump complete`, it has already written and SHA-256
   hashed the complete 1,048,576-byte image. **Unplug the USB cable then.**
   The aplet restores the saved controller/clock/interrupt configuration and
   returns to the stock OS only after physical disconnect.

The output is:

- `hp39gplus-flash.bin` — exactly 1,048,576 bytes;
- `hp39gplus-flash.bin.json` — protocol metadata and SHA-256.

## Stop conditions for this first hardware test

If the temporary `HP39g+ ROM Stream` HID device never appears, the calculator
freezes/resets, or the stock OS reports an error, do not repeatedly relaunch the
aplet. Unplug USB and use the rear reset button if necessary, then report the
exact behavior. The older 64-package dumper remains available as a conservative
fallback and diagnostic path.

If the PC receiver aborts after the HID device is active, unplug USB. The aplet
uses physical disconnect as the safe boundary before it restores stock state.

## Windows receiver details

The preferred PC tool is built with `CGO_ENABLED=0 GOOS=windows GOARCH=amd64`.
It enumerates the system HID class with SetupAPI, selects VID `03f0` / PID
`0121`, verifies the `HP39g+ ROM Stream` product string and Raw HID usage
page/usage, reads the report lengths reported by Windows, and then uses
overlapped `ReadFile` / `WriteFile` I/O. Windows prepends a zero report-ID
byte to this device's 64-byte HID payload, so the Win32 I/O buffers are 65
bytes while the on-wire application protocol remains unchanged.

The Go protocol test and the older Python protocol simulation both deliberately
corrupt one first-pass block and require an automatic retry before accepting the
final 1 MiB image.
