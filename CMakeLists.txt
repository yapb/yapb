#
# YaPB, based on PODBot by Count Floyd
# Maintained by YaPB Team <yapb@jeefo.net>
#
# SPDX-License-Identifier: Unlicense
#
# local builds (no zig needed for native):
#   cmake --preset release && cmake --build --preset release
# cross builds need zig on path, one preset per target, e.g.:
#   cmake --preset ci-linux-x86 && cmake --build --preset ci-linux-x86
# see cmakepresets.json for the full matrix (mirrors ci)
#
cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(yapb VERSION 4.8 DESCRIPTION "Yet Another PodBot" HOMEPAGE_URL "https://yapb.github.io/" LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(YAPB_64BIT "Build as 64-bit binary" OFF)
option(YAPB_NATIVE "Configure compiler for a native machine build" OFF)
option(YAPB_WINXP "Configure MSVC build for Windows XP compatible binary" OFF)
option(YAPB_NOSIMD "Disable all SIMD or NEON optimizations" OFF)
option(YAPB_STATIC_LINKENT "Use predefined entity link list" OFF)
option(YAPB_LTO "Enable link-time optimization" ON)
option(YAPB_SANITIZE "Enable sanitizers, e.g. address or address,undefined (dev builds)" "")
option(YAPB_WITH_TLS "Enable HTTPS support via vendored mbedtls" ON)
option(YAPB_BUILD_TESTS "Build test host (fake engine harness, native gcc/clang only, requires -DYAPB_64BIT=ON)" OFF)
option(YAPB_WITH_AMXX "Build AMX Mod X module (i386 only)" ON)

# classic headers only, no c++20 modules
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(ZIG_TARGET)
  set(IS_ZIG TRUE)
else()
  set(IS_ZIG FALSE)
endif()

set(_PROC "${CMAKE_SYSTEM_PROCESSOR}")
if(_PROC MATCHES "^(arm64|ARM64)$")
  set(_PROC "aarch64")
endif()

if(_PROC MATCHES "^(arm|ppc|ppc64|riscv)")
  set(CPU_NON_X86 TRUE)
else()
  set(CPU_NON_X86 FALSE)
endif()

if((CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND NOT MSVC)
  set(CLANG_OR_GCC TRUE)
else()
  set(CLANG_OR_GCC FALSE)
endif()

if(_PROC STREQUAL "aarch64")
  set(LIB_SUFFIX "_arm64")
elseif(_PROC MATCHES "^riscv")
  set(LIB_SUFFIX "_riscv64d")
elseif(YAPB_64BIT)
  set(LIB_SUFFIX "_amd64")
else()
  set(LIB_SUFFIX "")
endif()

set(TARGET_NAME "yapb${LIB_SUFFIX}")

include(YapbVersion)
include(YapbFlags)

set(SRC
  "src/analyze.cpp"
  "src/behavior.cpp"
  "src/buying.cpp"
  "src/chatlib.cpp"
  "src/clients.cpp"
  "src/combat.cpp"
  "src/config.cpp"
  "src/control.cpp"
  "src/debug.cpp"
  "src/engine.cpp"
  "src/fakeping.cpp"
  "src/funmode.cpp"
  "src/graph.cpp"
  "src/hooks.cpp"
  "src/linkage.cpp"
  "src/manager.cpp"
  "src/message.cpp"
  "src/module.cpp"
  "src/navigate.cpp"
  "src/planner.cpp"
  "src/practice.cpp"
  "src/radio.cpp"
  "src/rehlds.cpp"
  "src/sounds.cpp"
  "src/storage.cpp"
  "src/support.cpp"
  "src/tasks.cpp"
  "src/vision.cpp"
  "src/vistable.cpp"
  "src/weapons.cpp"
)

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  add_executable(${PROJECT_NAME} ${SRC})
else()
  add_library(${PROJECT_NAME} SHARED ${SRC})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  OUTPUT_NAME "${TARGET_NAME}"
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

yapb_configure_target(${PROJECT_NAME} STRICT DIST)

# undecorated stdcall export alias, main library only: amxx has no such symbol
if(WIN32 AND IS_ZIG AND NOT YAPB_64BIT AND NOT CPU_NON_X86)
  target_link_options(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/ext/ldscripts/yapb-x86.def)
endif()

# windows resource with version info, main library only
if(WIN32 AND GIT_FOUND)
  if(IS_ZIG)
    # drive `zig rc` directly: the triple never leaves our control here
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/yapb.res
      # pass -- so zig rc stops option parsing at the input file
      COMMAND zig rc -DVERSION_GENERATED -I${PROJECT_BINARY_DIR} /fo${CMAKE_CURRENT_BINARY_DIR}/yapb.res -- ${PROJECT_SOURCE_DIR}/vc/yapb.rc
      DEPENDS ${PROJECT_SOURCE_DIR}/vc/yapb.rc ${VERSION_H}
      COMMENT "Compiling Windows resource vc/yapb.rc"
      VERBATIM
    )
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yapb.res)
    # ninja with zig drops .res from the link, pass it explicitly
    target_link_options(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yapb.res)
  else()
    target_sources(${PROJECT_NAME} PRIVATE "vc/yapb.rc")
  endif()
endif()

# subprojects are C and don't inherit per-target -m32, pin their bitness
if(NOT YAPB_64BIT AND NOT IS_ZIG AND NOT ANDROID AND NOT _PROC STREQUAL "aarch64" AND NOT CPU_NON_X86)
  set(YSTL_32BIT ON)
else()
  set(YSTL_32BIT OFF)
endif()

# without mbedtls https requests cleanly report HttpOnly
if(YAPB_WITH_TLS AND NOT VITA AND NOT YAPB_WINXP AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_subdirectory(ext/ystl/ext/mbedtls)
  target_link_libraries(${PROJECT_NAME} PRIVATE ystl_mbedtls)
endif()

# no %n by design; winxp toolchain predates <stdbool.h>
if(NOT YAPB_WINXP)
  add_subdirectory(ext/ystl/ext/printf)
  target_link_libraries(${PROJECT_NAME} PRIVATE ystl_printf)
endif()

# amx mod x module (i386 linux/windows only), see ext/amxx
if(YAPB_WITH_AMXX AND NOT YAPB_64BIT AND NOT CPU_NON_X86 AND NOT _PROC STREQUAL "aarch64" AND NOT ANDROID)
  add_subdirectory(ext/amxx)
endif()

# legacy natives without a platform flags file (upstream android etc.)
if(ANDROID)
  target_link_libraries(${PROJECT_NAME} PRIVATE m dl log)
elseif(NOT VITA AND NOT WIN32 AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(${PROJECT_NAME} PRIVATE m dl pthread)
endif()

find_package(Python3 COMPONENTS Interpreter)

if(GIT_FOUND AND Python3_FOUND AND PROJECT_IS_TOP_LEVEL)
  add_custom_target(package
    COMMAND ${CMAKE_COMMAND} -E env "SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
      ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/gen_package.py" "${PROJECT_VERSION}.${BUILD_COUNT}"
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    COMMENT "Packaging release ${PROJECT_VERSION}.${BUILD_COUNT}"
    VERBATIM
  )
endif()

# test host (fake engine harness); native x64 only, ci presets never opt in
if(YAPB_BUILD_TESTS AND NOT IS_ZIG AND YAPB_64BIT AND YAPB_NATIVE
  AND EXISTS "${PROJECT_SOURCE_DIR}/tests/CMakeLists.txt")
  enable_testing()
  add_subdirectory(tests)

  yapb_configure_target(yapb_static STRICT DIST)

  if(NOT YAPB_WINXP)
    target_link_libraries(yapb_static PRIVATE ystl_printf)
  endif()
endif()

if(GAME_DIR AND SERVER_INSTALL_DIR)
  install(TARGETS ${PROJECT_NAME}
    DESTINATION "${GAME_DIR}/${SERVER_INSTALL_DIR}/"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

  if(MSVC)
    install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
      DESTINATION "${GAME_DIR}/${SERVER_INSTALL_DIR}/" OPTIONAL)
  endif()
endif()
