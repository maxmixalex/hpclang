# Library lineage

A filename-level comparison of the HPGCC 1.1 hplib tree and HPGCC3 R004.revA shows substantial continuity rather than a clean rewrite.

## Source-tree overlap

Using C/assembly/header files under:

- HPGCC 1.1: `sources/hplib/`;
- HPGCC3 R004.revA: `hplib/`;

the current inventory finds:

- 256 source/header files in the 1.1 tree;
- 278 in the HPGCC3 hplib tree;
- 129 paths retained with the same relative filename;
- 127 paths present only in the 1.1 layout;
- 149 paths present only in the HPGCC3 layout.

The retained paths are concentrated in long-lived library areas:

| Area | Same relative paths |
| --- | ---: |
| math | 34 |
| sys | 25 |
| strings | 25 |
| stdio | 23 |
| kos | 11 |
| fileio | 9 |
| keyboard | 2 |

This makes source/API migration more attractive than inventing a new calculator C library from scratch.

## Important HPGCC3 additions/reorganizations

### saturn -> sat3

The old `saturn/` layer is replaced by a much larger `sat3/` API.

HPGCC3 describes `SAT_OBJECT` as a common representation for calculator objects and adds higher-level creation/access for:

- integers/reals/complex;
- strings and identifiers;
- lists, arrays and matrices;
- directories;
- stack operations;
- allocation/free;
- object copying and memory movement.

This is a strong candidate for the modern compatibility API on 49/50. Other families need adapters where their OS object model differs.

### extreal

HPGCC3 adds an extended-real module with conversions, arithmetic and base helpers. It should be evaluated as portable library source independently from ROMLIB.

### arm runtime split

Functions that were mixed into old `kos`, `fileio` or startup code are reorganized under `hplib/arm/`, including:

- interrupt/cache helpers;
- syscall wrappers;
- setjmp;
- exit handling;
- exception machinery.

This is useful architectural guidance for hpclang's common ARMv4T layer.

### filesystem/KOS growth

HPGCC3 adds many filesystem and SD-oriented KOS wrappers: directory enumeration, disk status/free-space functions, rename/attributes and RTC functions.

These APIs must be capability-gated because not every target family exposes the same OS/storage facilities.

### startup policy

HPGCC3 `__launch_startup` centralizes save/restore for:

- interrupt state;
- LCD;
- CPU clock;
- exception handlers;
- timers;
- keyboard;
- console.

This is a useful model for hpclang common startup even without ROMLIB.

## Migration rule

For each module, prefer the newest implementation that can be made independent of a newer platform ABI.

Classify source as:

1. **portable** — migrate once into common;
2. **ARM-hardware** — share in armv4t with capability/config inputs;
3. **calculator-OS** — target-family adapter;
4. **ROMLIB-specific** — HPGCC3 compatibility backend only;
5. **legacy ABI** — retain only for old-binary compatibility tests.

This lets hpclang absorb HPGCC 2/3 improvements without making later platform assumptions mandatory on older targets.
