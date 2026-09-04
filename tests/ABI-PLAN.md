# HPGCC ABI probe plan

The smoke test only proves that current stock Clang can emit suitable ARMv4T objects. Full HPGCC compatibility requires binary-interface probes.

## HPGCC 1.x / 2.x probe matrix

Build each case once with the historical GCC and once with modern Clang, then cross-link where meaningful.

1. scalar calls: 8/16/32-bit arguments and returns;
2. 64-bit integer arguments, returns and compiler helpers;
3. soft-float argument/return convention;
4. small and large struct arguments/returns;
5. function pointers and callbacks;
6. global and static data access;
7. calls from modern object -> historical HPGCC library;
8. callbacks from historical HPGCC library -> modern object;
9. preservation and meaning of r10/sl across those boundaries;
10. startup/global initialization and BSS;
11. interrupt/cache-sensitive helpers.

## 39gs rule

The current golden path keeps historical `MMUloader.o` and `MMUglobals.o`. Any attempt to replace them must run the same aplet on real 39gs hardware before the old objects are retired.

## HPGCC3 probe matrix

Treat HPGCC3 separately. Verify:

- CC3/EXT3 header encoding;
- required ROMLIB revision;
- `__launch_loader` and `__launch_startup` ABI;
- ROM globals;
- program memory flags;
- old/new compiler interoperability with ROMLIB call stubs.

A successful HPGCC3 test does not imply HPGCC1/2 or HP39 compatibility.
