#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Target executable name
EXECUTABLE="./build/grrvm"

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

for arg in "$@"; do
    case $arg in
        --clean|-c)
            CLEAN_BUILD=true
            shift
            ;;
        --release|-r)
            BUILD_TYPE="Release"
            shift
            ;;
        *)
            ;;
    esac
done

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${COLOR_INFO}[1/4] Cleaning build directory...${COLOR_RESET}"
    rm -rf build
fi

# 1. Configure
echo -e "${COLOR_INFO}[2/4] Configuring CMake (${BUILD_TYPE})...${COLOR_RESET}"
cmake -B build -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

# 2. Build
echo -e "${COLOR_INFO}[3/4] Compiling source code...${COLOR_RESET}"
cmake --build build

# 3. Check and Execute
if [ -f "$EXECUTABLE" ]; then
    echo -e "${COLOR_SUCCESS}[4/4] Build successful! Running ${EXECUTABLE}...${COLOR_RESET}"
    echo "=================================================="
    "$EXECUTABLE"
else
    echo -e "${COLOR_ERROR}Error: Executable $EXECUTABLE was not found after build.${COLOR_RESET}"
    exit 1
fi
