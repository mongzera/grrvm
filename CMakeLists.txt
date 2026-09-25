cmake_minimum_required(VERSION 3.15)

# Select Target Port (Default: posix)
set(PORT "posix" CACHE STRING "Target port platform (posix, pico)")
set_property(CACHE PORT PROPERTY STRINGS posix pico)

# ------------------------------------------------------------------------------
# Pico SDK Initialization (MUST happen before project() call)
# ------------------------------------------------------------------------------
if(PORT STREQUAL "pico")
    # Pull in Raspberry Pi Pico SDK (expects PICO_SDK_PATH in environment or set here)
    include(pico_sdk_import.cmake OPTIONAL)
    if(NOT COMMAND pico_sdk_init)
        message(FATAL_ERROR "Pico SDK not found. Set PICO_SDK_PATH environment variable or add pico_sdk_import.cmake.")
    endif()
endif()

# Define project name, version, and language
project(grrvm
    VERSION 0.1.0
    LANGUAGES C CXX ASM
)

if(PORT STREQUAL "pico")
    pico_sdk_init()
endif()

# Enforce C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# 1. Platform-Agnostic Source Files
# ------------------------------------------------------------------------------
file(GLOB VM_CORE_SOURCES CONFIGURE_DEPENDS
    "src/*.c"
    "src/core/*.c"
    "src/core/evaluation/*.c"
)

set(VM_PORT_SOURCES "")
set(VM_PORT_INCLUDES "")

# ------------------------------------------------------------------------------
# 2. Port-Specific Sources and Libraries
# ------------------------------------------------------------------------------
if(PORT STREQUAL "posix")
    file(GLOB VM_PORT_SOURCES CONFIGURE_DEPENDS "src/ports/posix/*.c")
    list(APPEND VM_PORT_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/posix")

elseif(PORT STREQUAL "pico")
    file(GLOB VM_PORT_SOURCES CONFIGURE_DEPENDS "src/ports/pico/*.c")
    list(APPEND VM_PORT_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/src/ports/pico")

else()
    message(FATAL_ERROR "Unsupported target PORT: '${PORT}'. Choose 'posix' or 'pico'.")
endif()

# Combine generic sources and port sources
set(VM_ALL_SOURCES ${VM_CORE_SOURCES} ${VM_PORT_SOURCES})

# ------------------------------------------------------------------------------
# 3. Target Creation & Linking
# ------------------------------------------------------------------------------
add_executable(grrvm ${VM_ALL_SOURCES})

# Include directories
target_include_directories(grrvm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hal
    ${VM_PORT_INCLUDES}
)

# Port-specific linking
if(PORT STREQUAL "posix")
    target_link_libraries(grrvm PRIVATE m) # C Math Library

elseif(PORT STREQUAL "pico")
    pico_enable_stdio_usb(grrvm 1)
    pico_enable_stdio_uart(grrvm 1)
    # Link against Pico standard library (UART, GPIO, stdio)
    target_link_libraries(grrvm PRIVATE pico_stdlib)

    # Generate .uf2 file for dragging and dropping onto the Pico drive
    pico_add_extra_outputs(grrvm)
endif()

# ------------------------------------------------------------------------------
# 4. Compiler Flags & Warnings
# ------------------------------------------------------------------------------
if(MSVC)
    target_compile_options(grrvm PRIVATE /W4)
else()
    target_compile_options(grrvm PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
    )
endif()

# ------------------------------------------------------------------------------
# 5. Project Feature Options (Custom Definitions)
# ------------------------------------------------------------------------------
# Accept either VM_DEBUG_ENABLE or ENABLE_VM_DEBUG passed from build flags
option(VM_DEBUG_ENABLE "Enable VM debug logging and tracing" ON)
option(ENABLE_VM_COLORS "Enable ANSI colors in logging" ON)

if(VM_DEBUG_ENABLE)
    target_compile_definitions(grrvm PRIVATE VM_DEBUG_ENABLE=1)
endif()

if(ENABLE_VM_COLORS)
    target_compile_definitions(grrvm PRIVATE VM_LOG_COLORS_ENABLE=1)
endif()
