# hpclang toolchain: Arm Toolchain for Embedded, Armv4T multilib.
#
# Put ATfE's bin directory on PATH before configuring CMake.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR armv4t)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)

set(HPCLANG_ARCH_FLAGS
    "--target=arm-none-eabi -march=armv4t -mfpu=none -mfloat-abi=soft -mabi=aapcs"
    CACHE STRING "hpclang Armv4T ABI flags"
)

set(HPCLANG_RUNTIME_FLAGS
    "-fno-exceptions -fno-rtti -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-pic -fno-pie"
    CACHE STRING "hpclang runtime flags"
)

set(CMAKE_C_FLAGS_INIT
    "${HPCLANG_ARCH_FLAGS} -mthumb ${HPCLANG_RUNTIME_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT
    "${HPCLANG_ARCH_FLAGS} -mthumb ${HPCLANG_RUNTIME_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT
    "--target=arm-none-eabi -march=armv4t -mfpu=none -mfloat-abi=soft -mabi=aapcs -marm")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
