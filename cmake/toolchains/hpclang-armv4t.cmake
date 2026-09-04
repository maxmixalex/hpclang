# hpclang ARMv4T toolchain.
#
# The application ABI is AAPCS32. C is Thumb-1 by default; assembly startup
# files may opt into ARM state explicitly.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR armv4t)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)

set(HPCLANG_COMMON_FLAGS
    "--target=arm-none-eabi -march=armv4t -mabi=aapcs -mfloat-abi=soft"
    CACHE STRING "hpclang ARM ABI flags"
)

set(HPCLANG_FREESTANDING_FLAGS
    "-ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-pic -fno-pie"
    CACHE STRING "hpclang freestanding flags"
)

set(CMAKE_C_FLAGS_INIT
    "${HPCLANG_COMMON_FLAGS} -mthumb ${HPCLANG_FREESTANDING_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT
    "${HPCLANG_COMMON_FLAGS} -mthumb ${HPCLANG_FREESTANDING_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT
    "--target=arm-none-eabi -march=armv4t -marm")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
