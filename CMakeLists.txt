cmake_minimum_required(VERSION 3.20)
project(hpclang LANGUAGES C ASM)

add_library(hpclang_smoke OBJECT tests/smoke.c)
target_compile_options(hpclang_smoke PRIVATE
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -ffunction-sections
    -fdata-sections
)
