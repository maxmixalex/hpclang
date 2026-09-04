# Common compiler-only baseline for HPGCC 1.x/2.x ARMv4T targets.
#
# Calculator-specific runtime objects, linker scripts and packaging belong to
# target profiles; do not hide them in this generic toolchain file.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR armv4t)

set(CMAKE_C_COMPILER clang)
set(CMAKE_ASM_COMPILER clang)

set(HPCLANG_TARGET_FLAGS
    "--target=arm-none-eabi -march=armv4t -marm"
    CACHE STRING "Common hpclang ARM target flags"
)

set(HPCLANG_FREESTANDING_FLAGS
    "-ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables"
    CACHE STRING "Common hpclang freestanding flags"
)

set(CMAKE_C_FLAGS_INIT "${HPCLANG_TARGET_FLAGS} ${HPCLANG_FREESTANDING_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "--target=arm-none-eabi -march=armv4t -marm")

# Bare-metal try_compile must not attempt to link a host-style executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
