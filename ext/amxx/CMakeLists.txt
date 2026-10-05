# amx mod x module (i386 linux/windows only), standalone from the core bot sources
add_library(yapb_amxx SHARED
  "yapb_amxx.cpp"
)

# linux amxx modules carry the _i386 suffix by convention
set(_amxx_name "yapb_amxx")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_amxx_name "yapb_amxx_i386")
endif()

set_target_properties(yapb_amxx PROPERTIES
  PREFIX ""
  OUTPUT_NAME "${_amxx_name}"
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

yapb_configure_target(yapb_amxx NOLTO)

# hide everything but the amxx entry points, like version.lds does for the core
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_options(yapb_amxx PRIVATE -Wl,--version-script=${PROJECT_SOURCE_DIR}/ext/ldscripts/amxx.lds)
endif()

# windows resource with version info, mirrors the main library
if(WIN32 AND GIT_FOUND)
  if(IS_ZIG)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/yapb_amxx.res
      COMMAND zig rc -DVERSION_GENERATED -I${PROJECT_BINARY_DIR} /fo${CMAKE_CURRENT_BINARY_DIR}/yapb_amxx.res -- ${CMAKE_CURRENT_SOURCE_DIR}/yapb_amxx.rc
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/yapb_amxx.rc ${VERSION_H}
      COMMENT "Compiling Windows resource ext/amxx/yapb_amxx.rc"
      VERBATIM
    )
    target_sources(yapb_amxx PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yapb_amxx.res)
    # ninja with zig drops .res from the link, pass it explicitly
    target_link_options(yapb_amxx PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yapb_amxx.res)
  else()
    target_sources(yapb_amxx PRIVATE "yapb_amxx.rc")
  endif()
endif()

if(CLANG_OR_GCC)
  target_compile_options(yapb_amxx PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(NOT YAPB_WINXP)
  target_link_libraries(yapb_amxx PRIVATE ystl_printf)
endif()
