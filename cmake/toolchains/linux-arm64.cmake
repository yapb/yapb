# zig toolchain for linux aarch64, glibc 2.17 baseline
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(ZIG_TARGET "aarch64-linux-gnu.2.17")
include("${CMAKE_CURRENT_LIST_DIR}/zig-common.cmake")
