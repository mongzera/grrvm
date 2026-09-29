#!/usr/bin/env bash

set -e

# ==============================================================================
# Colors
# ==============================================================================

COLOR_RESET="\033[0m"
COLOR_INFO="\033[1;36m"
COLOR_SUCCESS="\033[1;32m"
COLOR_ERROR="\033[1;31m"


# ==============================================================================
# Project Root
# ==============================================================================

cd "$(dirname "$0")"


# ==============================================================================
# Defaults
# ==============================================================================

CLEAN_BUILD=false
BUILD_TYPE="Debug"
TARGET_PORT="posix"
OVERRIDE_VM_DEBUG=""
RUN_TESTS=true


# ==============================================================================
# Arguments
# ==============================================================================

for arg in "$@"; do

    case "$arg" in

        --clean|-c)
            CLEAN_BUILD=true
            ;;

        --release|-r)
            BUILD_TYPE="Release"
            ;;

        --debug|-d)
            OVERRIDE_VM_DEBUG=true
            ;;

        --no-debug)
            OVERRIDE_VM_DEBUG=false
            ;;

        --pico|-p)
            TARGET_PORT="pico"
            RUN_TESTS=false
            ;;

        --posix)
            TARGET_PORT="posix"
            ;;

        --test|-t)
            RUN_TESTS=true
            ;;

        --no-test)
            RUN_TESTS=false
            ;;

        --help|-h)

            cat << EOF
Usage:
    ./build.sh [OPTIONS]

Target:
    --posix          Build POSIX target (default)
    --pico, -p       Build Raspberry Pi Pico target

Build:
    --clean, -c      Clean selected target build directory
    --release, -r    Release build
    --debug, -d      Enable VM debug logging
    --no-debug       Disable VM debug logging

Tests:
    --test, -t       Run tests
    --no-test        Skip tests

Examples:
    ./build.sh
    ./build.sh --clean
    ./build.sh --release
    ./build.sh --pico
    ./build.sh --pico --clean
EOF

            exit 0
            ;;

        *)
            echo -e "${COLOR_ERROR}Unknown option: ${arg}${COLOR_RESET}"
            exit 1
            ;;

    esac

done


# ==============================================================================
# Debug Configuration
# ==============================================================================

if [ -n "$OVERRIDE_VM_DEBUG" ]; then
    ENABLE_VM_DEBUG="$OVERRIDE_VM_DEBUG"
elif [ "$BUILD_TYPE" = "Debug" ]; then
    ENABLE_VM_DEBUG=true
else
    ENABLE_VM_DEBUG=false
fi

if [ "$ENABLE_VM_DEBUG" = true ]; then
    VM_DEBUG_FLAG="ON"
else
    VM_DEBUG_FLAG="OFF"
fi


# ==============================================================================
# Build Directory
#
# Each platform has a completely independent CMake build tree.
# ==============================================================================

BUILD_DIR="build/${TARGET_PORT}"


# ==============================================================================
# Clean
# ==============================================================================

if [ "$CLEAN_BUILD" = true ]; then

    echo -e "${COLOR_INFO}[1/4] Cleaning ${BUILD_DIR}...${COLOR_RESET}"

    rm -rf "${BUILD_DIR}"

fi


# ==============================================================================
# Configure
# ==============================================================================

echo -e "${COLOR_INFO}[2/4] Configuring ${TARGET_PORT}...${COLOR_RESET}"

cmake \
    -S . \
    -B "${BUILD_DIR}" \
    -DPORT="${TARGET_PORT}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DVM_DEBUG_ENABLE="${VM_DEBUG_FLAG}"


# ==============================================================================
# Build
# ==============================================================================

echo -e "${COLOR_INFO}[3/4] Building ${TARGET_PORT}...${COLOR_RESET}"

cmake \
    --build "${BUILD_DIR}" \
    --parallel


# ==============================================================================
# POSIX
# ==============================================================================

if [ "$TARGET_PORT" = "posix" ]; then

    if [ "$RUN_TESTS" = true ]; then

        echo -e "${COLOR_INFO}Running POSIX tests...${COLOR_RESET}"
        echo "=================================================="

        (
            cd "${BUILD_DIR}"
            ctest --output-on-failure
        )

        echo "=================================================="

    fi

    EXECUTABLE="${BUILD_DIR}/grrvm"

    if [ ! -f "$EXECUTABLE" ]; then

        echo -e \
            "${COLOR_ERROR}Error: ${EXECUTABLE} was not generated.${COLOR_RESET}"

        exit 1

    fi

    echo -e "${COLOR_SUCCESS}POSIX build successful.${COLOR_RESET}"

    echo "=================================================="

    "${EXECUTABLE}"

    echo "=================================================="


# ==============================================================================
# Pico
# ==============================================================================

elif [ "$TARGET_PORT" = "pico" ]; then

    UF2_FILE="${BUILD_DIR}/grrvm.uf2"

    if [ ! -f "$UF2_FILE" ]; then

        echo -e \
            "${COLOR_ERROR}Error: ${UF2_FILE} was not generated.${COLOR_RESET}"

        exit 1

    fi

    echo -e "${COLOR_SUCCESS}Pico build successful!${COLOR_RESET}"
    echo -e "${COLOR_INFO}UF2: ${UF2_FILE}${COLOR_RESET}"

    echo
    echo "Plug the Pico in BOOTSEL mode and copy the UF2 file."

fi
