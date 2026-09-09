cmake_minimum_required(VERSION 3.20)
project(hpclang LANGUAGES C CXX ASM)

# Repository tests are intentionally separate projects:
#   cmake -S tests/unit -B build/unit
#   tools/build-e2e.sh
# The SDK itself is consumed through cmake/toolchains and cmake/profiles.
