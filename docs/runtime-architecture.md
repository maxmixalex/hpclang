# Runtime architecture

## Non-ROM policy

hpclang will not require calculator ROM replacement or ROM patching.

The project may support an HPGCC3-compatible ROMLIB environment when a user already has one, but the normal SDK and all core target backends must run on the calculator's ordinary supported software environment plus installable libraries/applets.

## Layering

```
application source
      |
      v
common hpclang API
      |
      +---------------- portable C / C++ ----------------+
      |                                                  |
      v                                                  v
compiler-rt / libc subset                         graphics / math / objects
      |                                                  |
      +---------------------- ABI ------------------------+
                             |
                       target runtime
                             |
             +---------------+---------------+
             |               |               |
          hp39           hp48gii          hp49/50
             |               |               |
        aplet/lib275     Toolbox48       ARMToolbox
             |               |               |
             +------ calculator OS / ARMSYS -+
```

## Shared runtime versus launcher

Historical library 275 is already shared infrastructure: multiple programs can call its launcher instead of embedding Saturn launch code in every application.

hpclang should preserve that idea.

The first shared layer should contain only platform services whose duplication is clearly wasteful or whose implementation must live in Saturn/System-RPL land:

- program discovery/alignment;
- Saturn-to-ARM transition;
- MMU/page-table construction;
- optional extra-memory acquisition;
- platform/version query.

Application-side C runtime can initially remain statically linked. Section garbage collection makes this simple and avoids prematurely designing a dynamic linker.

If a stable shared ARM service ABI later proves worthwhile, it can be implemented in an installable support library. This remains non-ROM.

## Common Launcher V3 payload

The common payload format is Launcher V3 simple/MMU.

It is deliberately narrower than the richest 2.0 format but is historically implemented across the three launcher families under study.

The ARM-side clean runtime should not assume a concrete Saturn memory map. Instead a family-specific entry shim translates launcher state into a common C structure before entering the common runtime.

Conceptually:

```c
struct hp_launch_context {
    void *saturn_state;
    void *mmu_table;
    unsigned mmu_pages;
    void *os_ram;
    unsigned os_ram_size;
    unsigned capabilities;
};
```

The exact ABI is not frozen yet. It will be defined only after the three historical launcher entry conventions are compared field-by-field.

## Target runtime split

The current `runtime/hp39gs` code is a bootstrap implementation, not the final directory architecture.

The desired split is:

```
runtime/
  common/
    startup/
    memory/
    compiler-support/
  armv4t/
    cache/
    cpu/
    mmu/
  targets/
    hp39/
    hp48gii/
    hp49/
platform/
  hp39/
    aplet/
    launcher/
  hp48gii/
    rpl/
    launcher/
  hp49/
    rpl/
    launcher/
  hpgcc3/
    romlib-compat/
```

Code moves into `common` only after at least two target families demonstrate the same behavior.

## HPGCC 2.0 compatibility

The 49/50 backend should eventually support both:

1. common Launcher V3 simple/MMU payloads;
2. Launcher V3 v1.2 virtual-code payloads.

Virtual-code mode fixes code at 0x01220000 and the runtime data area around 0x01101000. It is an optimization/compatibility profile, not a requirement imposed on HP39/48.

## HPGCC3 compatibility

HPGCC3 separates tiny application loaders from a large shared runtime. Its launcher/runtime source provides useful design patterns:

- versioned runtime requirement;
- startup options;
- centralized save/restore of LCD, clocks, timers, keyboard and exceptions;
- system/master global tables;
- object API isolation through `sat3`.

hpclang should reproduce these architectural benefits in source and installable-library form where useful.

No common hpclang application will depend on ROMLIB calls unless it explicitly selects an HPGCC3 compatibility profile.
