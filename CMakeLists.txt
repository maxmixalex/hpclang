cmake_minimum_required(VERSION 3.20)
project(hpclang LANGUAGES NONE)

# Repository tests are intentionally separate projects:
#   cmake -S tests/unit -B build/unit
#   tools/build-e2e.sh
# Installing the source SDK does not require a host or cross compiler.
include(cmake/install-sdk.cmake)
