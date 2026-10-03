# zig toolchain for macos intel 64-bit
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(ZIG_TARGET "x86_64-macos")
include("${CMAKE_CURRENT_LIST_DIR}/zig-common.cmake")

# gnu ar sysv tables break zig's mach-o linker, archive through zig itself
set(CMAKE_C_ARCHIVE_CREATE "zig ar qc <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_APPEND "zig ar q <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH "zig ranlib <TARGET>")
set(CMAKE_CXX_ARCHIVE_CREATE "zig ar qc <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_APPEND "zig ar q <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_FINISH "zig ranlib <TARGET>")
