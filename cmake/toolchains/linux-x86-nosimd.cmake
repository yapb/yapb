# zig toolchain for linux i386 without simd, glibc 2.17 baseline
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86)
set(ZIG_TARGET "x86-linux-gnu.2.17")
include("${CMAKE_CURRENT_LIST_DIR}/zig-common.cmake")
