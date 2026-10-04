#
# YaPB compiler/linker flags as interface libraries.
#
# SPDX-License-Identifier: Unlicense
#

add_library(yapb_flags INTERFACE)
add_library(yapb_strict INTERFACE)
add_library(yapb_dist INTERFACE)

if(GIT_FOUND)
  list(APPEND _cflags -DVERSION_GENERATED)
endif()

if(YAPB_NATIVE)
  list(APPEND _cflags -DYSTL_NATIVE_BUILD)
endif()

if(YAPB_NOSIMD)
  list(APPEND _cflags -DYSTL_DISABLE_SIMD)
endif()

if(NOT YAPB_WINXP)
  list(APPEND _cflags -DYSTL_USE_VENDORED_PRINTF)
endif()

target_include_directories(yapb_flags INTERFACE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_include_directories(yapb_flags SYSTEM INTERFACE
  "${PROJECT_SOURCE_DIR}/inc"
  "${PROJECT_SOURCE_DIR}/ext"
  "${PROJECT_SOURCE_DIR}/ext/ystl"
  "${PROJECT_SOURCE_DIR}/ext/linkage"
)

# android parent owns the toolchain flags, so only defines and includes apply
if(CLANG_OR_GCC AND NOT ANDROID)
  list(APPEND _cflags -pipe -fno-threadsafe-statics -pthread)
  list(APPEND _cflags -fno-exceptions -fno-rtti)

  if(NOT VITA AND NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    list(APPEND _strict_cflags -Wall -Wextra -Wpedantic -Werror)
  endif()

  list(APPEND _cflags -Wno-date-time -D_FILE_OFFSET_BITS=64)

  if(NOT YAPB_NATIVE AND NOT CPU_NON_X86 AND NOT IS_ZIG)
    list(APPEND _cflags -mtune=generic)
  endif()

  if(_PROC STREQUAL "aarch64")
    if(NOT IS_ZIG)
      list(APPEND _cflags -march=armv8-a+fp+simd)
    endif()
  elseif(NOT CPU_NON_X86)
    if(NOT YAPB_NOSIMD)
      list(APPEND _cflags -msse -msse2 -msse3 -mssse3 -mfpmath=sse)
    endif()

    if(YAPB_NATIVE)
      list(APPEND _cflags -march=native)
    elseif(YAPB_64BIT)
      if(NOT IS_ZIG)
        list(APPEND _cflags -march=x86-64)
      endif()
    else()
      list(APPEND _cflags -march=i686)
    endif()
  endif()

  if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    if(IS_ZIG)
      list(APPEND _cflags -g0)
    endif()

    list(APPEND _cflags -funroll-loops -fomit-frame-pointer -fno-stack-protector -fvisibility=hidden -fvisibility-inlines-hidden -fno-math-errno)
    list(APPEND _cflags -DNDEBUG)
  else()
    list(APPEND _cflags -g3 -ggdb -DDEBUG)

    if(YAPB_SANITIZE)
      list(APPEND _cflags -fsanitize=${YAPB_SANITIZE} -fno-omit-frame-pointer -O1)

      if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND _cflags -Wno-error=restrict)
        list(APPEND _cflags -Wno-error=null-dereference)
      endif()
    endif()
  endif()

  if(NOT APPLE)
    if(IS_ZIG OR CMAKE_CROSSCOMPILING OR (WIN32 AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
      list(APPEND _ldflags -static-libgcc)
    endif()
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT CMAKE_BUILD_TYPE MATCHES "Debug")
      if(NOT _PROC STREQUAL "aarch64" AND NOT CPU_NON_X86)
        if(NOT YAPB_STATIC_LINKENT)
          list(APPEND _cflags -fdata-sections -ffunction-sections -fcf-protection=none)
          if(IS_ZIG OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR NOT YAPB_LTO)
            list(APPEND _cflags -Wa,--noexecstack)
          endif()
          list(APPEND _dist_ldflags -Wl,--version-script=${PROJECT_SOURCE_DIR}/ext/ldscripts/version.lds)
          list(APPEND _ldflags -Wl,--gc-sections -Wl,-z,noexecstack)
        else()
          list(APPEND _cflags -DLINKENT_STATIC)
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
          list(APPEND _cflags -fgraphite-identity -floop-nest-optimize)
          list(APPEND _ldflags -fgraphite-identity -floop-nest-optimize)
        endif()
      endif()

      if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11.0")
        list(APPEND _ldflags -flto-partition=none)
      endif()
    endif()

    list(APPEND _ldflags -pthread -lm -ldl -lpthread)
    list(APPEND _ldflags -Wl,--as-needed -Wl,--no-undefined)
    list(APPEND _dist_ldflags -Wl,-soname,yapb${LIB_SUFFIX}.so)
    list(APPEND _strict_ldflags -Werror)
  elseif(WIN32)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0")
      list(APPEND _ldflags -Xlinker --script -Xlinker ${PROJECT_SOURCE_DIR}/ext/ldscripts/i386pe.lds)
    endif()

    if(CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT IS_ZIG)
      list(APPEND _ldflags -Wl,/MACHINE:X86)
      list(APPEND _cflags -Wl,/MACHINE:X86)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT IS_ZIG))
      list(APPEND _ldflags -Wl,--kill-at)
    endif()



    if(IS_ZIG)
      list(APPEND _ldflags -Wl,--allow-shlib-undefined)
    endif()

    list(APPEND _strict_ldflags -Werror)
    list(APPEND _ldflags -luser32 -lws2_32 -lkernel32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32)
  elseif(APPLE)
    if(IS_ZIG)
      list(APPEND _cflags -mmacosx-version-min=11.0)
      list(APPEND _ldflags -mmacosx-version-min=11.0 -Wl,-dead_strip)
      list(APPEND _ldflags -Wl,--as-needed -Wl,--no-undefined)
      list(APPEND _dist_ldflags -Wl,-soname,yapb${LIB_SUFFIX}.dylib)
    else()
      list(APPEND _cflags -mmacosx-version-min=10.9)
      list(APPEND _ldflags -lstdc++ -mmacosx-version-min=10.9)
    endif()
  endif()

  if(NOT YAPB_64BIT AND NOT _PROC STREQUAL "aarch64" AND NOT CPU_NON_X86)
    list(APPEND _cflags -m32)
    list(APPEND _ldflags -m32)

    if(YAPB_SANITIZE)
      list(APPEND _cflags -fPIC)
      list(APPEND _ldflags -fPIC)
      set(_yapb_no_pic OFF)
    else()
      set(_yapb_no_pic ON)
    endif()
  else()
    list(APPEND _cflags -fPIC)
    list(APPEND _ldflags -fPIC)
    set(_yapb_no_pic OFF)
  endif()

  if(IS_ZIG)
    list(APPEND _ldflags -nostdlib++)
  endif()

  list(APPEND _strict_cflags
    -Wcast-align -Wdouble-promotion -Wredundant-decls
    -Wcast-qual -Wctor-dtor-privacy -Wold-style-cast
    -Woverloaded-virtual -Wsign-promo -Wzero-as-null-pointer-constant
    -Wextra-semi -Wconversion -Wsign-conversion -Wnull-dereference -Wfloat-equal
    -Wshadow -Wformat=2 -Wundef -Wnon-virtual-dtor -Wimplicit-fallthrough
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND _strict_cflags -Wrange-loop-construct -Wsuggest-override)
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND _strict_cflags -Wlogical-op -Wduplicated-cond -Wduplicated-branches -Weffc++ -Wsuggest-override)
  endif()

  # executes via the compiler driver, so compile flags belong on the link line too
  list(APPEND _ldflags ${_cflags})
elseif(WIN32 AND MSVC)
  set(_yapb_msvc_static_crt ON)
  list(APPEND _strict_cflags /WX)
  list(APPEND _ldflags /DEBUG)

  if(YAPB_WINXP)
    list(APPEND _cflags /TP /D_WIN32_WINNT=0x0501 /D_USING_V110_SDK71_ /DYSTL_HAS_WINXP_SUPPORT)
    list(APPEND _ldflags /SUBSYSTEM:WINDOWS,5.01)
  endif()

  if(NOT YAPB_64BIT AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND _ldflags /MACHINE:X86)
  endif()

  list(APPEND _cflags "$<$<NOT:$<CONFIG:Debug>>:/Zc:threadSafeInit-;/GS-;/Ob2;/Oy;/Oi;/Ot;/fp:precise;/GF;/Gw;/Zi;/guard:ehcont-;/guard:cf->")
  list(APPEND _ldflags "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF,ICF;/GUARD:NO;delayimp.lib;/DELAYLOAD:user32.dll;/DELAYLOAD:ws2_32.dll>")

  if(NOT YAPB_64BIT)
    list(APPEND _cflags "$<$<NOT:$<CONFIG:Debug>>:/arch:SSE2>")
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND _cflags "$<$<NOT:$<CONFIG:Debug>>:/GL>")
    list(APPEND _ldflags "$<$<NOT:$<CONFIG:Debug>>:/LTCG>")
  endif()

  target_link_libraries(yapb_flags INTERFACE user32 ws2_32)
endif()

target_compile_options(yapb_flags INTERFACE ${_cflags})
target_link_options(yapb_flags INTERFACE ${_ldflags})
target_compile_options(yapb_strict INTERFACE ${_strict_cflags})
target_link_options(yapb_strict INTERFACE ${_strict_ldflags})
target_link_options(yapb_dist INTERFACE ${_dist_ldflags})

# usage: yapb_configure_target(<target> [STRICT] [DIST] [NOLTO])
function(yapb_configure_target _tgt)
  cmake_parse_arguments(_YC "STRICT;DIST;NOLTO" "" "" ${ARGN})

  set_target_properties(${_tgt} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )

  if(_yapb_no_pic)
    set_target_properties(${_tgt} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
  endif()

  if(_yapb_msvc_static_crt)
    set_property(TARGET ${_tgt} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()

  target_link_libraries(${_tgt} PRIVATE yapb_flags)

  if(_YC_STRICT)
    target_link_libraries(${_tgt} PRIVATE yapb_strict)
  endif()

  if(_YC_DIST)
    target_link_libraries(${_tgt} PRIVATE yapb_dist)
  endif()

  # per-target so single-tu modules can opt out: lto codegen emits libcalls
  # missing from 32-bit libc (wmem, long-double math) breaking the dll load
  if(YAPB_LTO AND NOT _YC_NOLTO AND NOT CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT (APPLE AND IS_ZIG))
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set(_lto_flag -flto=auto)
    else()
      set(_lto_flag -flto)
    endif()

    target_compile_options(${_tgt} PRIVATE ${_lto_flag})
    target_link_options(${_tgt} PRIVATE ${_lto_flag})
  endif()
endfunction()
