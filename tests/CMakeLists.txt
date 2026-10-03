#
# YaPB test suite (fake engine harness)
#
# SPDX-License-Identifier: Unlicense
#
# Native gcc/clang/msvc, x64 only. Requires 64-bit (string_t layout) and
# skips TLS (vendored mbedtls is unnecessary weight for the harness).
#

if(NOT YAPB_64BIT)
  message(FATAL_ERROR "YAPB_BUILD_TESTS requires -DYAPB_64BIT=ON (32-bit string_t layout is unsupported by the harness)")
endif()

add_subdirectory(host)
