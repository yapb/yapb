#
# YaPB test host: fake engine executable + fake cs gamedll
#
# SPDX-License-Identifier: Unlicense
#

# --- bot sources as a static lib (same TU list as the production .so) ---
set(YAPB_STATIC_SOURCES "")
foreach(s IN LISTS SRC)
    list(APPEND YAPB_STATIC_SOURCES "${PROJECT_SOURCE_DIR}/${s}")
endforeach()

add_library(yapb_static STATIC ${YAPB_STATIC_SOURCES})

target_compile_definitions(yapb_static PRIVATE YSTL_TESTS)

set_target_properties(yapb_static PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# production compile flags come from the root interface libs, linked below

# no process-wide dlsym detour in the harness
target_compile_definitions(yapb_static PRIVATE LINKENT_STATIC)

# --- fake cs gamedll (dlopen'ed by yapb_static at runtime, never linked) ---
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|ARM64|aarch64)$")
    set(FAKECS_SUFFIX "_arm64")
else()
    set(FAKECS_SUFFIX "_amd64")
endif()

if(WIN32)
    set(FAKECS_EXT ".dll")
elseif(APPLE)
    set(FAKECS_EXT ".dylib")
else()
    set(FAKECS_EXT ".so")
endif()

function(add_fake_cs_variant target)
    add_library(${target} SHARED fake_cs/fake_cs.cpp)

    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${target}"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        # keep them exported for dlsym
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )

    if(WIN32)
        set_target_properties(${target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/ext"
        "${PROJECT_SOURCE_DIR}/ext/ystl"
        "${PROJECT_SOURCE_DIR}/ext/linkage"
    )

    if(CLANG_OR_GCC)
        target_compile_options(${target} PRIVATE -Wall -Wextra -fPIC)
    endif()

    if(WIN32)
        target_link_libraries(${target} PRIVATE ws2_32)
    endif()
endfunction()

add_fake_cs_variant(fake_cs)
add_fake_cs_variant(fake_cs_modern)
add_fake_cs_variant(fake_cs_regame)
target_compile_definitions(fake_cs PRIVATE YSTL_TESTS)
target_compile_definitions(fake_cs_modern PRIVATE YSTL_TESTS)
target_compile_definitions(fake_cs_regame PRIVATE YSTL_TESTS)
target_compile_definitions(fake_cs_modern PRIVATE FAKECS_MODERN=1)
target_compile_definitions(fake_cs_regame PRIVATE FAKECS_MODERN=1 FAKECS_REGAME=1)

# stage each variant where Game::loadCSBinary looks
set(TEST_GAMEDIR "${CMAKE_CURRENT_BINARY_DIR}/gamedir")

function(stage_fake_cs target subdir)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${subdir}/cstrike/dlls"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}>
            "${CMAKE_CURRENT_BINARY_DIR}/${subdir}/cstrike/dlls/cs${FAKECS_SUFFIX}${FAKECS_EXT}"
        COMMENT "Staging ${target} into ${subdir}"
        VERBATIM
    )
endfunction()

stage_fake_cs(fake_cs gamedir)
stage_fake_cs(fake_cs_modern gamedir-modern)
stage_fake_cs(fake_cs_regame gamedir-regame)

# --- test host executable (fake engine + scenarios) ---
file(GLOB YAPB_TEST_SCENARIOS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.cpp")

add_executable(yapb_testhost
    host_main.cpp
    fake_engine.cpp
    ${YAPB_TEST_SCENARIOS}
)

set_target_properties(yapb_testhost PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
  # match the static CRT of yapb_static
  set_property(TARGET yapb_testhost PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(yapb_testhost PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)

target_include_directories(yapb_testhost SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/inc"
    "${PROJECT_SOURCE_DIR}/ext"
    "${PROJECT_SOURCE_DIR}/ext/ystl"
    "${PROJECT_SOURCE_DIR}/ext/linkage"
)

if(CLANG_OR_GCC)
    target_compile_options(yapb_testhost PRIVATE -Wall -Wextra)
endif()
target_compile_definitions(yapb_testhost PRIVATE
  "YAPB_TEST_GAMEDIR=\"${TEST_GAMEDIR}\""
  "YAPB_TEST_CSDLL=\"${TEST_GAMEDIR}/cstrike/dlls/cs${FAKECS_SUFFIX}${FAKECS_EXT}\""
  "YAPB_TEST_CSSUFFIX=\"cs${FAKECS_SUFFIX}${FAKECS_EXT}\""
  YSTL_TESTS
)

# the final link happens here, so repeat it for the exe
if(YAPB_SANITIZE AND CMAKE_BUILD_TYPE MATCHES "Debug")
  target_compile_options(yapb_testhost PRIVATE -fsanitize=${YAPB_SANITIZE} -fno-omit-frame-pointer -O1)
  target_link_options(yapb_testhost PRIVATE -fsanitize=${YAPB_SANITIZE} -fno-omit-frame-pointer -O1)
endif()

if(WIN32)
    target_link_libraries(yapb_testhost PRIVATE yapb_static user32 ws2_32)
else()
    target_link_libraries(yapb_testhost PRIVATE yapb_static dl m pthread)
endif()

add_dependencies(yapb_testhost fake_cs fake_cs_modern fake_cs_regame)

# one ctest entry per scenario: singletons have no reset,
# so scenarios must never share a process
set(DISCOVERED_TESTS "${CMAKE_CURRENT_BINARY_DIR}/discovered_tests.cmake")

# ctest errors on a missing include, so seed an empty one before the first build
file(WRITE "${DISCOVERED_TESTS}" "# pending yapb_testhost build\n")

add_custom_command(TARGET yapb_testhost POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_EXECUTABLE=$<TARGET_FILE:yapb_testhost>"
        "-DTEST_OUTPUT=${DISCOVERED_TESTS}"
        "-DTEST_GAMEDIR=${TEST_GAMEDIR}"
        "-DTEST_WINDOWS=${WIN32}"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/DiscoverTests.cmake"
    COMMENT "Discovering yapb test cases"
    VERBATIM
)

set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${DISCOVERED_TESTS}")

add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS yapb_testhost fake_cs
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running yapb test host"
    USES_TERMINAL
    VERBATIM
)
