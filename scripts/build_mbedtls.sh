#!/bin/bash

set -euo pipefail

# Set variables
MBEDTLS_REPO="https://github.com/Mbed-TLS/mbedtls.git"
MBEDTLS_VERSION="v3.6.1"             # The specific release tag of mbed TLS to use
MBEDTLS_DIR="./external/mbedtls"     # Directory outside your tracked repo for mbedtls
BUILD_DIR="$MBEDTLS_DIR/build"       # Build directory
EMSDK_PATH=$(dirname $(which emsdk)) # Automatically determine the directory of emsdk

# Set up Emscripten environment
source "$EMSDK_PATH/emsdk_env.sh"

# Determine the path to the Emscripten installation
EMSCRIPTEN=$(echo $EMSDK_PATH/upstream/emscripten)

# Create directory for mbed TLS if it doesn't exist
mkdir -p "$MBEDTLS_DIR"

# Clone mbed TLS if the directory is empty
if [ ! -d "$MBEDTLS_DIR/.git" ]; then
    echo "Cloning mbed TLS repository..."
    git clone "$MBEDTLS_REPO" "$MBEDTLS_DIR"
    cd "$MBEDTLS_DIR" || exit
    echo "Checking out mbed TLS version $MBEDTLS_VERSION..."
    git checkout "$MBEDTLS_VERSION"
    echo "Initializing and updating submodules..."
    git submodule update --init --recursive
    cd - || exit
else
    echo "mbed TLS repository already cloned. Ensuring version $MBEDTLS_VERSION is checked out..."
    cd "$MBEDTLS_DIR" || exit
    git fetch --tags
    git checkout "$MBEDTLS_VERSION"
    echo "Updating submodules..."
    git submodule update --init --recursive
    cd - || exit
fi

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Configure mbed TLS using Emscripten's CMake toolchain
echo "Configuring mbed TLS for WebAssembly..."
cmake -S "$MBEDTLS_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="${EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_TESTING=OFF \
    -DENABLE_PROGRAMS=OFF

# Build mbed TLS
echo "Building mbed TLS..."
cmake --build "$BUILD_DIR"

echo "mbed TLS build complete. The libraries are located in $BUILD_DIR/library"