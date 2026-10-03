# zig toolchain for linux riscv64
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(ZIG_TARGET "riscv64-linux-gnu.2.27")
include("${CMAKE_CURRENT_LIST_DIR}/zig-common.cmake")
