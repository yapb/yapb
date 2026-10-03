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

yapb_configure_target(yapb_amxx)

if(CLANG_OR_GCC)
  target_compile_options(yapb_amxx PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(NOT YAPB_WINXP)
  target_link_libraries(yapb_amxx PRIVATE ystl_printf)
endif()
