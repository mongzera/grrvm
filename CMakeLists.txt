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
# ==============================================================================

if(PORT STREQUAL "posix")

    enable_testing()

    file(GLOB COMMON_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/common/*.c"
    )

    file(GLOB POSIX_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/posix/*.c"
    )

    foreach(TEST_SRC IN LISTS
        COMMON_TEST_SOURCES
        POSIX_TEST_SOURCES
    )

        get_filename_component(TEST_NAME
            "${TEST_SRC}"
            NAME_WE
        )

        add_executable(
            "${TEST_NAME}"
            "${TEST_SRC}"
        )

        target_include_directories(
            "${TEST_NAME}"
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/common"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/posix"
        )

        target_link_libraries(
            "${TEST_NAME}"
            PRIVATE
                grrvm_core
        )

        add_test(
            NAME "${TEST_NAME}"
            COMMAND "${TEST_NAME}"
        )

    endforeach()

endif()


# ==============================================================================
# Pico Tests
#
# ONLY:
#
#   tests/common/**
#   tests/pico/**
# ==============================================================================

if(PORT STREQUAL "pico")

    file(GLOB COMMON_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/common/*.c"
    )

    file(GLOB PICO_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/pico/*.c"
    )

    foreach(TEST_SRC IN LISTS
        COMMON_TEST_SOURCES
        PICO_TEST_SOURCES
    )

        get_filename_component(TEST_NAME
            "${TEST_SRC}"
            NAME_WE
        )

        add_executable(
            "${TEST_NAME}"
            "${TEST_SRC}"
        )

        target_include_directories(
            "${TEST_NAME}"
            PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/common"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/pico"
        )

        target_link_libraries(
            "${TEST_NAME}"
            PRIVATE
                grrvm_core
                pico_stdlib
        )

        pico_enable_stdio_usb("${TEST_NAME}" 1)
        pico_enable_stdio_uart("${TEST_NAME}" 1)

        pico_add_extra_outputs("${TEST_NAME}")

    endforeach()

endif()
