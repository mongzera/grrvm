cmake_minimum_required(VERSION 3.15)

# ==============================================================================
# Target Selection
# ==============================================================================

set(PORT "posix" CACHE STRING "Target platform")
set_property(CACHE PORT PROPERTY STRINGS posix pico)

if(NOT PORT STREQUAL "posix" AND NOT PORT STREQUAL "pico")
    message(FATAL_ERROR
        "Unsupported PORT='${PORT}'. Choose 'posix' or 'pico'."
    )
endif()


# ==============================================================================
# Pico SDK
#
# Only initialize the Pico SDK when building the Pico target.
# ==============================================================================

if(PORT STREQUAL "pico")

    include(pico_sdk_import.cmake OPTIONAL)

    if(NOT COMMAND pico_sdk_init)
        message(FATAL_ERROR
            "Pico SDK not found. "
            "Set PICO_SDK_PATH or provide pico_sdk_import.cmake."
        )
    endif()

endif()


# ==============================================================================
# Project
# ==============================================================================

project(grrvm
    VERSION 0.1.0
    LANGUAGES C CXX ASM
)

if(PORT STREQUAL "pico")
    pico_sdk_init()
endif()


# ==============================================================================
# Compiler Configuration
# ==============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# ==============================================================================
# Shared Sources
#
# These are common to every platform.
#
#   src/core/**
#   src/*.c
#
# main.c is excluded because it belongs to the executable.
# ==============================================================================

file(GLOB_RECURSE GRRVM_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/debug/*.c"
)

file(GLOB GRRVM_SHARED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

list(FILTER GRRVM_SHARED_SOURCES
    EXCLUDE REGEX ".*/main\\.c$"
)


# ==============================================================================
# Selected Port
#
# Only ONE of these directories is ever included in a build.
#
# POSIX:
#   src/ports/posix/**
#
# Pico:
#   src/ports/pico/**
#
# Both contain their own:
#
#   grr_port_config.h
# ==============================================================================

if(PORT STREQUAL "posix")

    set(GRRVM_PORT_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/posix"
    )

elseif(PORT STREQUAL "pico")

    set(GRRVM_PORT_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/pico"
    )

endif()

file(GLOB_RECURSE GRRVM_PORT_SOURCES CONFIGURE_DEPENDS
    "${GRRVM_PORT_DIR}/*.c"
)


# ==============================================================================
# Core Library
#
# Contains:
#
#   src/core/**
#   shared src/*.c
#   selected src/ports/<PORT>/**
#
# The selected port directory is PUBLIC because:
#
#   include/grrvm/default_config.h
#
# includes:
#
#   grr_port_config.h
#
# which lives inside the selected port directory.
# ==============================================================================

add_library(grrvm_core STATIC
    ${GRRVM_CORE_SOURCES}
    ${GRRVM_SHARED_SOURCES}
    ${GRRVM_PORT_SOURCES}
)

target_include_directories(grrvm_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${GRRVM_PORT_DIR}"
)


# ==============================================================================
# Compiler Warnings
# ==============================================================================

if(MSVC)

    target_compile_options(grrvm_core PRIVATE
        /W4
    )

else()

    target_compile_options(grrvm_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
    )

endif()


# ==============================================================================
# Feature Options
# ==============================================================================

option(
    GRRVM_BUILD_TESTS
    "Compile tests into the GRRVM Pico firmware"
    OFF
)

option(
    VM_DEBUG_ENABLE
    "Enable VM debug logging and tracing"
    ON
)

option(
    ENABLE_VM_COLORS
    "Enable ANSI colors in logging"
    ON
)

if(VM_DEBUG_ENABLE)

    target_compile_definitions(grrvm_core
        PUBLIC
            VM_DEBUG_ENABLE=1
    )

endif()

if(ENABLE_VM_COLORS)

    target_compile_definitions(grrvm_core
        PUBLIC
            VM_LOG_COLORS_ENABLE=1
    )

endif()

# Public so the port's init.c can run the test suite before the runtime
# starts, while the tests themselves are only linked into the executable.
if(GRRVM_BUILD_TESTS)

    target_compile_definitions(grrvm_core
        PUBLIC
            GRRVM_BUILD_TESTS=1
    )

endif()


# ==============================================================================
# Main Executable
# ==============================================================================

add_executable(grrvm
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
)

target_link_libraries(grrvm
    PRIVATE
        grrvm_core
)


# ==============================================================================
# POSIX Configuration
# ==============================================================================

if(PORT STREQUAL "posix")

    target_link_libraries(grrvm_core
        PRIVATE
            m
    )

endif()


# ==============================================================================
# Pico Configuration
#
# The Pico SDK is NOT part of src/ports/pico.
#
# src/ports/pico/ is YOUR platform implementation.
#
# pico_stdlib is the EXTERNAL Raspberry Pi Pico SDK dependency.
# ==============================================================================

if(PORT STREQUAL "pico")

    # grrvm_core contains src/ports/pico/*.c.
    #
    # Those source files may include:
    #
    #     #include "pico/stdlib.h"
    #
    # Therefore grrvm_core needs the Pico SDK's usage requirements
    # while compiling its Pico-specific sources.
    target_link_libraries(grrvm_core
        PUBLIC
            pico_stdlib
            # Flash-safe runtime program storage (program_store.c):
            #   hardware_flash - flash_range_erase/program
            #   pico_flash     - flash_safe_execute
            #   hardware_sync  - interrupt control used by pico_flash
            hardware_flash
            pico_flash
            hardware_sync
            pico_stdio_usb

    )

    # The final executable also links against the Pico SDK.
    target_link_libraries(grrvm
        PRIVATE
            pico_stdlib
    )

    pico_enable_stdio_usb(grrvm 1)
    pico_enable_stdio_uart(grrvm 1)

    target_link_options(grrvm
        PRIVATE
            "-Wl,--print-memory-usage"
    )

    pico_add_extra_outputs(grrvm)

endif()


# ==============================================================================
# POSIX Tests
#
# ONLY:
#
#   tests/common/**
#   tests/posix/**
#
# Tests are linked INTO the grrvm executable (tests/run_tests.c) so they
# execute in-process via `grrvm --test`; no standalone test executables.
# ==============================================================================

if(PORT STREQUAL "posix")

    enable_testing()

    if(GRRVM_BUILD_TESTS)

        file(GLOB COMMON_TEST_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/common/*.c"
        )

        file(GLOB POSIX_TEST_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/posix/*.c"
        )

        target_sources(grrvm
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.c"
                ${COMMON_TEST_SOURCES}
                ${POSIX_TEST_SOURCES}
        )

        target_include_directories(grrvm
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/common"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/posix"
        )

        target_compile_definitions(grrvm
            PRIVATE
                GRRVM_TEST_POSIX=1
                # POSIX tests may read checked-in .grr.o artifacts from disk.
                GRRVM_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/guest_programs"
        )

        add_test(
            NAME grrvm_tests
            COMMAND grrvm --test
        )

    endif()

endif()



# ==============================================================================
# Pico Tests
#
# Tests are compiled directly into grrvm.uf2.
# No separate test firmware executables are generated.
# ==============================================================================

if(PORT STREQUAL "pico" AND GRRVM_BUILD_TESTS)

    file(GLOB PICO_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/common/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/pico/*.c"
    )

    target_sources(grrvm
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.c"
            ${PICO_TEST_SOURCES}
    )

    target_include_directories(grrvm
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/common"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/pico"
    )

    target_compile_definitions(grrvm
        PRIVATE
            GRRVM_TEST_PICO=1
            # pico_malloc panics on a failed malloc, which turns any
            # out-of-memory during the boot checks into a hard reset. The
            # allocation sites already handle NULL, so let them report the
            # failure instead of panicking in the test firmware.
            PICO_MALLOC_PANIC=0
    )

endif()
