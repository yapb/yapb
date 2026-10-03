# zig toolchain for linux x86_64, glibc 2.17 baseline
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(ZIG_TARGET "x86_64-linux-gnu.2.17")
include("${CMAKE_CURRENT_LIST_DIR}/zig-common.cmake")
