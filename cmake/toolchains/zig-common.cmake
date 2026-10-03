# shared body for the zig cc/c++ toolchains: compiler plus no-run checks.
# Each toolchain sets CMAKE_SYSTEM_NAME, CMAKE_SYSTEM_PROCESSOR and
# ZIG_TARGET (also read by the root CMakeLists), then includes this file.
set(CMAKE_C_COMPILER zig cc -target ${ZIG_TARGET})
set(CMAKE_CXX_COMPILER zig c++ -target ${ZIG_TARGET})
# never try to run target binaries during compiler checks
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
