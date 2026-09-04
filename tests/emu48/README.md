# Emu48 headless E2E harness

This directory contains the controller side of hpclang's Emu48/Emu48+ E2E
tests. It deliberately does not automate GUI coordinates.

## Dependency boundary

CI downloads the official Emu48+ Service Pack 67 binary and loads its DLL
debugger API dynamically. Emu48 remains an external GPL test dependency; no
Emu48 implementation code is linked into hpclang's runtime or SDK.

The controller requires existing Emu48 DLL APIs for document/state creation,
calculator model query, key injection, register/mapped-memory reads and
run/stop control.

## Headless meaning

Emu48's Win32 GDI backend still owns an HWND internally. The controller moves
that window completely off-screen and never sends mouse input or depends on
screen coordinates. This keeps Emu48's mature display implementation while
making the test unattended and deterministic.

A state-backed capture uses PrintWindow(PW_CLIENTONLY) to snapshot the
off-screen calculator client. The BMP is converted to a deterministic PNG by
tools/emu48-bmp-to-png.py.

## Current gate

The Emu48 headless harness workflow currently proves:

1. the pinned Emu48+ DLL loads from an x86 controller;
2. the debugger/key/memory APIs needed by E2E are present;
3. the screenshot path produces a deterministic PNG artifact.

This is infrastructure validation, not yet a calculator execution pass.

The HP39gs job becomes authoritative only when the remaining path is connected:

    known HP39gs Emu48 state
      -> key injection to APLET/RECV
      -> in-memory serial peer
      -> HP39 transfer protocol
      -> generated HPGCC000.000
      -> launch HPCLANG HPC1 TEST
      -> machine-readable RUNTIME PASS
      -> screenshot
      -> clean return to stock OS

The serial endpoint will be a narrow test-only patch at Emu48's host transport
boundary. Calculator UART behavior remains unchanged.
