#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Terminal Color Codes
COLOR_RESET="\033[0m"
COLOR_INFO="\033[1;36m"
COLOR_SUCCESS="\033[1;32m"
COLOR_ERROR="\033[1;31m"

# Ensure we execute relative to the script's directory
cd "$(dirname "$0")"

# --- Optional Arguments Handling ---
CLEAN_BUILD=false
BUILD_TYPE="Debug"
TARGET_PORT="posix" # Default port
OVERRIDE_VM_DEBUG="" # Unset by default to allow auto-setting based on BUILD_TYPE

for arg in "$@"; do
    case $arg in
        --clean|-c)
            CLEAN_BUILD=true
            ;;
        --release|-r)
            BUILD_TYPE="Release"
            ;;
        --pico|-p)
            TARGET_PORT="pico"
            ;;
        --posix)
            TARGET_PORT="posix"
            ;;
        --debug|-d)
            OVERRIDE_VM_DEBUG=true
            ;;
        --no-debug)
            OVERRIDE_VM_DEBUG=false
            ;;
        --help|-h)
            echo "Usage: ./build.sh [OPTIONS]"
            echo "Options:"
            echo "  -c, --clean     Clean the build directory before building"
            echo "  -r, --release   Build in Release mode (default: Debug)"
            echo "  -p, --pico      Build for Raspberry Pi Pico"
            echo "      --posix     Build for POSIX / Host (default)"
            echo "  -d, --debug     Explicitly enable VM_DEBUG_ENABLE macro"
            echo "      --no-debug  Explicitly disable VM_DEBUG_ENABLE macro"
            exit 0
            ;;
        *)
            ;;
    esac
done

# Set debug default based on BUILD_TYPE if not explicitly overridden
if [ -n "$OVERRIDE_VM_DEBUG" ]; then
    ENABLE_VM_DEBUG="$OVERRIDE_VM_DEBUG"
elif [ "$BUILD_TYPE" = "Debug" ]; then
    ENABLE_VM_DEBUG=true
else
    ENABLE_VM_DEBUG=false
fi

BUILD_DIR="build/${TARGET_PORT}"
EXECUTABLE="${BUILD_DIR}/grrvm"

# Translate boolean flag to CMake ON/OFF string
if [ "$ENABLE_VM_DEBUG" = true ]; then
    VM_DEBUG_FLAG="ON"
else
    VM_DEBUG_FLAG="OFF"
fi

# Clean port-specific build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${COLOR_INFO}[1/4] Cleaning build directory (${BUILD_DIR})...${COLOR_RESET}"
    rm -rf "${BUILD_DIR}"
fi

# 1. Configure
echo -e "${COLOR_INFO}[2/4] Configuring CMake for ${TARGET_PORT} (${BUILD_TYPE}, Debug Logs: ${VM_DEBUG_FLAG})...${COLOR_RESET}"
cmake -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DPORT="${TARGET_PORT}" \
      -DVM_DEBUG_ENABLE="${VM_DEBUG_FLAG}"

# 2. Build
echo -e "${COLOR_INFO}[3/4] Compiling source code...${COLOR_RESET}"
cmake --build "${BUILD_DIR}"

# 3. Check and Execute / Report Artifacts
if [ "${TARGET_PORT}" = "posix" ]; then
    if [ -f "$EXECUTABLE" ]; then
        echo -e "${COLOR_SUCCESS}[4/4] Build successful! Running ${EXECUTABLE}...${COLOR_RESET}"
        echo "=================================================="
        "$EXECUTABLE"
    else
        echo -e "${COLOR_ERROR}Error: Executable $EXECUTABLE was not found after build.${COLOR_RESET}"
        exit 1
    fi
elif [ "${TARGET_PORT}" = "pico" ]; then
    UF2_FILE="${BUILD_DIR}/grrvm.uf2"
    if [ -f "$UF2_FILE" ]; then
        echo -e "${COLOR_SUCCESS}[4/4] Pico build successful!${COLOR_RESET}"
        echo -e "${COLOR_INFO}UF2 Firmware File:${COLOR_RESET} ${UF2_FILE}"
        echo "Plug in your Pico in BOOTSEL mode and copy the .uf2 file over."
    else
        echo -e "${COLOR_ERROR}Error: Pico UF2 binary was not generated.${COLOR_RESET}"
        exit 1
    fi
fi
