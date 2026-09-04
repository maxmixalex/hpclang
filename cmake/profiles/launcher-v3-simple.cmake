# Common clean Launcher V3 simple/MMU profile.
#
# This is the target-side ABI shared by the HP39/40, HP48gII and HP49/50 clean
# backends. Calculator-side packaging/launcher installation remains a family
# responsibility.

add_compile_options(
    "$<$<COMPILE_LANGUAGE:C>:-mabi=apcs-gnu>"
    "$<$<COMPILE_LANGUAGE:C>:-mfloat-abi=soft>"
    "$<$<COMPILE_LANGUAGE:C>:-fropi>"
    "$<$<COMPILE_LANGUAGE:C>:-frwpi>"
)

set(HPCLANG_L3_MMU_LOADER
    "${CMAKE_CURRENT_LIST_DIR}/../../runtime/launcher-v3/mmu-loader.S")
set(HPCLANG_L3_PACKER
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/elf2l3.py")

set(HPCLANG_CODE_MODEL "ropi-rwpi" CACHE STRING
    "Common clean hpclang ARM code model")
set(HPCLANG_EXECUTABLE_ABI "launcher-v3-simple-mmu" CACHE STRING
    "Common native-ARM executable ABI")
