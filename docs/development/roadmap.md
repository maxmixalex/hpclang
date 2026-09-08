# Roadmap

Issue [#1](https://github.com/zyf722/hpclang/issues/1) is the live development board. This page records durable milestones and completion criteria.

## Completed foundations

The following architecture is established:

- one application ABI: HPC1 / hpclang v1;
- ARMv4T, AAPCS32, Thumb-1 application code, and soft float;
- one common runtime with compiler builtins, picolibc integration, static TLS, constructors, heap, and normal C/C++ linkage;
- one common bootstrap and fixed virtual layout;
- three platform families: `hp39`, `hp48`, and `hp4950`.

## Platform execution

HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g pass Direct runtime E2E and their original-system launch E2E.

HP 39gs also passes the canonical runtime and stock return path on real hardware.

### HP 39g+

**Blocked on real hardware.**

The HP39 package and read-only flash-dump mechanism are complete. Resume this work after acquiring a real HP 39g+:

1. dump and verify its complete stock flash;
2. establish the native emulator fixture;
3. add HP 39g+ to Direct runtime E2E;
4. automate the original-system installation/launch path available on that model.

No model-specific hpclang runtime fork is planned.

## Common platform services

After the execution layer is stable, add common services only when required by applications:

- display;
- keyboard;
- timers;
- storage;
- calculator object/stack integration.

## SDK stabilization

The next broader milestone is a coherent public SDK: stable headers, build UX, examples, and application-facing documentation. C++ exceptions/RTTI and broader POSIX/libc hooks remain optional and should be added only when a real application justifies them.
