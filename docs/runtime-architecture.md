# Runtime architecture

## Layers

```
application
    |
hpclang public C/C++ API
    |
common runtime / libc / compiler-rt
    |
hpclang v1 boot/service boundary
    |
+-------------+-------------+
|             |             |
HP39/40      48gII        49g+/50g
platform     platform      platform
|             |             |
calculator OS / ARMSYS / hardware
```

## Common application runtime

The common runtime owns normal language/runtime concerns:

- C/C++ startup;
- initialized data and BSS;
- compiler builtins;
- heap allocator;
- libc primitives;
- common graphics/math utility code;
- target-neutral API dispatch.

It is compiled once against the hpclang v1 ABI.

## Platform backend

A platform backend owns only calculator-specific work:

- locating the HPC1 object;
- physical-memory discovery;
- MMU page-table construction;
- boot-page initialization;
- stack placement;
- Saturn-to-ARM transition;
- state save/restore required by that calculator OS;
- native package/storage format;
- low-level display, keyboard, timer, storage and object services.

The goal is not to preserve historical ARMToolbox APIs. The goal is to provide
the same hpclang services on every target.

## Launcher implementation direction

The final launcher should be small.

Where practical, family-specific Saturn/System-RPL code should do only the work
that genuinely must happen before ARM execution. Common launch logic should be
implemented once in ARM code with small family adapters.

The existing historical Launcher-V3 implementation is therefore treated as a
hardware specification and bootstrap aid, not as the shape of the final
launcher.

## Shared support code

An installable support library/aplet is acceptable and likely desirable.
Requiring ROM modification is not.

A shared support component may contain:

- the family Saturn entry shim;
- common ARM launcher code;
- platform adapter tables;
- OS-call thunks.

Applications themselves remain ordinary HPC1 images.
