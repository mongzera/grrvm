cmake_minimum_required(VERSION 3.15)

# Define project name, version, and language
project(grrvm
    VERSION 0.1.0
    LANGUAGES C
)

# Enforce C11 standard (or C99 if targeting older MCUs)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile_commands.json (useful for clangd, VS Code, and IDE auto-complete)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# 1. Source and Header Definitions
# ------------------------------------------------------------------------------
file(GLOB VM_SOURCES CONFIGURE_DEPENDS
    "src/*.c"
    "src/core/*.c"
    "src/core/evaluation/*.c"
)

# ------------------------------------------------------------------------------
# 2. Target Creation
# ------------------------------------------------------------------------------
add_executable(grrvm ${VM_SOURCES})
target_link_libraries(grrvm m) # math library

# Specify include directories (PUBLIC/PRIVATE scope)
target_include_directories(grrvm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hal
)

# ------------------------------------------------------------------------------
# 3. Compiler Flags & Warnings
# ------------------------------------------------------------------------------
# Strict warning flags help catch VM struct alignment and casting issues early
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
# 4. Project Feature Options (Custom Definitions)
# ------------------------------------------------------------------------------
# Toggle VM debugging logs on/off
option(ENABLE_VM_DEBUG "Enable VM debug logging and tracing" ON)
option(ENABLE_VM_COLORS "Enable ANSI colors in logging" ON)

if(ENABLE_VM_DEBUG)
    target_compile_definitions(grrvm PRIVATE VM_DEBUG_ENABLE)
endif()

if(ENABLE_VM_COLORS)
    target_compile_definitions(grrvm PRIVATE VM_LOG_COLORS_ENABLE)
endif()
