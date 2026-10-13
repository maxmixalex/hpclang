cmake_minimum_required(VERSION 3.20)
project(hpclang_hello LANGUAGES C CXX ASM)
find_package(hpclang CONFIG REQUIRED)

hpclang_add_application(hello_c main.c)
hpclang_add_application(hello_cpp main.cpp)
target_compile_features(hello_cpp PRIVATE cxx_std_23)
foreach(target IN ITEMS hello_c hello_cpp)
    hpclang_target_launchers(${target})
    hpclang_target_font(${target}
        NAME hello_font FONT @mini MODE STATIC_AUTO FALLBACK NONE)
endforeach()
